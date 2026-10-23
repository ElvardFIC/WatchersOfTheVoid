#include "coop/props/prop_action_sync.h"

#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_save_data.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/safe.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/world/keyed_objects.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/hit_result.h"
#include "ue_wrap/world/weapon_catalog.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <unordered_map>
#include <vector>

namespace coop::prop_action_sync {
namespace {
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;

constexpr uint8_t kDamage = 1, kSlice = 2, kOpenMre = 3, kUnlockSafe = 4, kWallDamage = 5;
constexpr float kReachUU = 450.f;
constexpr float kBurst = 8.f, kRate = 8.f;
constexpr int kDamageTag = 0x5041444D; // PADM
constexpr int kSliceTag = 0x5041534C;  // PASL
constexpr int kMreTag = 0x50414D52;  // PAMR
constexpr wchar_t kDamageFn[] = L"addDamage";
constexpr wchar_t kSliceFn[] = L"slice";

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_watchesInstalled = false;
struct QueuedIntent {
    coop::net::PropActionIntentPayload payload{};
    uint64_t queuedAt = 0;
    ue_wrap::CachedObjRef target,type;
    uint64_t revision=0;
};
std::deque<QueuedIntent> g_pending;
bool g_draining=false;
uint64_t g_epoch=0,g_intentRevision=0;
coop::net::Session* g_source=nullptr;
uint32_t g_serial=0;
coop::net::Role g_role=coop::net::Role::Host;
bool g_running=false,g_connected=false;
struct Bucket { float tokens = kBurst; uint64_t stamp = 0; };
Bucket g_buckets[coop::net::kMaxPeers];
uint32_t g_world = 0;
ue_wrap::CachedObjRef g_lastMreRequest;
uint64_t g_lastMreRequestAt = 0;
struct SafePublish {ue_wrap::CachedObjRef actor,type;uint64_t retryAt=0,serial=0;};
std::unordered_map<std::wstring,SafePublish> g_safePublish;
uint64_t g_safeSerial=0;
struct MreScope {
    coop::net::Session* session=nullptr;
    uint64_t epoch=0,invocation=0;
    ue_wrap::CachedObjRef actor,type,function,owner;
    uint8_t slot=0;uint16_t person=0;uint32_t born=0;
};
std::vector<MreScope> g_handOpen,g_mreSpawn;
struct MreAttempt {ue_wrap::CachedObjRef actor,type;uint64_t revision=0;bool admitted=false;};
std::vector<MreAttempt> g_mreAttempts;
uint64_t g_mreRevision=0;

bool Current(coop::net::Session* s,uint64_t epoch) {
    return s && g_session.load()==s && g_source==s && epoch==g_epoch && s->running() && s->connected() &&
        s->role()==g_role && g_serial==coop::net::session_serial::Current() && g_world==ue_wrap::world_identity::Generation();
}
bool TargetCurrent(const ue_wrap::CachedObjRef& actor,const ue_wrap::CachedObjRef& type) {
    return actor.Get() && type.Get() && R::ClassOf(actor.Raw())==type.Raw();
}
bool ScopeCurrent(const MreScope& row) {
    const auto& person=coop::roster_ledger::Get(row.slot);
    return Current(row.session,row.epoch) && row.session->role()==coop::net::Role::Client &&
        row.person==person.playerNo && row.born==person.bornGeneration && SG::IsInvocationActive(row.invocation) &&
        TargetCurrent(row.actor,row.type) && row.function.Get() && row.owner.Get() && R::OuterOf(row.function.Raw())==row.owner.Raw();
}

void EnsureWorld() {
    const uint32_t world = ue_wrap::world_identity::Generation();
    auto* s=g_session.load();const auto serial=coop::net::session_serial::Current();
    const auto role=s?s->role():coop::net::Role::Host;
    const bool running=s && s->running(),connected=s && s->connected();
    if(world==g_world && s==g_source && serial==g_serial && role==g_role && running==g_running && connected==g_connected)return;
    g_world=world;g_source=s;g_serial=serial;g_role=role;g_running=running;g_connected=connected;++g_epoch;
    g_pending.clear();
    g_handOpen.clear();g_mreSpawn.clear();g_mreAttempts.clear();
    g_safePublish.clear();++g_safeSerial;
    g_lastMreRequest.Reset();
    g_lastMreRequestAt = 0;
    for (auto& bucket : g_buckets) bucket = {};
}

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool TakeToken(uint8_t slot) {
    Bucket& b = g_buckets[slot]; const uint64_t now = NowMs();
    if (b.stamp && now > b.stamp) b.tokens = (std::min)(kBurst, b.tokens + kRate * (now - b.stamp) / 1000.f);
    b.stamp = now; if (b.tokens < 1.f) return false; b.tokens -= 1.f; return true;
}

bool Sawable(void* actor,const std::function<bool()>& caller={}) {
    if((caller && !caller()) || !ue_wrap::prop::IsDescendantOfProp(actor))return false;
    ue_wrap::CachedObjRef ref,type;ref.Set(actor);type.Set(R::ClassOf(actor));const auto world=ue_wrap::world_identity::Generation();
    const auto current=[&]{return (!caller || caller()) && world==ue_wrap::world_identity::Generation() && TargetCurrent(ref,type);};
    const auto field=R::FindPropertyOffset(type.Raw(),L"physicsImpact"),actorSize=R::StructSize(type.Raw());
    if(!current() || field<0 || field>actorSize || int(sizeof(void*))>actorSize-field)return false;
    void* component=nullptr;std::memcpy(&component,static_cast<uint8_t*>(actor)+field,sizeof(component));
    if(!component || !R::IsLive(component))return false;
    ue_wrap::CachedObjRef componentRef,componentType;componentRef.Set(component);componentType.Set(R::ClassOf(component));
    int32_t off=-1;uint8_t mask=0;
    const bool have=R::FindBoolProperty(componentType.Raw(),L"isNotSawable",off,mask);
    const auto size=R::StructSize(componentType.Raw());
    if(!current() || !TargetCurrent(componentRef,componentType) || !have || !mask || off<0 || off>=size ||
        R::FindPropertyOffset(type.Raw(),L"physicsImpact")!=field || !current() || !TargetCurrent(componentRef,componentType))return false;
    void* latest=nullptr;std::memcpy(&latest,static_cast<uint8_t*>(actor)+field,sizeof(latest));
    return latest==component && !(static_cast<uint8_t*>(component)[off]&mask);
}

bool IsSwingValid(const std::wstring& held, float damage) {
    ue_wrap::weapon_catalog::Swing swing{};
    return std::isfinite(damage) && damage > 0.f && ue_wrap::weapon_catalog::Lookup(held, swing) &&
           swing.canSwing && damage <= swing.maxDamage + 0.01f;
}

template<class T> bool ReadArg(const SG::Call& c, const wchar_t* name, T& value,const std::function<bool()>& current={}) {
    if (!c.function || !c.locals || (current && !current())) return false;
    const int32_t off = R::FindParamOffset(c.function, name);
    if(current && !current())return false;
    const int32_t size = R::FunctionFrameSize(c.function);
    if ((current && !current()) || off < 0 || off > size || sizeof(T) > size - off) return false;
    std::memcpy(&value, c.locals + off, sizeof(T));
    return true;
}

struct InputSource {
    coop::net::Session* session=nullptr;uint64_t epoch=0;
    ue_wrap::CachedObjRef actor,type,player,function,owner;uint8_t slot=0;uint16_t person=0;uint32_t born=0;
    bool Current()const {
        const auto& row=coop::roster_ledger::Get(slot);
        return coop::prop_action_sync::Current(session,epoch) && session->role()==coop::net::Role::Client &&
            TargetCurrent(actor,type) && player.Get() && function.Get() && owner.Get() && R::OuterOf(function.Raw())==owner.Raw() &&
            coop::players::Registry::Get().Local()==player.Raw() &&
            coop::players::Registry::Get().LocalPeerId()==slot && row.playerNo==person && row.bornGeneration==born;
    }
};
InputSource CaptureInput(void* actor,void* function) {
    InputSource source;source.session=g_session.load();source.epoch=g_epoch;source.actor.Set(actor);source.type.Set(R::ClassOf(actor));
    source.player.Set(coop::players::Registry::Get().Local());source.slot=coop::players::Registry::Get().LocalPeerId();
    source.function.Set(function);source.owner.Set(function?R::OuterOf(function):nullptr);
    const auto& person=coop::roster_ledger::Get(source.slot);source.person=person.playerNo;source.born=person.bornGeneration;return source;
}

void EraseIntent(uint64_t revision) {std::erase_if(g_pending,[&](const QueuedIntent& row){return row.revision==revision;});}
void DrainIntents(coop::net::Session* s,uint64_t watched=0,bool* delivered=nullptr) {
    if(g_draining)return;
    g_draining=true;struct Guard{~Guard(){g_draining=false;}}guard;
    const auto epoch=g_epoch,now=NowMs();
    for(size_t work=0;work<8 && !g_pending.empty();++work) {
        if(!Current(s,epoch) || s->role()!=coop::net::Role::Client)return;
        const auto row=g_pending.front(); // owned across native key/send callbacks
        const auto current=[&]{return Current(s,epoch) && s->role()==coop::net::Role::Client && TargetCurrent(row.target,row.type) &&
            !g_pending.empty() && g_pending.front().revision==row.revision;};
        if(!current()){if(Current(s,epoch))EraseIntent(row.revision);else return;continue;}
        const auto key=row.payload.action==kWallDamage?ue_wrap::wall_build::Key(row.target.Raw()):ue_wrap::prop::GetKeyString(row.target.Raw());
        if(!current()){if(!Current(s,epoch))return;EraseIntent(row.revision);continue;}
        if(now<row.queuedAt || (row.payload.action!=kUnlockSafe && now-row.queuedAt>5000) ||
            key!=coop::net::StringFromWireKey(row.payload.key) ||
            (row.payload.action==kOpenMre && coop::hand_item::IsHandAxisActor(row.target.Raw()))) {EraseIntent(row.revision);continue;}
        const bool sent=s->SendReliableToSlot(0,coop::net::ReliableKind::PropActionIntent,&row.payload,sizeof(row.payload));
        if(!Current(s,epoch))return;
        if(!sent)return;
        if(delivered && row.revision==watched)*delivered=true;
        EraseIntent(row.revision);
    }
}

bool SendIntent(void* target, uint8_t action, float damage, const ue_wrap::FVector& impact, bool clean) {
    EnsureWorld();
    auto* s = g_session.load(std::memory_order_acquire);
    const auto epoch=g_epoch;
    if (!Current(s,epoch) || s->role() != coop::net::Role::Client || !target) return false;
    QueuedIntent queued;queued.target.Set(target);queued.type.Set(R::ClassOf(target));
    const auto current=[&]{return Current(s,epoch) && s->role()==coop::net::Role::Client && TargetCurrent(queued.target,queued.type);};
    const std::wstring key = action == kWallDamage ? ue_wrap::wall_build::Key(target) : ue_wrap::prop::GetKeyString(target);
    if (!current() || key.empty() || key == L"None" || key.size() > sizeof(coop::net::WireKey{}.data)) return false;
    coop::net::PropActionIntentPayload p{};
    coop::net::WireKeyFromString(key, p.key);
    if (coop::net::StringFromWireKey(p.key) != key) return false;
    p.action = action; p.damage = damage;
    p.impactX = impact.X; p.impactY = impact.Y; p.impactZ = impact.Z;
    p.clean = clean ? 1 : 0;
    // A new hit/open must not overtake an older intent waiting for queue space.
    if (g_pending.size() >= 256) return false;
    const bool first=g_pending.empty();queued.payload=p;queued.queuedAt=NowMs();queued.revision=++g_intentRevision;
    g_pending.push_back(queued);
    bool delivered=false;if(first)DrainIntents(s,queued.revision,&delivered);
    const bool retained=std::any_of(g_pending.begin(),g_pending.end(),[&](const QueuedIntent& row){return row.revision==queued.revision;});
    return current() && (delivered || retained);
}

SG::Verdict OnSafeOpened(const SG::Call& c) {
    auto* s=g_session.load();
    if(!s||!s->connected()||s->role()!=coop::net::Role::Client||
       !ue_wrap::safe::IsSafe(c.object)||!coop::net_pump::IsInAnnouncedWorld(c.object))return SG::Verdict::Run;
    // Both dial checkPass and damage's hacked() reach opened AFTER changing the
    // local lock. Keep the native dial/UI feedback; only the host creates loot.
    if(!c.fromOurCode&&!coop::hand_item::IsHandAxisActor(c.object)) {
        EnsureWorld();
        bool pending=false;
        for(const auto& item:g_pending)if(item.payload.action==kUnlockSafe&&item.target.Is(c.object)){pending=true;break;}
        if(!pending)SendIntent(c.object,kUnlockSafe,0.f,{},false);
    }
    return SG::Verdict::Cancel;
}
void AfterSafeOpened(const SG::Call& c) {
    auto* s=g_session.load();
    if(!s||!s->connected()||s->role()!=coop::net::Role::Host||!ue_wrap::safe::IsSafe(c.object))return;
    EnsureWorld();
    const auto epoch=g_epoch;ue_wrap::CachedObjRef actor,type;actor.Set(c.object);type.Set(R::ClassOf(c.object));
    // loadData restores the lock/password without calling opened again. The
    // existing record lane handles send retry, birth ordering and late join.
    const auto key=ue_wrap::prop::GetKeyString(c.object);
    if(!Current(s,epoch) || !TargetCurrent(actor,type) || key.empty()||key==L"None"||key.size()>31||(g_safePublish.size()>=64&&!g_safePublish.count(key)))return;
    auto& row=g_safePublish[key];row.actor=actor;row.type=type;row.retryAt=0;row.serial=++g_safeSerial;
}

void PlayMreRequestCue(void* target,const std::function<bool()>& current) {
    // Cancelling the client body also suppresses its native ripping sound. Keep
    // requester feedback, using the same cooked cue and small attenuation.
    static ue_wrap::CachedObjRef cue, attenuation;
    if(!current())return;
    if (!cue.Get()) cue.Set(R::FindObject(L"mreopen_Cue", L"SoundCue"));
    if(!current())return;
    if (!attenuation.Get()) attenuation.Set(R::FindObject(L"att_small", L"SoundAttenuation"));
    ue_wrap::FVector loc{};
    if (current() && cue.Get() && attenuation.Get() && ue_wrap::engine::TryGetActorLocation(target, loc) && current())
        ue_wrap::engine::PlaySoundAtLocation(target, cue.Get(), loc, attenuation.Get(), 1.f, 1.f);
}

bool TrackMreScope(const SG::Call& c,const wchar_t* verb,std::vector<MreScope>& rows) {
    std::erase_if(rows,[](const MreScope& row){return !ScopeCurrent(row);});
    if(!c.invocation || rows.size()>=64 || std::any_of(rows.begin(),rows.end(),[&](const MreScope& row){return row.actor.Raw()==c.object;}))return false;
    MreScope row;row.session=g_session.load();row.epoch=g_epoch;row.invocation=c.invocation;
    row.slot=coop::players::Registry::Get().LocalPeerId();if(!row.slot || row.slot>=coop::net::kMaxPeers)return false;
    const auto& person=coop::roster_ledger::Get(row.slot);row.person=person.playerNo;row.born=person.bornGeneration;if(!row.person)return false;
    row.actor.Set(c.object);row.type.Set(R::ClassOf(c.object));row.function.Set(c.function);row.owner.Set(c.function?R::OuterOf(c.function):nullptr);
    if(!ScopeCurrent(row) || R::FindDispatchFunctionCached(row.type.Raw(),verb)!=c.function ||
        !R::NameEquals(R::NameOf(row.owner.Raw()),L"prop_food_mre_C"))return false;
    rows.push_back(row);return true;
}
void AfterMreOpen(const SG::Call& c) {
    std::erase_if(g_handOpen,[&](const MreScope& row){return row.actor.Raw()==c.object && row.invocation==c.invocation;});
}
SG::Verdict OnMreSpawn(const SG::Call& c) {
    auto* s=g_session.load();if(!s || !s->connected() || s->role()!=coop::net::Role::Client)return SG::Verdict::Run;
    EnsureWorld();std::erase_if(g_handOpen,[](const MreScope& row){return !ScopeCurrent(row);});
    const bool hand=c.object && (c.object==coop::hand_item::LocalHandActor() ||
        std::any_of(g_handOpen.begin(),g_handOpen.end(),[&](const MreScope& row){return row.actor.Raw()==c.object && ScopeCurrent(row);}));
    return hand && TrackMreScope(c,L"spawn",g_mreSpawn)?SG::Verdict::Run:SG::Verdict::Cancel;
}
void AfterMreSpawn(const SG::Call& c) {
    std::erase_if(g_mreSpawn,[&](const MreScope& row){return row.actor.Raw()==c.object && row.invocation==c.invocation;});
}

SG::Verdict OnMreOpen(const SG::Call& c) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Client ||
        !c.object || !ue_wrap::prop::IsDescendantOfProp(c.object))
        return SG::Verdict::Run;
    EnsureWorld();
    if(!c.locals || !c.function)return SG::Verdict::Cancel;
    const int32_t action = R::FindParamOffset(c.function, L"action");
    const int32_t player = R::FindParamOffset(c.function, L"player");
    const int32_t frame = R::FunctionFrameSize(c.function);
    if (action < 0 || action >= frame)return SG::Verdict::Cancel;
    if(c.locals[action]!=4)return SG::Verdict::Run;
    if(player < 0 || player > frame || sizeof(void*) > frame - player)return SG::Verdict::Cancel;
    void* source = nullptr;
    std::memcpy(&source, c.locals + player, sizeof(source));
    if (!source || source != coop::players::Registry::Get().Local()) return SG::Verdict::Cancel;
    // Hand items have a separate generation/identity lane. Do not cancel their
    // native action: prop_spawn_authoring admits this hand MRE's own contents.
    if (c.object==coop::hand_item::LocalHandActor())
        return TrackMreScope(c,L"actionOptionIndex",g_handOpen)?SG::Verdict::Run:SG::Verdict::Cancel;
    if(c.fromOurCode || coop::hand_item::IsHandAxisActor(c.object))return SG::Verdict::Cancel;
    if(std::any_of(g_pending.begin(),g_pending.end(),[&](const QueuedIntent& row){return row.payload.action==kOpenMre && row.target.Is(c.object);}))return SG::Verdict::Cancel;
    const auto epoch=g_epoch;ue_wrap::CachedObjRef actor,type;actor.Set(c.object);type.Set(R::ClassOf(c.object));
    const auto current=[&]{return Current(s,epoch) && TargetCurrent(actor,type);};
    const uint64_t now = NowMs();
    // Press/release or another click before the host destroys the package must
    // not repeat the request cue. A denied/lost request can be tried again soon.
    if (g_lastMreRequest.Is(c.object) && now >= g_lastMreRequestAt && now - g_lastMreRequestAt < 750)
        return SG::Verdict::Cancel;
    const uint8_t slot = coop::players::Registry::Get().LocalPeerId();
    if (slot > 0 && slot < coop::net::kMaxPeers && TakeToken(slot) &&
        SendIntent(c.object, kOpenMre, 0.f, {}, false)) {
        if(!current())return SG::Verdict::Cancel;
        g_lastMreRequest=actor;
        g_lastMreRequestAt = now;
        PlayMreRequestCue(c.object,current);
    }
    return SG::Verdict::Cancel;
}

SG::Verdict OnDamage(const SG::Call& c) {
    auto* s = g_session.load(std::memory_order_acquire);
    const bool wall = c.object && ue_wrap::placed_save::Type(c.object) == ue_wrap::placed_save::Kind::Wall;
    if (!s || !s->connected() || s->role() != coop::net::Role::Client || !c.object ||
        (!wall && !ue_wrap::prop::IsDescendantOfProp(c.object)))
        return SG::Verdict::Run;
    EnsureWorld();
    const auto input=CaptureInput(c.object,c.function);
    void* source = nullptr;
    if (!ReadArg(c, L"actor", source,[&]{return input.Current();})) return SG::Verdict::Cancel;
    void* localPlayer = input.player.Raw();
    if (!localPlayer || source != localPlayer) return SG::Verdict::Run;
    if(c.fromOurCode || !input.Current())return SG::Verdict::Cancel;
    // Once a client-local player authored this hit, the prop must not execute
    // its native mutation on that client. That includes malformed parameters,
    // cooldown rejection, and a temporarily full network queue; running locally
    // in those cases creates precisely the one-peer break/destruction state
    // this authority lane is meant to prevent.
    float damage = 0.f;
    ue_wrap::FVector impact{};
    if (!ReadArg(c, L"damage", damage,[&]{return input.Current();}) || !ReadArg(c, L"impact", impact,[&]{return input.Current();})) return SG::Verdict::Cancel;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    // A local-player hit in a live client session must never fall back to native
    // client-side mutation just because the peer slot is not ready yet.
    if (localSlot == 0 || localSlot >= coop::net::kMaxPeers) return SG::Verdict::Cancel;
    const std::wstring held = coop::hand_item::HeldItem(localSlot);
    if (!TakeToken(localSlot) || !IsSwingValid(held, damage) || !std::isfinite(impact.X) ||
        !std::isfinite(impact.Y) || !std::isfinite(impact.Z) ||
        impact.X * impact.X + impact.Y * impact.Y + impact.Z * impact.Z > 10000000000.f)
        return SG::Verdict::Cancel;
    if (!input.Current() || (wall && !coop::net_pump::IsInAnnouncedWorld(c.object))) return SG::Verdict::Cancel;
    SendIntent(c.object, wall ? kWallDamage : kDamage, damage, impact, false);
    return SG::Verdict::Cancel;
}

SG::Verdict OnSlice(const SG::Call& c) {
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Client || !c.object ||
        !ue_wrap::prop::IsDescendantOfProp(c.object))
        return SG::Verdict::Run;
    EnsureWorld();
    const auto input=CaptureInput(c.object,c.function);
    if(c.fromOurCode || !input.Current())return SG::Verdict::Cancel;
    if (localSlot == 0 || localSlot >= coop::net::kMaxPeers) return SG::Verdict::Cancel;
    if (!coop::hand_item::LastHeldClassIs(localSlot, L"prop_hacksaw_C")) return SG::Verdict::Run;
    uint8_t cleanValue = 0;
    if (!ReadArg(c, L"clean", cleanValue,[&]{return input.Current();}) || cleanValue > 1) return SG::Verdict::Cancel;
    const bool clean = cleanValue != 0;
    const ue_wrap::FVector zero{};
    if (!Sawable(c.object,[&]{return input.Current();}) || !input.Current() || !TakeToken(localSlot)) return SG::Verdict::Cancel;
    SendIntent(c.object, kSlice, 0.f, zero, clean);
    return SG::Verdict::Cancel;
}

void RunDamage(void* target, void* player, float damage, const ue_wrap::FVector& impact,const std::function<bool()>& current) {
    if(!current || !current())return;
    ue_wrap::CachedObjRef actor,type;actor.Set(target);type.Set(R::ClassOf(target));
    // Most prop subclasses inherit this event. Exact-owner lookup silently
    // drops their hits after the client has already cancelled native damage.
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(target), L"addDamage");
    ue_wrap::CachedObjRef function,owner;function.Set(fn);owner.Set(fn?R::OuterOf(fn):nullptr);
    const auto prepared=[&]{return current() && TargetCurrent(actor,type) && function.Get() && owner.Get() &&
        R::OuterOf(fn)==owner.Raw() && R::FindDispatchFunctionCached(type.Raw(),L"addDamage")==fn;};
    if(!prepared())return;
    ue_wrap::ParamFrame frame(fn);
    if(!prepared())return;
    ue_wrap::CachedObjRef mesh,meshType;mesh.Set(ue_wrap::prop::GetStaticMesh(target));if(!mesh.Get())return;meshType.Set(R::ClassOf(mesh.Raw()));
    const auto ready=[&]{return prepared() && TargetCurrent(mesh,meshType) && ue_wrap::prop::GetStaticMesh(target)==mesh.Raw();};
    ue_wrap::FVector loc{};
    if (!ready() || !frame.valid() || !ue_wrap::engine::TryGetActorLocation(target, loc) || !ready() ||
        !std::isfinite(loc.X) || !std::isfinite(loc.Y) || !std::isfinite(loc.Z) ||
        !frame.Set<void*>(L"actor", player) || !ready() || !frame.Set<float>(L"damage", damage) || !ready() ||
        !frame.Set<ue_wrap::FVector>(L"impact", impact) || !ready() || !frame.Set<bool>(L"skipSetting", false) || !ready() ||
        !ue_wrap::hit_result::WriteWhile(frame, L"hit", target, mesh.Raw(), loc,ready) || !ready())return;
    ue_wrap::Call(target, frame);
}

void RunSlice(void* target, bool clean,const std::function<bool()>& current) {
    if(!current || !current())return;
    ue_wrap::CachedObjRef actor,type;actor.Set(target);type.Set(R::ClassOf(target));
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(target), L"slice");
    ue_wrap::CachedObjRef function,owner;function.Set(fn);owner.Set(fn?R::OuterOf(fn):nullptr);
    const auto prepared=[&]{return current() && TargetCurrent(actor,type) && function.Get() && owner.Get() &&
        R::OuterOf(fn)==owner.Raw() && R::FindDispatchFunctionCached(type.Raw(),L"slice")==fn;};
    if (!prepared()) return;
    ue_wrap::ParamFrame frame(fn);
    if (prepared() && frame.valid() && frame.Set<bool>(L"clean", clean) && prepared()) ue_wrap::Call(target, frame);
}

void RunMreOpen(void* target, void* player,const std::function<bool()>& current) {
    if(!current())return;
    std::erase_if(g_mreAttempts,[](const MreAttempt& row){return !TargetCurrent(row.actor,row.type);});
    if(g_mreAttempts.size()>=64 || std::any_of(g_mreAttempts.begin(),g_mreAttempts.end(),[&](const MreAttempt& row){return row.actor.Raw()==target;}))return;
    MreAttempt attempt;attempt.actor.Set(target);attempt.type.Set(R::ClassOf(target));attempt.revision=++g_mreRevision;
    g_mreAttempts.push_back(attempt);
    // Preparation may reenter this receiver. Only a pre-dispatch refusal frees
    // this actor for another attempt; an admitted native call may have made loot.
    struct AttemptScope {uint64_t revision;~AttemptScope(){std::erase_if(g_mreAttempts,[&](const MreAttempt& row){return row.revision==revision && !row.admitted;});}} scope{attempt.revision};
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(target), L"actionOptionIndex");
    void* owner = fn ? R::OuterOf(fn) : nullptr;
    // Only the verified MRE opening body (including inherited use). Arbitrary
    // prop action 4 can eat, toggle or summon things and must not run here.
    ue_wrap::CachedObjRef functionRef,ownerRef;functionRef.Set(fn);ownerRef.Set(owner);
    const auto prepared=[&]{return current() && TargetCurrent(attempt.actor,attempt.type) && functionRef.Get() && ownerRef.Get() &&
        R::OuterOf(fn)==owner && R::FindDispatchFunctionCached(attempt.type.Raw(),L"actionOptionIndex")==fn;};
    if (!prepared() || !R::NameEquals(R::NameOf(owner), L"prop_food_mre_C")) return;
    ue_wrap::ParamFrame frame(fn);
    if(!prepared())return;
    ue_wrap::CachedObjRef mesh;mesh.Set(ue_wrap::prop::GetStaticMesh(target));
    const auto ready=[&]{return prepared() && mesh.Get() && ue_wrap::prop::GetStaticMesh(target)==mesh.Raw();};
    ue_wrap::FVector loc{};
    if (!ready() || !frame.valid() || !ue_wrap::engine::TryGetActorLocation(target, loc) || !ready() ||
        !frame.Set<void*>(L"player", player) || !ready() || !frame.Set<uint8_t>(L"action", 4) || !ready() ||
        !frame.Set<void*>(L"lookAtComponent", mesh.Raw()) || !ready() ||
        !ue_wrap::hit_result::Write(frame, L"hit", target, mesh.Raw(), loc) || !ready())
        return;
    // Native spawn() creates the random contents once, then destroys the packet.
    // The host birth watcher publishes every resulting prop with its final data.
    const auto row=std::find_if(g_mreAttempts.begin(),g_mreAttempts.end(),[&](const MreAttempt& row){return row.revision==attempt.revision;});
    if(row==g_mreAttempts.end())return;row->admitted=true;
    ue_wrap::Call(target, frame);
}
} // namespace

bool IsLocalMreOutput(void* actor,void* callerFunction) {
    EnsureWorld();std::erase_if(g_mreSpawn,[](const MreScope& row){return !ScopeCurrent(row);});
    for(const auto& row:g_mreSpawn)if(row.actor.Raw()==actor && row.function.Is(callerFunction) && ScopeCurrent(row) &&
        R::FindDispatchFunctionCached(row.type.Raw(),L"spawn")==callerFunction)return true;
    return false;
}

void Install(coop::net::Session* session) {
    if (g_session.load(std::memory_order_acquire) != session) OnDisconnect();
    g_session.store(session, std::memory_order_release);
    if (g_watchesInstalled) return;
    // These Blueprint events are overridden on several prop subclasses. A
    // class-scoped watch on prop_C only sees the base UFunction body, so a
    // client could still run (and locally destroy) a subclass without ever
    // sending its intent. Match the function name globally and keep the strict
    // prop/source/tool checks in the callbacks below.
    const bool a = SG::WatchName(kDamageFn, kDamageTag, &OnDamage, nullptr);
    const bool b = SG::WatchName(kSliceFn, kSliceTag, &OnSlice, nullptr);
    const bool c = SG::WatchClassName(L"prop_food_mre_C", L"actionOptionIndex", kMreTag, &OnMreOpen, &AfterMreOpen);
    const bool d = SG::WatchClassName(L"prop_safe_C", L"opened", 0x53414645, &OnSafeOpened, &AfterSafeOpened);
    const bool e = SG::WatchClassName(L"prop_food_mre_C", L"spawn", kMreTag, &OnMreSpawn, &AfterMreSpawn);
    g_watchesInstalled = a && b && c && d && e;
    if (g_watchesInstalled)
        UE_LOGI("prop_action_sync: client damage/slice, world MRE opening and safe completion are relayed to host");
    else UE_LOGW("prop_action_sync: prop action watches failed (damage=%d slice=%d mre=%d safe=%d)", a ? 1 : 0, b ? 1 : 0, c ? 1 : 0, d ? 1 : 0);
}

void Tick() {
    EnsureWorld();
    auto* s = g_session.load(std::memory_order_acquire);
    if(s&&s->connected()&&s->role()==coop::net::Role::Host) {
        std::vector<std::wstring> keys;for(const auto& row:g_safePublish)keys.push_back(row.first);
        const auto epoch=g_epoch;const auto now=NowMs();
        for(const auto& key:keys) {
            if(!Current(s,epoch) || s->role()!=coop::net::Role::Host)return;
            auto it=g_safePublish.find(key);if(it==g_safePublish.end())continue;
            const auto row=it->second;
            const auto current=[&]{const auto latest=g_safePublish.find(key);return Current(s,epoch) && s->role()==coop::net::Role::Host &&
                latest!=g_safePublish.end() && latest->second.serial==row.serial && TargetCurrent(row.actor,row.type);};
            if(!TargetCurrent(row.actor,row.type)){g_safePublish.erase(it);continue;}
            void* actor=row.actor.Raw();const auto liveKey=ue_wrap::prop::GetKeyString(actor);
            if(!Current(s,epoch))return;
            if(!current())continue;
            if(liveKey!=key){g_safePublish.erase(key);continue;}
            if(now<row.retryAt)continue;
            g_safePublish[key].retryAt=now+500;
            const bool sent=coop::prop_save_data::Publish(s,actor,key);
            if(!Current(s,epoch))return;
            it=g_safePublish.find(key);
            if(sent&&current())g_safePublish.erase(it);
        }
    }
    if (!s || !s->connected() || s->role() != coop::net::Role::Client) {
        g_pending.clear();
        return;
    }
    DrainIntents(s);
}

void OnReliable(const coop::net::PropActionIntentPayload& input, uint8_t senderSlot) {
    EnsureWorld();
    const auto p=input;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host || senderSlot == 0 ||
        senderSlot >= coop::net::kMaxPeers || p.key.len>sizeof(p.key.data) ||
        (p.action != kDamage && p.action != kSlice && p.action != kOpenMre && p.action != kUnlockSafe && p.action != kWallDamage) ||
        p.clean > 1 || p._pad[0] || p._pad[1] || !std::isfinite(p.damage) ||
        !std::isfinite(p.impactX) || !std::isfinite(p.impactY) || !std::isfinite(p.impactZ)) return;
    if(!s->IsSlotWorldReady(senderSlot))return;
    const auto epoch=g_epoch;const auto person=coop::roster_ledger::Get(senderSlot);
    if(!person.playerNo)return;
    const auto source=[&]{const auto& live=coop::roster_ledger::Get(senderSlot);return Current(s,epoch) && s->role()==coop::net::Role::Host &&
        s->IsSlotWorldReady(senderSlot) && live.playerNo==person.playerNo && live.bornGeneration==person.bornGeneration;};
    const std::wstring key = coop::net::StringFromWireKey(p.key);
    void* target = key.empty() ? nullptr : (p.action == kWallDamage ? ue_wrap::keyed_objects::Resolve(key.c_str()) :
        coop::prop_element_tracker::ResolveLiveActorByKey(key, nullptr));
    if (!source() || !target) return;
    if (p.action == kWallDamage ? ue_wrap::placed_save::Type(target) != ue_wrap::placed_save::Kind::Wall :
        !ue_wrap::prop::IsDescendantOfProp(target)) return;
    ue_wrap::CachedObjRef live,type;live.Set(target);type.Set(R::ClassOf(target));
    const auto current=[&]{return source() && TargetCurrent(live,type);};
    auto* remote = coop::players::Registry::Get().Puppet(senderSlot);
    void* player = remote && remote->valid() ? remote->GetActor() : nullptr;
    ue_wrap::CachedObjRef playerRef;playerRef.Set(player);
    const auto author=[&]{auto* r=coop::players::Registry::Get().Puppet(senderSlot);return current() && playerRef.Get() && r && r->valid() && r->GetActor()==player;};
    const auto addressed=[&]{if(!author())return false;const auto liveKey=p.action==kWallDamage?ue_wrap::wall_build::Key(target):ue_wrap::prop::GetKeyString(target);
        return author() && liveKey==key;};
    const float impactLengthSq = p.impactX * p.impactX + p.impactY * p.impactY + p.impactZ * p.impactZ;
    if (!std::isfinite(impactLengthSq) || impactLengthSq > 10000000000.f) return;
    const auto auth = coop::element::IntentTarget::ForClientIntent(*s, senderSlot, kReachUU).Authorize(target);
    if(!current())return;
    if (!auth) {
        // The requester already saw native dial success. A rejected completion
        // must restore the authority's lock instead of leaving a local unlock.
        if(p.action==kUnlockSafe&&ue_wrap::safe::IsSafe(target))
            coop::prop_save_data::PublishToSlot(s,senderSlot,target,key);
        return;
    }
    if (!author() || !TakeToken(senderSlot)) return;
    if (p.action == kDamage || p.action == kWallDamage) {
        const std::wstring held = coop::hand_item::HeldItem(senderSlot);
        if (p.clean || !IsSwingValid(held, p.damage)) return;
        const auto armed=[&]{return addressed() && IsSwingValid(coop::hand_item::HeldItem(senderSlot),p.damage) && author();};
        if (!armed()) return;
        if (p.action == kWallDamage) ue_wrap::wall_build::DamageWhile(target, p.damage,armed);
        else RunDamage(target, player, p.damage, {p.impactX, p.impactY, p.impactZ},armed);
    } else if (p.action == kSlice) {
        if (p.damage != 0.f || !coop::hand_item::LastHeldClassIs(senderSlot, L"prop_hacksaw_C") ||
            !Sawable(target)) return;
        const auto sawing=[&]{return addressed() && coop::hand_item::LastHeldClassIs(senderSlot,L"prop_hacksaw_C") &&
            Sawable(target,author) && author();};
        if(!sawing())return;
        RunSlice(target, p.clean != 0,sawing);
    } else if (p.action == kOpenMre) {
        if (p.damage != 0.f || p.clean || p.impactX != 0.f || p.impactY != 0.f || p.impactZ != 0.f ||
            coop::hand_item::IsHandAxisActor(target)) return;
        const auto opening=[&]{if(!author())return false;const auto liveKey=ue_wrap::prop::GetKeyString(target);
            return author() && liveKey==key && !coop::hand_item::IsHandAxisActor(target);};
        RunMreOpen(target, player,opening);
    } else if (p.action == kUnlockSafe) {
        if(p.damage!=0.f||p.clean||p.impactX!=0.f||p.impactY!=0.f||p.impactZ!=0.f||
           coop::hand_item::IsHandAxisActor(target)||!ue_wrap::safe::IsSafe(target))return;
        ue_wrap::CachedObjRef live;live.Set(target);
        if(ue_wrap::safe::Unlock(target)&&author()&&live.Get()) {
            SG::Call opened{};opened.object=target;AfterSafeOpened(opened);
        }
    }
}
void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_pending.clear();
    ++g_epoch;g_source=nullptr;g_serial=0;g_running=g_connected=false;
    g_handOpen.clear();g_mreSpawn.clear();g_mreAttempts.clear();
    g_safePublish.clear();++g_safeSerial;
    g_lastMreRequest.Reset();
    g_lastMreRequestAt = 0;
    g_world = 0;
    for (auto& bucket : g_buckets) bucket = {};
}
} // namespace coop::prop_action_sync
