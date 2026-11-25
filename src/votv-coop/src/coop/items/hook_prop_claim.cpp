// coop/items/hook_prop_claim.cpp -- the hook lane feeding the driven-prop channel: on the host,
// every prop a hook on this machine is tied to is claimed for as long as the tie holds.
//
// The tie is a PhysX constraint, and coop/items/hook_constraint puts every one of them on the host:
// the host's own hook in its owner phase, every adopted anchored hook, the host's mirror of a
// client's hook once it has bitten, and the hooks the world itself holds -- the save's anchored
// ones the load path re-ties and the level-placed variant that ties itself at begin-play, which no
// table of the lane names and the shared scan pass finds. All four sets are read here every pass.
// The same sets answer FindHookByKey on either role: a pickup names its hook by save key (coop/items/hook_retrieve).
// Game thread; the claims host only.

#include "hook_sync_detail.h"
#include "coop/items/hook_retrieve.h"

#include "coop/element/object_scan_hub.h"   // the world's own hooks, found by the shared pass
#include "coop/items/hook_sync.h"           // PlayerHookHolders
#include "coop/player/players_registry.h"   // kMaxPeers
#include "coop/props/prop_drive_host.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/actors/hook.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"  // Generation: the scan's stamp
#include "ue_wrap/world/keyed_objects.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace coop::hook_sync::detail {
namespace {

namespace H  = ue_wrap::hook;
namespace PR = ue_wrap::prop;
namespace R  = ue_wrap::reflection;

// The props seen tied on the last pass, held through slot-validated references: a pointer seen
// before is answered by its own reference and never probed bare, the owner poll's rule for a
// field the game can leave dangling. Only a pointer seen for the first time reaches the bare
// liveness probe, once. Both vectors are kept so the 20 Hz pass allocates nothing.
std::vector<ue_wrap::CachedObjRef> g_tied;
std::vector<ue_wrap::CachedObjRef> g_next;
// Beside each entry of g_tied and g_next, in step with it: the PLAYER slots whose hook pulls that prop, as a bit mask
// (bit 0 the host). An anchored hook and a level hook are a tether, no player's hold, and set nothing.
std::vector<uint8_t> g_tiedHolders;
std::vector<uint8_t> g_nextHolders;
std::unordered_map<void*,coop::prop_drive_host::ClaimLease> g_claimLeases;
uint64_t g_claimEpoch=0;
bool g_claimTicking=false;

const ue_wrap::CachedObjRef* Known(void* p) {
    for (const auto& r : g_tied)
        if (r.Raw() == p) return &r;
    return nullptr;
}

// The index of `p` in g_next, or -1.
int NextIndex(void* p) {
    for (size_t i = 0; i < g_next.size(); ++i)
        if (g_next[i].Raw() == p) return static_cast<int>(i);
    return -1;
}

bool InNext(void* p) { return NextIndex(p) >= 0; }

void PushNext(const ue_wrap::CachedObjRef& r, uint8_t holders) {
    g_next.push_back(r);
    g_nextHolders.push_back(holders);
}

// The hooks the WORLD holds on this machine that no table of the lane names: the save's anchored
// hooks, re-tied by the game's own load path at world start, and the level-placed variant, which
// ties itself at begin-play. Their constraints are as real as an adopted hook's, and a prop they
// hold needs the same stream. Found by the shared scan pass, stamped with the world it walked,
// and read only while that stamp is current; liveness by the index the pass captured.
struct WorldHook { void* actor; int32_t idx; };
std::vector<WorldHook> g_world;         // the last completed pass
std::vector<WorldHook> g_worldScratch;  // the pass in progress
uint32_t               g_worldGen = 0;

bool HubIsInstance(void* obj) { return H::IsHookFamily(obj); }   // class-pure: a descent test
void HubPassBegin(void*, bool) { g_worldScratch.clear(); }
void HubMatch(void*, void* obj) {
    if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
    if (!R::IsLive(obj)) return;
    g_worldScratch.push_back(WorldHook{obj, R::InternalIndexOf(obj)});
    coop::hook_retrieve::Note(obj);
}
size_t HubPassComplete(void*, bool isFull, uint32_t worldGen) {
    if (isFull) {
        g_world.swap(g_worldScratch);
    } else {
        for (auto it = g_world.begin(); it != g_world.end();)   // a tail pass: prune, then add
            it = R::IsLiveByIndex(it->actor, it->idx) ? it + 1 : g_world.erase(it);
        for (const WorldHook& w : g_worldScratch) {
            bool seen = false;
            for (const WorldHook& k : g_world) if (k.actor == w.actor) { seen = true; break; }
            if (!seen) g_world.push_back(w);
        }
    }
    g_worldScratch.clear();
    g_worldGen = worldGen;
    return g_world.size();
}

// The keyed props `hook` is tied to, appended to g_next without duplicates. `holders` is the player-slot bit
// the hook stands for (0 for a tether); a prop another hook already collected takes the bit into its mask.
void CollectTied(void* hook, uint8_t holders, const std::function<bool()>& current) {
    void* a = nullptr;
    void* b = nullptr;
    if (!current() || !H::AttachedActors(hook, a, b) || !current()) return;
    for (void* p : {a, b}) {
        if (!current()) return;
        if (!p) continue;
        if (const int at = NextIndex(p); at >= 0) {
            g_nextHolders[static_cast<size_t>(at)] |= holders;
            continue;
        }
        if (const ue_wrap::CachedObjRef* known = Known(p)) {
            const auto saved=*known;
            if (saved.Alive() && current()) PushNext(saved, holders);   // its own reference answers
            continue;
        }
        if (!R::IsLive(p) || !PR::IsDescendantOfProp(p) || !current()) continue;
        ue_wrap::CachedObjRef r;
        r.Set(p);
        PushNext(r, holders);
    }
}

// The mask bit of a client's own hook: its owner's slot, from the mirror's key.
uint8_t PlayerBit(uint8_t slot) {
    return slot < coop::players::kMaxPeers ? static_cast<uint8_t>(1u << slot) : uint8_t{0};
}

}  // namespace

void TickPropClaims() {
    if(g_claimTicking)return;
    struct Busy{Busy(){g_claimTicking=true;}~Busy(){g_claimTicking=false;}} busy;
    const auto epoch=g_claimEpoch;
    const auto world=ue_wrap::world_identity::Generation(),serial=coop::net::session_serial::Current();
    auto* session=Session();const auto stops=session?session->StopCount():0;
    const auto current=[&]{return epoch==g_claimEpoch&&world==ue_wrap::world_identity::Generation()&&
        serial==coop::net::session_serial::Current()&&session==Session()&&(!session||
        (session->StopCount()==stops&&session->running()&&session->role()==coop::net::Role::Host));};
    const auto owned=OwnedHooks();const auto adopted=AdoptedHooks();const auto mirrors=Mirrors();const auto worldHooks=g_world;
    const auto previous=g_claimLeases;
    g_next.clear();g_nextHolders.clear();
    for(const auto& o:owned)if(void* h=o.ref.Get())CollectTied(h,PlayerBit(0),current);
    for(const auto& a:adopted)if(void* h=a.ref.Get())CollectTied(h,0,current);
    for(const auto& [key,mirror]:mirrors)if(mirror.bitten)
        if(void* h=mirror.ref.Get())CollectTied(h,mirror.anchored?uint8_t{0}:PlayerBit(SlotOf(key)),current);
    if(g_worldGen==world)for(const auto& w:worldHooks)
        if(R::IsLiveByIndex(w.actor,w.idx))CollectTied(w.actor,0,current);
    if(!current())return;
    const auto next=g_next;const auto holders=g_nextHolders;
    std::unordered_map<void*,coop::prop_drive_host::ClaimLease> leases;
    for(const auto& ref:next){
        if(!current())return;
        if(void* actor=ref.Get()) {
            const auto lease=coop::prop_drive_host::ClaimOwned(actor,coop::prop_drive_host::ClaimSource::Hook,"hook",current);
            if(!current())return;
            if(lease)leases[actor]=lease;
        }
    }
    g_tied=next;g_tiedHolders=holders;g_claimLeases=std::move(leases);
    for(const auto& [actor,lease]:previous){
        if(!current())return;
        if(!InNext(actor))(void)coop::prop_drive_host::ReleaseOwned(lease,current);
    }
}

void ResetPropClaims() {
    ++g_claimEpoch;g_claimLeases.clear();
    g_tied.clear();
    g_next.clear();
    g_tiedHolders.clear();
    g_nextHolders.clear();
    g_world.clear();
    g_worldScratch.clear();
    g_worldGen = 0;
}

void* FindHookByKey(const std::wstring& key) {
    if (key.empty() || key == L"None") return nullptr;
    auto keyed = [&key](void* h) { return h && PR::GetActorSaveKeyString(h) == key; };
    for (const Adopted& a : AdoptedHooks())
        if (void* h = a.ref.Get(); keyed(h)) return h;
    for (const auto& kv : Mirrors())
        if (kv.second.anchored)
            if (void* h = kv.second.ref.Get(); keyed(h)) return h;
    if (g_worldGen == ue_wrap::world_identity::Generation())
        for (const WorldHook& w : g_world)
            if (R::IsLiveByIndex(w.actor, w.idx) && keyed(w.actor)) return w.actor;
    // A hook a save loaded is in the game's own key map from the load, before the scan's first pass has seen it.
    if (ue_wrap::keyed_objects::Available()) {
        void* h = ue_wrap::keyed_objects::Resolve(key.c_str());
        if (h && R::IsLive(h) && H::IsHookFamily(h)) return h;
    }
    return nullptr;
}

void CollectAnchoredKeys(std::vector<std::wstring>& out) {
    auto add = [&out](void* h) {
        if (!h) return;
        std::wstring k = PR::GetActorSaveKeyString(h);
        if (!k.empty() && k != L"None") out.push_back(std::move(k));
    };
    for (const Adopted& a : AdoptedHooks()) add(a.ref.Get());
    for (const auto& kv : Mirrors())
        if (kv.second.anchored) add(kv.second.ref.Get());
    if (g_worldGen == ue_wrap::world_identity::Generation())
        for (const WorldHook& w : g_world)
            if (R::IsLiveByIndex(w.actor, w.idx)) add(w.actor);
}

void RegisterWorldHooksScan() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::element::scan_hub::Register(coop::element::scan_hub::Consumer{
        "hook_world", nullptr, &H::EnsureResolved, &HubIsInstance,
        &HubPassBegin, &HubMatch, &HubPassComplete, /*settleScans*/ 15});
}

}  // namespace coop::hook_sync::detail

namespace coop::hook_sync {

uint8_t PlayerHookHolders(void* actor) {
    UE_ASSERT_GAME_THREAD("hook_sync::PlayerHookHolders");
    if (!actor) return 0;
    for (size_t i = 0; i < detail::g_tied.size(); ++i)
        if (detail::g_tied[i].Is(actor)) return detail::g_tiedHolders[i];
    return 0;
}

}  // namespace coop::hook_sync
