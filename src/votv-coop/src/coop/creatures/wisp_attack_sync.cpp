// coop/creatures/wisp_attack_sync.cpp -- see coop/creatures/wisp_attack_sync.h.

#include "coop/creatures/wisp_attack_sync.h"

#include "coop/element/element.h"
#include "coop/element/mirror_manager.h"
#include "coop/element/mirror_managers.h"  // PropMirrors/NpcMirrors/WaMirrors
#include "coop/element/npc.h"
#include "coop/element/player.h"
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/creatures/npc_sync.h"
#include "coop/player/players_registry.h"
#include "coop/player/ragdoll_gate.h"
#include "coop/player/remote_player.h"
#include "coop/creatures/wisp_grab_hold.h"
#include "coop/creatures/wisp_tear_mirror.h"

#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/actors/wisp.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace coop::wisp_attack_sync {
namespace {

namespace R = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace E = ue_wrap::engine;

// The tear length: the victim ragdolls and the host wisp despawns this long after the grab,
// so the tear animation plays first. Host-decided; carried in the grab message.
constexpr uint32_t kKillDelayMs = 3500;

std::atomic<coop::net::Session*> g_session{nullptr};

// The wisps whose blueprint has the host in a false grab right now. The damage refusal is
// scoped to these: the verb carries its attacker, so only that wisp's damage is refused and
// everything else still hurts the host, where a window-wide latch made it invulnerable.
// Written by Tick and read by the gate's pre callback, both game thread.
std::unordered_set<void*> g_falseGrabWisps;
std::atomic<bool> g_damageWatchInstalled{false};

// The player's damage verb, watched by name so an overriding subclass is covered too.
UE_FACT_FN(kRowAddPlayerDamage, "wisp_attack_sync", nullptr, L"Add Player Damage", AnyClass);
constexpr int kDamageVerbTag = 1;

// The health pin: the host's pre-grab health is pinned across the false-grab window. The
// damage cancel arms one tick after the rising edge (the latch is the previous tick's store),
// so a hit on the rising-edge tick can land; snapshot before that and re-write each tick, so
// any slipped damage is undone at once. Game thread only.
bool  g_haveHostHp = false;
float g_hostHpSnapshot = 0.f;
// The ragdoll belt: the false-grab montage's damage notify writes playerDamaged and its drop
// notify fires the ragdoll, both blueprint-internal, so neither the damage cancel nor the
// health pin can stop the resulting host death (a ragdoll death carries no health write). The
// player's canRagdoll flag is the blueprint's own early-out for that path; it is forced off
// for the window and restored on the falling edge or at disconnect. Game thread only.
bool g_canRagdollForced = false;
// The native-grab rising edge per wisp, game thread only: the false-grab abort fires the
// release once per grab; per-tick spam would queue a latent grab-reset chain on the wisp every
// tick and re-flop the host before the ragdoll belt lands.
std::unordered_map<uint32_t, bool> g_lastNativeGrab;

std::unordered_set<uint32_t> g_relayed;  // wisp eids whose grab was already relayed
std::unordered_map<uint32_t,ue_wrap::CachedObjRef> g_wispBodies;
// The grab window: the wisp despawns at the deadline (breaking the re-grab loop), and until
// then the host lifts it, the native kill's signature rise; the pose stream carries the lift
// to every peer, and the attached victim and the held puppet ride.
struct PendingDestroy {
    uint32_t eid=0;
    uint64_t deadlineMs=0, lastLiftMs=0, token=0;
    uint32_t world=0;
    ue_wrap::CachedObjRef actor;
};
std::vector<PendingDestroy> g_pendingDestroy;
uint64_t g_pendingEpoch=0, g_nextPendingToken=0;
uint32_t g_hostWorld=0;
bool g_pendingTicking=false, g_hostTicking=false;

// The aggro selector, the host-authoritative owner of the wisp's Target. While at least one
// player candidate (the host pawn or a live puppet) is inside the native acquire radius and
// visible, we own the wisp's target: a uniform random pick among the eligible (the blueprint's
// own nearest pick made the host the perpetual victim), sticky (the victim is held while it
// stays valid and in range, re-rolled only on death, leaving or going out of range),
// re-asserted through the raw target write every tick so it dominates the blueprint's re-scan.
// With no eligible player the native scan owns the target, so the kerfur and hound hunting is
// preserved. A documented divergence: natively a kerfur closer than any player would win the
// nearest pick; the selector prefers players whenever one is eligible, since the killer wisp
// is the player-hunting event creature and fairness among players is the point.
struct Aggro { uint8_t slot; void* actor; ue_wrap::CachedObjRef body; };
std::unordered_map<uint32_t, Aggro> g_aggro;

// The two-stage close: the native capture fires at contact, not at the arm radius. Relaying at
// the arm radius killed the victim from 5 m away; now the radius-and-line-of-sight edge only
// arms a closing window, the wisp keeps chasing (its move-to acceptance is a few units) and
// the grab fires at contact, or at the hover timeout with the line of sight re-verified, so a
// blocked hover never kills through a wall; it re-arms when the victim is visible again. The
// closing entry doubles as the arm-edge memory.
struct Closing { void* victim; ue_wrap::CachedObjRef body; uint64_t armedAtMs; };
std::unordered_map<uint32_t, Closing> g_closing;

constexpr float    kAcquireRadius  = 5000.f;  // native scanForActors radius
constexpr float    kKeepRadius     = 5500.f;  // stickiness hysteresis (re-roll past this)
constexpr float    kContactRadius  = 200.f;   // fire the synthetic grab at contact-ish
constexpr uint64_t kCloseTimeoutMs = 2500;    // hovering-wisp fallback -> fire anyway (LOS-gated)
constexpr float    kLiftCmPerSec   = 150.f;   // grab-window rise (~5 m over the 3.5 s tear)
constexpr float    kLiftMaxStepCm  = 50.f;    // per-tick cap (a hitch must not teleport it)

int UniformPick(int n) {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> d(0, n - 1);
    return d(rng);
}

// The NPC-victim death-watch, game thread only. A wisp kills a kerfur or hound through the
// NPC's own damage and death chain, which self-destroys through a virtual call invisible to
// ProcessEvent, so the NPC destroy observer never fires and the mirror would survive as a
// ghost on clients. When a wisp closes on a tracked NPC target its eid is enrolled here; the
// discharge polls liveness and, on death, runs the idempotent NPC destroy body, so the mirror
// despawns. Keyed by victim eid; the actor and index snapshot plus an identity re-check guard
// against pointer recycling.
struct NpcWatch {
    ue_wrap::CachedObjRef actor;
    uint64_t deadlineMs=0, token=0;
    uint32_t world=0;
};
std::unordered_map<uint32_t, NpcWatch> g_npcKillWatch;
constexpr uint64_t kNpcWatchWindowMs = 6000;  // watch a closed-on NPC for death up to 6s, else drop

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void ObserveWispBody(uint32_t eid,void* actor) {
    auto& prior=g_wispBodies[eid];
    if (prior.Is(actor)) return;
    g_aggro.erase(eid);g_closing.erase(eid);g_relayed.erase(eid);g_lastNativeGrab.erase(eid);
    prior.Set(actor);
}

bool PlayerAlive(uint8_t slot,void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    auto& players=coop::players::Registry::Get();
    if (slot!=coop::players::kPeerIdHost) {
        auto* puppet=players.Puppet(slot);
        return puppet && puppet->valid() && puppet->GetActor()==actor && puppet->GetHealth()>0.f;
    }
    if (players.Local()!=actor) return false;
    ue_wrap::CachedObjRef victim;victim.Set(actor);
    const auto world=ue_wrap::world_identity::Generation();
    float dead=0;
    return ue_wrap::vitals::Read(ue_wrap::vitals::Field::Dead,&dead) && dead==0 &&
        victim.Get() && players.Local()==actor && world==ue_wrap::world_identity::Generation();
}

// Refuse the player's damage verb for a false-grabbing wisp, per call, by its own attacker
// argument. The verb is Blueprint-internal on every route -- the killer wisp reaches it as
// `getMainPlayer()->Add Player Damage(24, .., source=this)` through a context switch, and the
// player's own ubergraph self-calls it -- so no ProcessEvent seam can see it; the script-body
// gate sees the body with its parameters. MTA decides the same question the same way, per event
// with the inflictor in hand (reference/mtasa-blue/Client/sdk/multiplayer/CMultiplayer.h:86 and
// its handler, which reads GetInflictingEntity before deciding).
// Cheap: a set that is empty except during a grab window, then one offset read.
sg::Verdict OnAddPlayerDamagePre(const sg::Call& call) {
    if (g_falseGrabWisps.empty() || !call.locals || !call.function) return sg::Verdict::Run;
    // The HOST'S OWN pawn only. The wisp reaches the verb through lib_C::getMainPlayer, which
    // returns the local player, so in practice the body never runs on a puppet -- but the lane's
    // promise is about the host, and a refusal on a mirrored body would be a different claim.
    if (call.object != coop::players::Registry::Get().Local()) return sg::Verdict::Run;
    // Only mainPlayer_C declares this verb, so one cached offset serves the process; keyed on
    // the function anyway, since a name watch is free to match a second class after a recook.
    static void* sFn = nullptr;
    static int32_t sSourceOff = -1;
    if (call.function != sFn) {
        sFn = call.function;
        sSourceOff = R::FindParamOffset(call.function, L"source");
    }
    if (sSourceOff < 0) return sg::Verdict::Run;
    void* source = *reinterpret_cast<void**>(
        static_cast<uint8_t*>(call.locals) + sSourceOff);
    if (!source || g_falseGrabWisps.count(source) == 0) return sg::Verdict::Run;
    // The set holds raw pointers rebuilt once a tick, so a freed wisp's address could be reused
    // by an unrelated actor before the next rebuild. One class compare closes that window; the
    // NPC death watch in this file carries the same guard for the same reason.
    if (!ue_wrap::wisp::IsKillerWisp(source)) return sg::Verdict::Run;
    return sg::Verdict::Cancel;
}

using coop::element::NpcMirrors;   // canonical accessor (coop/element/mirror_managers.h)

// Relay one client-victim grab: the grab to the victim slot, the tear to all, the tear run on
// the host's own wisp, and the host wisp's despawn scheduled after the tear. The release was
// already called on the rising edge, before this.
bool RelayGrab(coop::net::Session* s, void* wispActor, uint32_t wispEid, void* victimActor) {
    const auto epoch=g_pendingEpoch;
    const auto world=ue_wrap::world_identity::Generation();
    ue_wrap::CachedObjRef source;source.Set(wispActor);
    ue_wrap::CachedObjRef victim;victim.Set(victimActor);
    const uint8_t victimSlot = coop::players::Registry::Get().PeerIdOfActor(victimActor);
    if (victimSlot == coop::players::kPeerIdUnknown || victimSlot == coop::players::kPeerIdHost) {
        UE_LOGW("wisp_attack: victim puppet has no client slot (slot=%u) -- not relaying", victimSlot);
        return false;
    }
    coop::element::Player* victimEl = coop::players::Registry::Get().GetPlayerElement(victimSlot);
    if (!victimEl) {
        UE_LOGW("wisp_attack: no Player Element for victim slot %u -- not relaying", victimSlot);
        return false;
    }
    const uint32_t victimEid = static_cast<uint32_t>(victimEl->GetId());
    const auto generation=s->peerGenerationForSlot(victimSlot);
    const auto current=[&] {
        if (epoch!=g_pendingEpoch || world!=ue_wrap::world_identity::Generation() ||
            !source.Get() || !victim.Get() || g_session.load()!=s || !s->connected() ||
            s->role()!=coop::net::Role::Host || !s->IsSlotWorldReady(victimSlot) ||
            s->peerGenerationForSlot(victimSlot)!=generation || !PlayerAlive(victimSlot,victimActor)) return false;
        auto* entity=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(wispEid));
        auto* player=coop::players::Registry::Get().GetPlayerElement(victimSlot);
        return entity && entity->LiveActor()==wispActor && player &&
            static_cast<uint32_t>(player->GetId())==victimEid;
    };
    if (!current()) return false;
    // The host's health protection is the damage cancel and the per-tick health pin in Tick, not
    // a write here.

    coop::net::WispGrabPayload g{};
    g.victimElementId = victimEid;
    g.wispElementId   = wispEid;
    g.killDelayMs     = kKillDelayMs;
    const uint64_t started=NowMs();
    if (!s->SendReliableToSlot(victimSlot, coop::net::ReliableKind::WispGrab, &g, sizeof(g)) || !current()) return false;

    coop::net::WispTearPayload t{};
    t.wispElementId = wispEid;
    t.victimSlot    = victimSlot;
    s->SendReliable(coop::net::ReliableKind::WispTear, &t, sizeof(t));
    if (!current()) return false;

    // The host does not receive its own broadcast. Use the same animation-readiness
    // retry path as clients; the receiver also holds the victim's puppet at the socket.
    coop::wisp_tear_mirror::OnWispTear(t, 0);

    if (!current()) return false;

    PendingDestroy pending;
    pending.eid=wispEid;pending.deadlineMs=started+kKillDelayMs;pending.lastLiftMs=started;
    pending.token=++g_nextPendingToken;pending.world=world;pending.actor=source;
    g_pendingDestroy.push_back(pending);
    UE_LOGI("wisp_attack: RELAYED grab -- wispEid=%u victimSlot=%u victimEid=%u (WispGrab->slot, "
            "WispTear->all, local tear+hold, lift armed, wisp despawn scheduled +%ums)",
            wispEid, victimSlot, victimEid, kKillDelayMs);
    return true;
}

void DischargePendingDestroys() {
    if (g_pendingDestroy.empty() || g_pendingTicking) return;
    g_pendingTicking=true;
    struct Guard {~Guard(){g_pendingTicking=false;}} guard;
    const auto epoch=g_pendingEpoch;
    const uint64_t now = NowMs();
    const auto pending=g_pendingDestroy;
    for (const auto& row:pending) {
        if (epoch!=g_pendingEpoch) return;
        const auto find=[&]{return std::find_if(g_pendingDestroy.begin(),g_pendingDestroy.end(),
            [&](const PendingDestroy& live){return live.token==row.token;});};
        const auto erase=[&]{const auto it=find();if(it!=g_pendingDestroy.end())g_pendingDestroy.erase(it);};
        void* actor=row.actor.Get();
        const auto current=[&] {
            if (epoch!=g_pendingEpoch || row.world!=ue_wrap::world_identity::Generation() ||
                !actor || row.actor.Get()!=actor || find()==g_pendingDestroy.end()) return false;
            auto* el=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(row.eid));
            return el && el->LiveActor()==actor && ue_wrap::wisp::IsKillerWisp(actor);
        };
        if (!current()) {erase();continue;}
        if (now < row.deadlineMs) {
            // The grab window still open: lift the wisp, the native kill's signature rise. The pose
            // stream mirrors it everywhere; the attached victim and the held puppet ride the socket
            // up with it. Scaled by elapsed time and step-capped.
            const uint64_t elapsed=now>=row.lastLiftMs?now-row.lastLiftMs:0;
            float dz = kLiftCmPerSec * static_cast<float>(elapsed) * 0.001f;
            if (dz > kLiftMaxStepCm) dz = kLiftMaxStepCm;
            ue_wrap::FVector loc{};
            if (dz > 0.f && E::TryGetActorLocation(actor, loc)) {   // unread: no lift this tick
                if (!current()) {erase();continue;}
                loc.Z += dz;
                if (std::isfinite(loc.X) && std::isfinite(loc.Y) && std::isfinite(loc.Z))
                    E::SetActorLocation(actor, loc);
            }
            if (!current()) {erase();continue;}
            find()->lastLiftMs=now;
            continue;
        }
        // The destroy fires the NPC destroy PRE observer and the destroy broadcast, so the mirrors
        // despawn too, and every peer's grab hold self-releases on its liveness guard.
        // Consume before native destruction: its callbacks may clear/repopulate
        // the queue, including another job with this eid in a newer world.
        erase();
        E::DestroyActor(actor);
        UE_LOGI("wisp_attack: despawned host wisp eid=%u after tear (breaks the re-grab loop)", row.eid);
    }
}

// Enrol a wisp-targeted NPC's exact body/world; notify by the original id even
// after collection, without retiring a newer occupant of that id/address.
void EnrollNpcKillWatch(uint32_t npcEid, void* npcActor) {
    if (!npcActor || !R::IsLive(npcActor)) return;
    const uint64_t deadline = NowMs() + kNpcWatchWindowMs;
    const auto world=ue_wrap::world_identity::Generation();
    auto it = g_npcKillWatch.find(npcEid);
    if (it != g_npcKillWatch.end() && it->second.world==world && it->second.actor.Is(npcActor)) {
        it->second.deadlineMs=deadline;return;
    }
    NpcWatch watch;watch.actor.Set(npcActor);watch.deadlineMs=deadline;
    watch.world=world;watch.token=++g_nextPendingToken;
    g_npcKillWatch[npcEid]=watch;
    UE_LOGI("wisp_attack: watching NPC eid=%u (a wisp closed on it) for a wisp-kill death", npcEid);
}

// Poll the watched NPCs; mirror the death of any that died (the self-destroy the observer
// cannot see), and drop survivors at their deadline.
void DischargeNpcKillWatch() {
    if (g_npcKillWatch.empty()) return;
    const uint64_t now = NowMs();
    const auto epoch=g_pendingEpoch;
    const auto watches=g_npcKillWatch;
    for (const auto& [eid,w]:watches) {
        if (epoch!=g_pendingEpoch) return;
        const auto it=g_npcKillWatch.find(eid);
        if (it==g_npcKillWatch.end() || it->second.token!=w.token) continue;
        auto* entity=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(eid));
        void* live=entity?entity->LiveActor():nullptr;
        void* original=w.actor.Get();
        if (w.world!=ue_wrap::world_identity::Generation() || !entity || live!=original ||
            static_cast<uint32_t>(coop::npc_sync::GetNpcIdForActor(w.actor.Raw()))!=eid) {
            g_npcKillWatch.erase(it);continue;
        }
        if (original) {
            if (now>=w.deadlineMs)g_npcKillWatch.erase(it);
            continue;
        }
        // Dead and still mapped to our eid: the wisp killed it. Broadcast the destroy.
        g_npcKillWatch.erase(it);
        // Retire the original id, not whichever newer id now uses this address.
        // The canonical live-actor check above protects a reused id as well.
        coop::npc_sync::SyncDestroyedNpcByEid(static_cast<coop::element::ElementId>(eid),w.actor.Raw());
        UE_LOGI("wisp_attack: NPC victim eid=%u died (wisp kill, BP-internal self-destroy) -- "
                "mirrored EntityDestroy so the kerfur/fossilhound mirror despawns", eid);
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    auto* previous=g_session.load(std::memory_order_acquire);
    if (previous!=session) {
        const auto epoch=g_pendingEpoch;
        OnDisconnect();
        if (g_pendingEpoch!=epoch+1 || g_session.load()!=previous) return;
    }
    g_session.store(session, std::memory_order_release);
    if (g_damageWatchInstalled.load(std::memory_order_acquire)) return;
    // No throttle and no wait for the player class: a name watch registers immediately and the
    // gate resolves the name itself on the game thread, so the old once-a-second retry (there to
    // bound an object-array lookup that no longer happens) would only delay the guard.
    if (sg::WatchName(kRowAddPlayerDamage, kDamageVerbTag, &OnAddPlayerDamagePre, nullptr)) {
        g_damageWatchInstalled.store(true, std::memory_order_release);
        UE_LOGI("wisp_attack: watching %ls at the script-body gate (attacker-scoped refusal)",
                kRowAddPlayerDamage.Name());
    }
}

bool RefreshHostWorld(coop::net::Session* session) {
    const auto world=ue_wrap::world_identity::Generation();
    if (world==g_hostWorld) return true;
    const auto epoch=g_pendingEpoch;
    OnDisconnect();
    if (g_pendingEpoch!=epoch+1 || g_session.load()!=session ||
        world!=ue_wrap::world_identity::Generation() || !session->connected() ||
        session->role()!=coop::net::Role::Host) return false;
    g_hostWorld=world;
    return true;
}

void Tick() {
    if (g_hostTicking) return;
    g_hostTicking=true;
    struct Guard {~Guard(){g_hostTicking=false;}} guard;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) {
        if (!g_falseGrabWisps.empty()) g_falseGrabWisps.clear();
        return;
    }
    if (!RefreshHostWorld(s)) return;
    const auto epoch=g_pendingEpoch;
    const auto current=[&]{return epoch==g_pendingEpoch && g_hostWorld==ue_wrap::world_identity::Generation() &&
        g_session.load()==s && s->connected() && s->role()==coop::net::Role::Host;};
    // Walk the host's tracked NPC Elements, a small set, not the object array, and find the
    // killer wisps.
    std::vector<coop::element::Npc*> npcs;
    NpcMirrors().Snapshot(npcs);
    struct Observed {uint32_t eid;ue_wrap::CachedObjRef actor;};
    std::vector<Observed> observed;
    for (auto* npc:npcs) {
        if (!npc) continue;
        void* actor=npc->LiveActor();
        if (!actor || !ue_wrap::wisp::IsKillerWisp(actor)) continue;
        const auto eid=static_cast<uint32_t>(coop::npc_sync::GetNpcIdForActor(actor));
        if (!eid || eid==static_cast<uint32_t>(coop::element::kInvalidId)) continue;
        Observed row{eid,{}};row.actor.Set(actor);observed.push_back(row);
    }
    bool anyHostFalseGrab = false;  // a wisp's BP grabbed the HOST while its real Target is a puppet
    std::unordered_set<void*> falseGrabbers;  // and the wisps doing it, for the damage refusal
    std::unordered_set<uint32_t> liveWispEids;

    for (const auto& row:observed) {
        void* actor=row.actor.Get();
        const auto eid=row.eid;
        const auto currentActor=[&] {
            if (!current() || !actor || row.actor.Get()!=actor) return false;
            auto* entity=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(eid));
            return entity && entity->LiveActor()==actor;
        };
        if (!currentActor()) continue;
        ObserveWispBody(eid,actor);
        liveWispEids.insert(eid);

        ue_wrap::wisp::State st;
        if (!ue_wrap::wisp::ReadState(actor, st)) continue;
        void* victim = st.target;
        auto& reg = coop::players::Registry::Get();

        // The aggro selector, one owner (see its state comment). The hands-off set: our relayed
        // grab window, a committed fatality, and a native grab or try-grab when the selector's pick
        // is the host (or there is no pick), since the native host kill must not have its target
        // yanked mid-choreography. A native try-grab or grab while our pick is a puppet is the
        // false grab (the blueprint arms on host proximity, not on the target); there the selector
        // keeps asserting the puppet and the false-grab protections below abort the host side.
        const bool nativeBusy = st.tryGrab || st.grab || st.killed;
        const auto aitPeek = g_aggro.find(eid);
        const bool pickedPuppet =
            aitPeek != g_aggro.end() && aitPeek->second.slot != coop::players::kPeerIdHost;
        const bool midSequence =
            g_relayed.count(eid) != 0 || st.killed || (nativeBusy && !pickedPuppet);
        if (!st.harmless && !midSequence) {
            auto ait = g_aggro.find(eid);
            bool haveValid = false;
            if (ait != g_aggro.end()) {
                Aggro& a = ait->second;
                const bool identity=a.body.Get()==a.actor && PlayerAlive(a.slot,a.actor);
                if (!currentActor()) return;
                const bool near=identity && ue_wrap::wisp::DistanceTo(actor, a.actor)<=kKeepRadius;
                if (!currentActor()) return;
                if (near && a.body.Get()==a.actor) {
                    haveValid = true;  // stickiness: hold the victim
                } else {
                    g_aggro.erase(ait);  // died / left / out of range -> re-roll below
                }
            }
            if (!haveValid) {
                struct Cand { uint8_t slot; void* a; ue_wrap::CachedObjRef body; };
                Cand cands[coop::players::kMaxPeers + 1];
                int n = 0;
                const auto add=[&](uint8_t slot,void* actor){
                    cands[n].slot=slot;cands[n].a=actor;cands[n].body.Set(actor);++n;
                };
                void* hostPawn = reg.Local();
                const bool hostAlive=PlayerAlive(coop::players::kPeerIdHost,hostPawn);
                if (!currentActor()) return;
                const bool hostNear=hostAlive &&
                    ue_wrap::wisp::DistanceTo(actor, hostPawn)<=kAcquireRadius;
                if (!currentActor()) return;
                if (hostNear) {
                    const bool reachable=ue_wrap::wisp::CanReach(actor,hostPawn);
                    if (!currentActor()) return;
                    if (reachable) add(coop::players::kPeerIdHost,hostPawn);
                }
                for (uint8_t s2 = 0; s2 < coop::players::kMaxPeers; ++s2) {
                    coop::RemotePlayer* rp = reg.Puppet(s2);
                    if (!rp || !rp->valid()) continue;
                    void* pa = rp->GetActor();
                    if (!PlayerAlive(s2,pa)) continue;
                    const auto distance=ue_wrap::wisp::DistanceTo(actor,pa);
                    if (!currentActor()) return;
                    if (distance>kAcquireRadius) continue;
                    const bool reachable=ue_wrap::wisp::CanReach(actor,pa);
                    if (!currentActor()) return;
                    if (!reachable) continue;
                    add(s2,pa);
                }
                // Earlier candidates may retire while another candidate's LOS
                // trace dispatches. Never choose a successor by a recycled pointer.
                for (int i=0;i<n;) {
                    const bool alive=cands[i].body.Get()==cands[i].a && PlayerAlive(cands[i].slot,cands[i].a);
                    if (!currentActor()) return;
                    if (alive) ++i;else cands[i]=cands[--n];
                }
                if (n > 0) {
                    const Cand& c = cands[UniformPick(n)];
                    g_aggro[eid] = Aggro{c.slot,c.a,c.body};
                    haveValid = true;
                    UE_LOGI("wisp_aggro: wisp eid=%u picked victim slot=%u (%d eligible; "
                            "uniform random + sticky)", eid, c.slot, n);
                }
            }
            if (haveValid) {
                // Re-assert every tick: the blueprint's nearest re-scan writes the target on its
                // own cadence, and the per-tick write dominates it between scans.
                Aggro& a = g_aggro.find(eid)->second;
                ue_wrap::wisp::WriteTarget(actor, a.actor);
                victim = a.actor;  // downstream classification sees OUR pick this tick
            }
        }

        // Classify the wisp's victim: the selector's pick, or the blueprint's own target when no
        // player is eligible. The blueprint chases any of the host pawn, a puppet, a kerfur or a
        // hound, but only grabs and kills player 0, the host; the missing effect is synthesised
        // against the real victim.
        const bool isPuppet = victim && reg.IsPuppet(victim);
        const uint32_t npcVictimEid =
            (victim && !isPuppet && !reg.IsLocal(victim))
                ? static_cast<uint32_t>(coop::npc_sync::GetNpcIdForActor(victim))
                : static_cast<uint32_t>(coop::element::kInvalidId);
        const bool isNpcVictim = npcVictimEid != static_cast<uint32_t>(coop::element::kInvalidId);

        // The wisp is in lethal range of its victim, the blueprint's grab radius, evaluated against
        // the actual victim rather than the host's pawn. The liveness check is required before the
        // dereference: the blueprint does not null its target the frame the target dies, so a
        // dead-but-referenced pointer would fault in the range or enrol reads (the classification
        // above is map-key-only). The liveness primitive is SEH-firewalled, safe on a freed
        // pointer.
        const bool inRange = victim && R::IsLive(victim) && !st.harmless &&
                             ue_wrap::wisp::InGrabRange(actor, victim);
        if (!currentActor()) return;

        // An NPC victim: a wisp closing on a kerfur or hound will kill it through its own
        // blueprint. Watch the eid, so the blueprint-internal self-destroy is mirrored.
        if (isNpcVictim && inRange) EnrollNpcKillWatch(npcVictimEid, victim);

        // A puppet victim: the two-stage close (arm at the radius with line of sight, swoop, fire
        // at contact).
        if (isPuppet) {
            // If the blueprint also grabbed, or is winding up to grab, the host (which happened to
            // be within the radius too), that is a false grab: protect the host across the whole
            // window while the kill is redirected to the puppet.
            if (st.grab || st.tryGrab) {
                anyHostFalseGrab = true;
                falseGrabbers.insert(actor);
                // Abort the native false grab on its own rising edge, decoupled from the closing
                // machinery below: an edge- or contact-gated abort could run after the grab
                // montage's damage notify set playerDamaged inline, turning the release's own
                // ragdoll lethal to the host, or never run at all on a blocked hover. Once
                // playerDamaged is up the release is no longer a safe abort, so it is skipped; the
                // ragdoll belt at the end of Tick keeps the montage's drop notify from killing the
                // host, and the wisp despawn breaks the hold.
                if (st.grab && !g_lastNativeGrab[eid] && !st.playerDamaged)
                    ue_wrap::wisp::CallReleasePlayer(actor);
                if (!currentActor()) return;
            }
            const bool relayed = g_relayed.count(eid) != 0;
            auto cit = g_closing.find(eid);
            if (!inRange && cit!=g_closing.end()) {g_closing.erase(cit);cit=g_closing.end();}
            if (cit == g_closing.end()) {
                // The arm edge: in the blueprint's grab radius and visible, the native grab arm's
                // own line-of-sight gate; distance alone was a wall-through divergence.
                const bool reachable=!relayed && inRange && ue_wrap::wisp::CanReach(actor,victim);
                if (!currentActor()) return;
                if (reachable) {
                    Closing close{victim,{},NowMs()};close.body.Set(victim);g_closing[eid]=close;
                    UE_LOGI("wisp_attack: CLOSING -- wispEid=%u armed (550u + LOS), swooping to "
                            "%.0fu (timeout %llu ms)",
                            eid, kContactRadius, static_cast<unsigned long long>(kCloseTimeoutMs));
                }
            } else {
                Closing& c = cit->second;
                if (victim!=c.victim || c.body.Get()!=victim || !PlayerAlive(reg.PeerIdOfActor(victim),victim)) {
                    g_closing.erase(cit);  // victim changed/died mid-close -> re-arm fresh
                } else {
                    const float d = ue_wrap::wisp::DistanceTo(actor, victim);
                    if (!currentActor()) return;
                    const bool timeout = (NowMs() - c.armedAtMs) >= kCloseTimeoutMs;
                    if (d <= kContactRadius || timeout) {
                        const bool reachable=ue_wrap::wisp::CanReach(actor,victim);
                        if (!currentActor()) return;
                        if (reachable) {
                            // Relay once; the relayed latch and the scheduled wisp despawn break
                            // the re-grab loop. The false-grab abort is per tick, above.
                            if (!relayed) {
                                const bool admitted=RelayGrab(s,actor,eid,victim);
                                if (!currentActor()) return;
                                if (!admitted) continue;
                                g_relayed.insert(eid);
                            }
                            UE_LOGI("wisp_attack: CONTACT%s -- wispEid=%u d=%.0f -> grab fired",
                                    timeout ? " (timeout)" : "", eid, d);
                            g_closing.erase(cit);
                        } else if (timeout) {
                            // A blocked hover at the timeout: never kill through a wall. Drop the
                            // window; it re-arms when the victim is visible again.
                            UE_LOGI("wisp_attack: closing TIMEOUT with LOS blocked -- wispEid=%u "
                                    "re-arms when visible", eid);
                            g_closing.erase(cit);
                        }
                        // Contact but blocked (a thin floor between): keep closing, retry next
                        // tick.
                    }
                }
            }
        } else {
            g_closing.erase(eid);  // victim no longer a puppet -> drop any stale window
        }
        g_lastNativeGrab[eid] = st.grab;  // rising-edge memory for the false-grab abort
    }

    if (!current()) return;
    g_falseGrabWisps.swap(falseGrabbers);

    // The host health pin: snapshot on the rising edge (before the wisp's limb damage), then
    // re-write each tick while a wisp false-grabs the host (its real target being a puppet), so
    // any hit that slipped past the one-tick-late cancel is undone at once. Cleared when over.
    if (anyHostFalseGrab) {
        if (!g_haveHostHp) {
            float hp = 0.f;
            const bool read=ue_wrap::vitals::Read(ue_wrap::vitals::Field::Health,&hp);
            if (!current()) return;
            g_haveHostHp=read;
            g_hostHpSnapshot = hp;
        } else {
            ue_wrap::vitals::Write(ue_wrap::vitals::Field::Health, g_hostHpSnapshot);
            if (!current()) return;
        }
        // The ragdoll belt: block every ragdoll cause on the host for the window (the montage's own
        // notifies would ragdoll-kill it). Routed through the ragdoll gate rather than writing the
        // raw flag: the flag is process-wide, and a second lane that also held it had its hold
        // silently freed by this lane's unconditional window close. The gate refcounts, so neither
        // holder can free the other.
        if (!g_canRagdollForced) {
            void* local = coop::players::Registry::Get().Local();
            if (local && R::IsLive(local)) {
                g_canRagdollForced = true;
                coop::ragdoll_gate::Hold(coop::ragdoll_gate::Holder::WispFalseGrab);
                if (!current()) return;
                coop::ragdoll_gate::Tick(local);
                if (!current()) return;
                UE_LOGI("wisp_attack: canRagdoll=false forced on the host for the false-grab window");
            }
        }
    } else if (g_haveHostHp || g_canRagdollForced) {
        g_haveHostHp = false;
        if (g_canRagdollForced) {
            g_canRagdollForced = false;
            coop::ragdoll_gate::Release(coop::ragdoll_gate::Holder::WispFalseGrab);
            if (!current()) return;
            UE_LOGI("wisp_attack: canRagdoll restored on the host (false-grab window over)");
        }
    }

    DischargePendingDestroys();
    if (!current()) return;
    DischargeNpcKillWatch();  // mirror any wisp-killed NPC's BP-internal self-destroy
    if (!current()) return;

    // Drop state for wisps that despawned, no longer tracked, so a recycled eid starts clean.
    for (auto it = g_aggro.begin(); it != g_aggro.end();)
        it = liveWispEids.count(it->first) ? std::next(it) : g_aggro.erase(it);
    for (auto it = g_closing.begin(); it != g_closing.end();)
        it = liveWispEids.count(it->first) ? std::next(it) : g_closing.erase(it);
    for (auto it = g_relayed.begin(); it != g_relayed.end();)
        it = liveWispEids.count(*it) ? std::next(it) : g_relayed.erase(it);
    for (auto it = g_lastNativeGrab.begin(); it != g_lastNativeGrab.end();)
        it = liveWispEids.count(it->first) ? std::next(it) : g_lastNativeGrab.erase(it);
    for (auto it=g_wispBodies.begin();it!=g_wispBodies.end();)
        it=liveWispEids.count(it->first)?std::next(it):g_wispBodies.erase(it);
}

void OnDisconnect() {
    ++g_pendingEpoch;
    g_hostWorld=0;
    const bool releaseRagdoll=g_canRagdollForced;
    g_canRagdollForced=false;
    g_falseGrabWisps.clear();
    g_haveHostHp = false;
    g_aggro.clear();
    g_closing.clear();
    g_relayed.clear();
    g_lastNativeGrab.clear();
    g_wispBodies.clear();
    g_pendingDestroy.clear();
    g_npcKillWatch.clear();
    if (releaseRagdoll) {
        // Never strand the local player un-ragdollable past the session: a mid-window teardown
        // would otherwise block every future ragdoll cause, real deaths included. Release only our
        // hold; the gate restores the flag once the last holder is gone.
        coop::ragdoll_gate::Release(coop::ragdoll_gate::Holder::WispFalseGrab);
    }
}

}  // namespace coop::wisp_attack_sync
