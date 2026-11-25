// coop/props/prop_stick_sync.cpp -- see coop/props/prop_stick_sync.h for the design + RE.

#include "coop/props/prop_stick_sync.h"

#include "coop/element/mirror_manager.h"
#include "coop/element/prop.h"
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/local_streams.h"  // CurrentHoldGen, the hold a stick ends
#include "coop/player/local_prop_hold.h"
#include "coop/player/hand_item.h"
#include "coop/props/prop_drive_stream.h"  // IsParked: the host's channel moves an unstuck copy
#include "coop/props/bracket_position_wait.h"  // KeepIfWaiting: a stick for a spawn that waits for the bracket's end
#include "coop/props/prop_drive_host.h"  // OverrideClientSyncer: a replay that writes the copy ends a client's row on it
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_lifecycle.h"
#include "coop/props/prop_key_index.h"  // a key on the wire to a live actor
#include "coop/props/remote_prop.h"
#include "ue_wrap/engine/actor_end_play.h"
#include "ue_wrap/engine/engine_attach.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_physics.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace coop::prop_stick_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace E  = ue_wrap::engine;
namespace PT = coop::prop_element_tracker;
namespace sg = ue_wrap::script_gate;

UE_FACT_FN(kRowCompUbergraph, "prop_stick_sync", L"comp_wallAttachable_C",
           L"ExecuteUbergraph_comp_wallAttachable", Declared);
UE_FACT_FN(kRowUnstick, "prop_stick_sync", L"comp_wallAttachable_C", L"unstick", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};
// The observer and unstick verb latch independently. Stick application uses the exact support
// component carried on the wire, so it never dispatches forceStick on a receiver.
std::atomic<bool> g_stickInstalled{false};
std::atomic<bool> g_unstickInstalled{false};
std::atomic<bool> g_unstickDisabled{false};

coop::net::Session* LoadSession() {
    return g_session.load(std::memory_order_acquire);
}

// ---- resolved engine refs, written on the game thread by the install -- Install, or on demand when
// a join's converge or a live message needs the lane first -- before the observer and the watch
// register; read-only afterwards. The component class is loaded at the main menu and lives for the
// process (coop/dev/class_lifetime_probe measures it), so neither the class nor the functions below
// go stale across a world change.
void* g_compClass        = nullptr;  // comp_wallAttachable_C
void* g_uberFn           = nullptr;  // comp_wallAttachable_C::ExecuteUbergraph_comp_wallAttachable
void* g_unstickFn        = nullptr;  // comp_wallAttachable_C::unstick(bool withTool)
int32_t g_entryParamOff  = -1;       // ExecuteUbergraph 'EntryPoint' int32 param offset
int32_t g_withToolOff    = -1;       // unstick 'withTool' bool param offset
int32_t g_compPropOff    = -1;       // comp_wallAttachable_C::prop (Aprop_C*) field offset
// NOTE this module never dispatches a prop's init() itself: init is overridden along the camera
// lineage, and the component's own unstick runs the right override inside the Blueprint. A stick
// writes the replicated flags and attaches to the sender's exact support component.

// The commit's ubergraph entry: offset 45 in ExecuteUbergraph_comp_wallAttachable, from
// the kismet bytecode. The byte offset is part of the cooked BP the same way the keypad
// and door entry constants are; a game update that recooks the BP shifts it, and the
// install log prints it so a silent no-fire is diagnosable.
constexpr int32_t kStickCommitEntry = 45;

constexpr int kTagUnstick = 0x554E5354;  // 'UNST'
constexpr int kTagPry = 0x50525953;  // 'PRYS'
bool g_pryWatchInstalled = false;

// Inside ReplayUnstick: the copy's own unstick is this peer taking another's outcome, and is not
// mirrored back. Game thread, as the gate's callbacks are.
int g_replayDepth = 0;

// The offset of each class's comp_wallAttachable variable, or -1, resolved once per class. Every
// owner the game has names the component so: the wall-attachable lineage and the plasma TV, whose
// grab preludes both call comp_wallAttachable->unstick. The class is held by slot and serial, so a
// class freed and another loaded at its address resolves again. Game thread.
struct CompOffset {
    ue_wrap::CachedObjRef cls;
    int32_t off = -1;
};
std::unordered_map<void*, CompOffset> g_compOffsets;

// The on-demand install runs once a session; Install keeps retrying after it.
bool g_onDemandTried = false;

// The wall-attach component `actor` carries, checked for its class, or null. The class comes from the
// object index, so a joiner's first snapshot, which can arrive before Install runs, needs no install.
void* WallAttachCompOf(void* actor) {
    UE_ASSERT_GAME_THREAD("prop_stick_sync::WallAttachCompOf");
    if (!actor) return nullptr;
    void* cls = R::ClassOf(actor);
    if (!cls) return nullptr;
    CompOffset& entry = g_compOffsets[cls];
    if (!entry.cls.Alive()) {
        entry.cls.Set(cls);
        entry.off = R::FindPropertyOffset(cls, L"comp_wallAttachable");
    }
    if (entry.off < 0) return nullptr;
    void* comp = *reinterpret_cast<void* const*>(reinterpret_cast<uint8_t*>(actor) + entry.off);
    if (!comp || !R::IsLive(comp)) return nullptr;
    void* compCls = R::ClassOf(comp);
    if (!compCls) return nullptr;
    if (!g_compClass) g_compClass = ue_wrap::object_index::ClassByName(L"comp_wallAttachable_C");
    if (!g_compClass) return nullptr;
    return R::IsDescendantOfAny(compCls, &g_compClass, 1) ? comp : nullptr;
}

// Ask only after WallAttachCompOf populated the class cache. A declared but
// temporarily missing/unresolved component is not an ordinary non-component prop.
bool HasWallAttachMember(void* actor) {
    const auto it = g_compOffsets.find(R::ClassOf(actor));
    return it != g_compOffsets.end() && it->second.off >= 0;
}

// ---- pending list ----------------------------------------------------------------
// The observers only RECORD; Tick() (game thread) verifies + broadcasts on the NEXT net-pump pass.
// Same record-then-act shape as kerfur_convert.
//
// A stick: NO settle delay. frozen/static are already final when the POST observer fires (the
// commit body ran inside the observed dispatch), and the actor's current parent component is the
// exact support the receiver will attach to. A delay re-opens the
// release-beats-stick window: the hold breaks 0-100 ms after the commit and that PropRelease must
// not arrive before the PropStickState. GNS orders reliable delivery WITHIN a lane, so the pair is
// FIFO only because both ride the one Default lane (LaneForKind in
// coop/net/session_lanes.h). Draining next pass makes the order structural as well: TickGameplay
// runs before local_streams' release edge. At the host hop the stick is relayed on the net thread at its receipt and the
// release from the game thread after its dispatch, so the stick-then-release order holds; the inverse pair (a release,
// then a stick after a flight) is benign.
struct Pending {
    ue_wrap::CachedObjRef prop;
    bool    unstick = false;
    uint64_t queuedAt = 0;
    uint32_t worldGeneration = 0;
};
constexpr int kMaxPending = 8;
std::mutex g_pendingMutex;
Pending g_pending[kMaxPending];
int g_pendingCount = 0;

struct PendingSupport {
    coop::net::PropStickStatePayload payload{};
    uint8_t senderPeerSlot = 0xFF;
    uint64_t queuedAt = 0;
    uint32_t worldGeneration = 0;
};
constexpr int kMaxPendingSupport = 64;
std::mutex g_pendingSupportMutex;
PendingSupport g_pendingSupport[kMaxPendingSupport];
int g_pendingSupportCount = 0;

struct SupportAttachment {
    ue_wrap::CachedObjRef prop;
    ue_wrap::CachedObjRef support;
    uint32_t worldGeneration = 0;
    bool used = false;
    bool retire = false;
};
constexpr int kMaxSupportAttachments = 256;
SupportAttachment g_supportAttachments[kMaxSupportAttachments];
bool g_supportSinkInstalled = false;

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void Record(void* prop, bool unstick) {
    Pending p{};
    p.prop.Set(prop);
    p.unstick = unstick;
    p.queuedAt = NowMs();
    p.worldGeneration = ue_wrap::world_identity::Generation();
    std::lock_guard<std::mutex> lk(g_pendingMutex);
    // Dedupe a re-fired record for the same prop and edge still settling (the trace can re-enter
    // 45 after a failed first pass) -- keep the earliest.
    for (int i = 0; i < g_pendingCount; ++i) {
        if (g_pending[i].prop.Is(prop) && g_pending[i].unstick == unstick) return;
    }
    if (g_pendingCount >= kMaxPending) {
        static std::atomic<uint32_t> sDropped{0};
        UE_LOGW("prop_stick_sync: pending full -- dropping a %s (#%u)", unstick ? "unstick" : "stick commit",
                sDropped.fetch_add(1, std::memory_order_relaxed) + 1);
        return;
    }
    g_pending[g_pendingCount++] = p;
}

// POST observer on ExecuteUbergraph_comp_wallAttachable. Fires only for
// LATENT resumes (PE-dispatched): the 10 Hz sticking() poll while a wall-
// attachable is held, the re-arm entries, and the stick COMMIT (45). Cheap
// EntryPoint gate first; memory reads + the leaf mutex only (observer
// thread contract).
void OnCompUbergraphPost(void* self, void* /*function*/, void* params) {
    // Capturing actor world/serial belongs to the game thread, like the latent
    // commit itself. Do not walk an actor's Outer chain from a worker callback.
    if (GT::IsDefinitelyOffGameThread()) return;
    if (!self || !params || g_entryParamOff < 0 || g_compPropOff < 0) return;
    const int32_t entry =
        *reinterpret_cast<const int32_t*>(reinterpret_cast<uint8_t*>(params) + g_entryParamOff);
    if (entry != kStickCommitEntry) return;
    void* prop = *reinterpret_cast<void* const*>(
        reinterpret_cast<uint8_t*>(self) + g_compPropOff);
    if (!prop) return;
    auto* s = LoadSession();
    if (!s || !s->running() || !s->connected()) return;  // SP: nothing to mirror
    Record(prop, /*unstick=*/false);
}

// The script gate's pre on comp_wallAttachable_C::unstick, whatever called it: a stuck prop is
// recorded, and the next pass broadcasts it if the body freed it. That pass is also after whatever
// the calling Blueprint did next, which for a pry is the kick (crowbarOpen sets the angular velocity
// once unstick returns), so the broadcast carries the velocity the prop really left the wall with.
// A prop already free has nothing to mirror, and neither has this peer's own replay.
sg::Verdict OnUnstickPre(const sg::Call& c) {
    if (g_replayDepth > 0 || !c.object || g_compPropOff < 0) return sg::Verdict::Run;
    void* prop = *reinterpret_cast<void* const*>(reinterpret_cast<uint8_t*>(c.object) + g_compPropOff);
    if (!prop || !(ue_wrap::prop::IsFrozen(prop) || ue_wrap::prop::IsStatic(prop))) return sg::Verdict::Run;
    auto* s = LoadSession();
    if (!s || !s->running() || !s->connected()) return sg::Verdict::Run;
    Record(prop, /*unstick=*/true);
    return sg::Verdict::Run;
}

// Some pryable props implement crowbarOpen without comp_wallAttachable. Observe the
// actual overriding body too; Tick only publishes if the body really freed the prop.
// A pryable that does use the component is deduplicated with OnUnstickPre.
sg::Verdict OnPryPre(const sg::Call& c) {
    if (g_replayDepth || !c.object ||
        !ue_wrap::prop::IsDescendantOfProp(c.object) ||
        !(ue_wrap::prop::IsStatic(c.object) || ue_wrap::prop::IsFrozen(c.object)))
        return sg::Verdict::Run;
    auto* s = LoadSession();
    if (s && s->running() && s->connected()) Record(c.object, true);
    return sg::Verdict::Run;
}

// The prop by key, its eid as the fallback, and its pose, for either edge; the result says what it
// lacked when it could not be filled.
enum class Fill { Ok, NoIdentity, NoPose };

Fill FillIdentityAndPose(coop::net::PropStickStatePayload& p, void* prop, std::wstring& keyW) {
    keyW = ue_wrap::prop::GetInteractableKeyString(prop);
    p.key.len = 0;
    if (!keyW.empty() && keyW != L"None") {
        for (size_t k = 0; k < keyW.size() && k < 31; ++k)
            p.key.data[p.key.len++] = static_cast<char>(keyW[k]);
    }
    const coop::element::ElementId eid = PT::GetPropElementIdForActor(prop);
    p.elementId = (eid == coop::element::kInvalidId) ? 0u : static_cast<uint32_t>(eid);
    if (p.key.len == 0 && p.elementId == 0) return Fill::NoIdentity;
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    if (!E::TryGetActorLocation(prop, loc) || !E::TryGetActorRotation(prop, rot)) return Fill::NoPose;
    p.locX = loc.X; p.locY = loc.Y; p.locZ = loc.Z;
    p.rotPitch = rot.Pitch; p.rotYaw = rot.Yaw; p.rotRoll = rot.Roll;
    return Fill::Ok;
}

bool CopyWireKey(coop::net::WireKey& out, const std::wstring& value, bool allowEmpty) {
    if (value.empty()) return allowEmpty;
    if (value.size() > sizeof(out.data)) return allowEmpty;
    out.len = static_cast<uint8_t>(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const wchar_t c = value[i];
        if (c < 0x21 || c > 0x7E) {
            if (allowEmpty) {
                out.len = 0;
                return true;
            }
            return false;
        }
        out.data[i] = static_cast<char>(c);
    }
    return true;
}

bool CopyWireClass(coop::net::WireClassName& out, const std::wstring& value) {
    if (value.empty() || value.size() > sizeof(out.data)) return false;
    out.len = static_cast<uint8_t>(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const wchar_t c = value[i];
        if (c < 0x21 || c > 0x7E) return false;
        out.data[i] = static_cast<char>(c);
    }
    return true;
}

bool DecodeWireKey(const coop::net::WireKey& wire, std::wstring& out, bool allowEmpty) {
    if (wire.len > sizeof(wire.data) || (!allowEmpty && wire.len == 0)) return false;
    out.clear();
    out.reserve(wire.len);
    for (uint8_t i = 0; i < wire.len; ++i) {
        const unsigned char c = static_cast<unsigned char>(wire.data[i]);
        if (c < 0x21 || c > 0x7E) return false;
        out.push_back(static_cast<wchar_t>(c));
    }
    return true;
}

bool DecodeWireClass(const coop::net::WireClassName& wire, std::wstring& out) {
    if (wire.len == 0 || wire.len > sizeof(wire.data)) return false;
    out.clear();
    out.reserve(wire.len);
    for (uint8_t i = 0; i < wire.len; ++i) {
        const unsigned char c = static_cast<unsigned char>(wire.data[i]);
        if (c < 0x21 || c > 0x7E) return false;
        out.push_back(static_cast<wchar_t>(c));
    }
    return true;
}

bool ValidSupportIdentity(const coop::net::PropStickStatePayload& p) {
    std::wstring key, cls, actor, component;
    return p.elementId < coop::element::kMaxElements &&
           (p.elementId != 0 || p.key.len != 0) &&
           DecodeWireKey(p.key, key, p.elementId == 0) &&
           (p.support.elementId == 0 || p.support.elementId < coop::element::kMaxElements) &&
           DecodeWireClass(p.support.className, cls) &&
           DecodeWireKey(p.support.actorName, actor, p.support.elementId != 0) &&
           DecodeWireKey(p.support.componentName, component, false);
}

bool FillSupportIdentity(coop::net::PropStickStatePayload& p, void* prop) {
    void* supportActor = nullptr;
    void* component = nullptr;
    if (!E::GetAttachParentActor(prop, supportActor) || !supportActor ||
        !E::GetAttachParentComponent(prop, component) || !component ||
        R::OuterOf(component) != supportActor)
        return false;

    void* const currentWorld = ue_wrap::world_identity::CurrentWorld();
    if (!currentWorld || ue_wrap::world_identity::WorldOf(supportActor) != currentWorld) return false;
    void* const actorClass = R::FindClass(L"Actor");
    void* const sceneClass = R::FindClass(L"SceneComponent");
    void* const supportClass = R::ClassOf(supportActor);
    void* const componentClass = R::ClassOf(component);
    if (!actorClass || !sceneClass || !supportClass || !componentClass ||
        !R::IsDescendantOfAny(supportClass, &actorClass, 1) ||
        !R::IsDescendantOfAny(componentClass, &sceneClass, 1))
        return false;

    const std::wstring className = R::ClassNameOf(supportActor);
    const std::wstring actorName = R::ToString(R::NameOf(supportActor));
    const std::wstring componentName = R::ToString(R::NameOf(component));
    if (!CopyWireClass(p.support.className, className) ||
        !CopyWireKey(p.support.actorName, actorName, true) ||
        !CopyWireKey(p.support.componentName, componentName, false))
        return false;
    const coop::element::ElementId supportId = coop::element::Registry::Get().EidForActor(supportActor);
    p.support.elementId = supportId < coop::element::kMaxElements ? supportId : 0;
    return p.support.elementId != 0 || p.support.actorName.len != 0;
}

enum class SupportResolution { Found, Missing, Invalid };

SupportResolution ResolveSupport(const coop::net::PropStickStatePayload& p,
                                 void*& supportActor, void*& component) {
    supportActor = nullptr;
    component = nullptr;
    std::wstring className, actorName, componentName;
    if (!ValidSupportIdentity(p) ||
        !DecodeWireClass(p.support.className, className) ||
        !DecodeWireKey(p.support.actorName, actorName, p.support.elementId != 0) ||
        !DecodeWireKey(p.support.componentName, componentName, false))
        return SupportResolution::Invalid;

    void* const currentWorld = ue_wrap::world_identity::CurrentWorld();
    if (!currentWorld) return SupportResolution::Missing;
    if (p.support.elementId != 0) {
        coop::element::Registry::IdentitySnapshot identity{};
        if (coop::element::Registry::Get().ReadIdentity(p.support.elementId, identity)) {
            if (identity.deleting || !identity.actor ||
                !R::IsLiveByIndex(identity.actor, identity.internalIdx))
                return SupportResolution::Missing;
            supportActor = identity.actor;
            if (R::ClassNameOf(supportActor) != className)
                return SupportResolution::Invalid;
        }
    }

    if (!supportActor && !actorName.empty()) {
        int matches = 0;
        for (void* candidate : R::FindObjectsByClass(className.c_str())) {
            if (!candidate || !R::IsLive(candidate) ||
                ue_wrap::world_identity::WorldOf(candidate) != currentWorld ||
                R::ToString(R::NameOf(candidate)) != actorName)
                continue;
            supportActor = candidate;
            if (++matches > 1) return SupportResolution::Invalid;
        }
    }
    if (!supportActor) return SupportResolution::Missing;
    if (ue_wrap::world_identity::WorldOf(supportActor) != currentWorld)
        return SupportResolution::Missing;

    void* const sceneClass = R::FindClass(L"SceneComponent");
    if (!sceneClass) return SupportResolution::Missing;
    int componentMatches = 0;
    for (const R::ObjectRef& child : R::ChildObjectsOf(supportActor)) {
        if (!child.object || child.name != componentName ||
            !R::IsLive(child.object) ||
            ue_wrap::world_identity::WorldOf(child.object) != currentWorld)
            continue;
        void* const childClass = R::ClassOf(child.object);
        if (!childClass || !R::IsDescendantOfAny(childClass, &sceneClass, 1)) continue;
        component = child.object;
        if (++componentMatches > 1) return SupportResolution::Invalid;
    }
    return componentMatches == 1 ? SupportResolution::Found : SupportResolution::Missing;
}

void OnSupportEndPlay(void* actor, ue_wrap::actor_end_play::Reason reason) {
    if (reason != ue_wrap::actor_end_play::Reason::Destroyed &&
        reason != ue_wrap::actor_end_play::Reason::RemovedFromWorld)
        return;
    for (SupportAttachment& attachment : g_supportAttachments) {
        if (attachment.used && attachment.support.Is(actor)) attachment.retire = true;
    }
}

bool EnsureSupportSink() {
    if (g_supportSinkInstalled) return true;
    if (!ue_wrap::actor_end_play::IsInstalled()) return false;
    g_supportSinkInstalled = ue_wrap::actor_end_play::AddSink(&OnSupportEndPlay);
    if (!g_supportSinkInstalled)
        UE_LOGE("prop_stick_sync: support EndPlay sink unavailable -- exact wall attachments are disabled");
    return g_supportSinkInstalled;
}

int SupportAttachmentSlot(void* prop) {
    int freeSlot = -1;
    for (int i = 0; i < kMaxSupportAttachments; ++i) {
        SupportAttachment& attachment = g_supportAttachments[i];
        if (attachment.used && attachment.prop.Is(prop)) return i;
        if (!attachment.used && freeSlot < 0) freeSlot = i;
    }
    return freeSlot;
}

bool HasSupportAttachment(void* prop) {
    for (const SupportAttachment& attachment : g_supportAttachments) {
        if (attachment.used && attachment.prop.Is(prop)) return true;
    }
    return false;
}

void ForgetSupportAttachment(void* prop) {
    for (SupportAttachment& attachment : g_supportAttachments) {
        if (attachment.used && attachment.prop.Is(prop)) {
            attachment = SupportAttachment{};
        }
    }
}

void TickSupportAttachments() {
    const uint32_t generation = ue_wrap::world_identity::Generation();
    for (SupportAttachment& attachment : g_supportAttachments) {
        if (!attachment.used) continue;
        if (attachment.worldGeneration != generation) {
            attachment = SupportAttachment{};
            continue;
        }
        void* prop = attachment.prop.Get();
        if (!prop) {
            attachment = SupportAttachment{};
            continue;
        }
        if (!attachment.retire && attachment.support.Get()) continue;
        attachment = SupportAttachment{};
        coop::prop_lifecycle::DestroyLocalProp(prop, /*deferred=*/false);
    }
}

bool SamePendingSupport(const PendingSupport& pending, const coop::net::PropStickStatePayload& payload,
                        uint8_t senderPeerSlot) {
    return pending.senderPeerSlot == senderPeerSlot &&
           pending.payload.elementId == payload.elementId &&
           pending.payload.key.len == payload.key.len &&
           std::memcmp(pending.payload.key.data, payload.key.data, payload.key.len) == 0 &&
           pending.payload.support.elementId == payload.support.elementId &&
           pending.payload.support.componentName.len == payload.support.componentName.len &&
           std::memcmp(pending.payload.support.componentName.data, payload.support.componentName.data,
                       payload.support.componentName.len) == 0 &&
           pending.payload.flags == payload.flags;
}

bool SamePendingProp(const PendingSupport& pending, const coop::net::PropStickStatePayload& payload,
                     uint8_t senderPeerSlot) {
    return pending.senderPeerSlot == senderPeerSlot &&
           pending.payload.elementId == payload.elementId &&
           pending.payload.key.len == payload.key.len &&
           std::memcmp(pending.payload.key.data, payload.key.data, payload.key.len) == 0;
}

void CancelPendingSupport(const coop::net::PropStickStatePayload& payload, uint8_t senderPeerSlot) {
    std::lock_guard<std::mutex> lk(g_pendingSupportMutex);
    for (int i = 0; i < g_pendingSupportCount;) {
        if (SamePendingProp(g_pendingSupport[i], payload, senderPeerSlot)) {
            g_pendingSupport[i] = g_pendingSupport[--g_pendingSupportCount];
            continue;
        }
        ++i;
    }
}

void QueuePendingSupport(const coop::net::PropStickStatePayload& payload, uint8_t senderPeerSlot,
                         uint64_t queuedAt = 0) {
    std::lock_guard<std::mutex> lk(g_pendingSupportMutex);
    for (int i = 0; i < g_pendingSupportCount; ++i) {
        PendingSupport& pending = g_pendingSupport[i];
        if (SamePendingProp(pending, payload, senderPeerSlot)) {
            if (SamePendingSupport(pending, payload, senderPeerSlot)) {
                if (queuedAt && queuedAt < pending.queuedAt) pending.queuedAt = queuedAt;
                return;
            }
            pending.payload = payload;
            pending.queuedAt = queuedAt ? queuedAt : NowMs();
            pending.worldGeneration = ue_wrap::world_identity::Generation();
            return;
        }
    }
    if (g_pendingSupportCount >= kMaxPendingSupport) {
        UE_LOGW("prop_stick_sync: exact-support wait list full -- dropped stick for eid=%u",
                payload.elementId);
        return;
    }
    PendingSupport& pending = g_pendingSupport[g_pendingSupportCount++];
    pending.payload = payload;
    pending.senderPeerSlot = senderPeerSlot;
    pending.queuedAt = queuedAt ? queuedAt : NowMs();
    pending.worldGeneration = ue_wrap::world_identity::Generation();
}

void DrainPendingSupport() {
    PendingSupport ready[kMaxPendingSupport];
    int count = 0;
    {
        std::lock_guard<std::mutex> lk(g_pendingSupportMutex);
        count = g_pendingSupportCount;
        for (int i = 0; i < count; ++i) ready[i] = g_pendingSupport[i];
        g_pendingSupportCount = 0;
    }
    const uint64_t now = NowMs();
    const uint32_t generation = ue_wrap::world_identity::Generation();
    for (int i = 0; i < count; ++i) {
        if (ready[i].worldGeneration != generation) continue;
        if (now < ready[i].queuedAt || now - ready[i].queuedAt >= 10000) {
            UE_LOGW("prop_stick_sync: exact support for key '%.*s' eid=%u did not arrive within 10s",
                    ready[i].payload.key.len, ready[i].payload.key.data, ready[i].payload.elementId);
            continue;
        }
        OnStickState(ready[i].payload, ready[i].senderPeerSlot);
        std::lock_guard<std::mutex> lk(g_pendingSupportMutex);
        for (int j = 0; j < g_pendingSupportCount; ++j) {
            if (SamePendingSupport(g_pendingSupport[j], ready[i].payload, ready[i].senderPeerSlot)) {
                g_pendingSupport[j].queuedAt = ready[i].queuedAt;
                break;
            }
        }
    }
}

bool ApplySupportStick(void* prop, const coop::net::PropStickStatePayload& payload,
                       void* supportActor, void* component, const std::wstring& keyW) {
    if (!EnsureSupportSink()) {
        UE_LOGW("prop_stick_sync: cannot attach key='%ls' -- support EndPlay sink is unavailable", keyW.c_str());
        return false;
    }
    const int slot = SupportAttachmentSlot(prop);
    if (slot < 0) {
        UE_LOGE("prop_stick_sync: support attachment table full -- refusing key='%ls'", keyW.c_str());
        return false;
    }

    ue_wrap::FVector oldLoc{};
    ue_wrap::FRotator oldRot{};
    if (!E::TryGetActorLocation(prop, oldLoc) || !E::TryGetActorRotation(prop, oldRot)) return false;
    void* oldComponent = nullptr;
    if (!E::GetAttachParentComponent(prop, oldComponent)) return false;
    const bool wasFrozen = ue_wrap::prop::IsFrozen(prop);
    const bool wasStatic = ue_wrap::prop::IsStatic(prop);
    const bool wasSimulating = E::IsActorRootSimulatingPhysics(prop);

    const auto restore = [&]() {
        void* currentComponent = nullptr;
        if (E::GetAttachParentComponent(prop, currentComponent) && currentComponent != oldComponent &&
            currentComponent)
            E::DetachActorFromParent(prop);
        if (oldComponent && R::IsLive(oldComponent))
            E::AttachActorToComponentKeepWorld(prop, oldComponent);
        ue_wrap::prop::WriteFrozen(prop, wasFrozen);
        ue_wrap::prop::WriteStatic(prop, wasStatic);
        E::SetActorSimulatePhysics(prop, wasSimulating);
        E::SetActorLocation(prop, oldLoc);
        E::SetActorRotation(prop, oldRot);
    };

    coop::remote_prop::ClearAnyDriveFor(prop);
    if (oldComponent && oldComponent != component && !E::DetachActorFromParent(prop)) {
        UE_LOGW("prop_stick_sync: could not detach key='%ls' from its previous support", keyW.c_str());
        return false;
    }
    if (!E::SetActorLocation(prop, ue_wrap::FVector{payload.locX, payload.locY, payload.locZ}) ||
        !E::SetActorRotation(prop, ue_wrap::FRotator{payload.rotPitch, payload.rotYaw, payload.rotRoll})) {
        restore();
        UE_LOGW("prop_stick_sync: sender pose could not be applied for key='%ls'", keyW.c_str());
        return false;
    }
    ue_wrap::prop::WriteFrozen(prop, (payload.flags & 1u) != 0);
    ue_wrap::prop::WriteStatic(prop, (payload.flags & 2u) != 0);
    if (!E::SetActorSimulatePhysics(prop, false) || !E::AttachActorToComponentKeepWorld(prop, component)) {
        restore();
        UE_LOGW("prop_stick_sync: exact support attach failed for key='%ls' support=%p component=%p",
                keyW.c_str(), supportActor, component);
        return false;
    }

    SupportAttachment& attachment = g_supportAttachments[slot];
    attachment.prop.Set(prop);
    attachment.support.Set(supportActor);
    attachment.worldGeneration = ue_wrap::world_identity::Generation();
    attachment.retire = false;
    attachment.used = true;
    UE_LOGI("prop_stick_sync: STICK applied key='%ls' eid=%u support=%u '%.*s'/%.*s",
            keyW.c_str(), payload.elementId, payload.support.elementId,
            payload.support.className.len, payload.support.className.data,
            payload.support.componentName.len, payload.support.componentName.data);
    return true;
}

// Each half settles on its first attempt: a name that does not resolve is a recooked Blueprint, and a
// full table is a capacity fault to fix, not a slot to wait for.
void DisableStickHalf() {
    g_stickInstalled.store(true, std::memory_order_release);
}
void DisableUnstickHalf() {
    g_unstickDisabled.store(true, std::memory_order_release);
    g_unstickInstalled.store(true, std::memory_order_release);
}

void InstallStickHalf() {
    const ue_wrap::facts::Resolved uber = ue_wrap::facts::ResolveOn(kRowCompUbergraph, g_compClass);
    g_uberFn = uber.Fn();
    g_entryParamOff = g_uberFn ? R::FindParamOffset(g_uberFn, L"EntryPoint") : -1;
    const int32_t uberFrame = g_uberFn ? R::FunctionFrameSize(g_uberFn) : 0;
    if (!g_uberFn || g_entryParamOff < 0 || uberFrame < 4 ||
        g_entryParamOff > uberFrame - static_cast<int32_t>(sizeof(int32_t))) {
        UE_LOGE("prop_stick_sync: stick observer resolve failed (uber=%p entryOff=%d frame=%d) -- observer "
                "DISABLED (a recooked comp_wallAttachable)", g_uberFn, g_entryParamOff, uberFrame);
        DisableStickHalf();
        return;
    }
    if (!GT::RegisterPostObserver(uber, &OnCompUbergraphPost)) {
        UE_LOGE("prop_stick_sync: RegisterPostObserver failed (the observer table is full) -- stick half DISABLED");
        DisableStickHalf();
        return;
    }
    g_stickInstalled.store(true, std::memory_order_release);
    UE_LOGI("prop_stick_sync: stick observer installed (commit entry %d, entryOff=%d, comp.prop@%d)",
            kStickCommitEntry, g_entryParamOff, g_compPropOff);
}

void InstallUnstickHalf() {
    const ue_wrap::facts::Resolved unstick = ue_wrap::facts::ResolveOn(kRowUnstick, g_compClass);
    g_unstickFn = unstick.Fn();
    g_withToolOff = g_unstickFn ? R::FindParamOffset(g_unstickFn, L"withTool") : -1;
    if (!g_unstickFn || g_withToolOff < 0) {
        UE_LOGE("prop_stick_sync: unstick resolve failed (unstick=%p toolOff=%d) -- unstick half DISABLED (a "
                "recooked comp_wallAttachable)", g_unstickFn, g_withToolOff);
        DisableUnstickHalf();
        return;
    }
    const int32_t unstickFrame = R::FunctionFrameSize(g_unstickFn);
    if (g_withToolOff >= 16 || unstickFrame > 16) {
        UE_LOGE("prop_stick_sync: unstick signature drift (withToolOff=%d frameSize=%d vs a 16-byte frame) -- "
                "unstick half DISABLED (re-RE comp_wallAttachable)", g_withToolOff, unstickFrame);
        DisableUnstickHalf();
        return;
    }
    // The gate refuses only a native or null function, a full table or a gate that never installed.
    if (!sg::Watch(unstick, kTagUnstick, &OnUnstickPre, nullptr)) {
        UE_LOGE("prop_stick_sync: the script gate refused the unstick watch -- unstick half DISABLED");
        DisableUnstickHalf();
        return;
    }
    g_unstickInstalled.store(true, std::memory_order_release);
    UE_LOGI("prop_stick_sync: unstick half installed (withToolOff=%d, a script-gate watch)", g_withToolOff);
}

// The component's pieces and each half not yet installed. Needs the class. Game thread.
void InstallHalves() {
    if (!g_compClass) return;
    g_compPropOff = R::FindPropertyOffset(g_compClass, L"prop");
    if (g_compPropOff < 0) {
        UE_LOGE("prop_stick_sync: comp_wallAttachable_C::prop did not resolve -- both halves DISABLED (a "
                "recooked comp_wallAttachable)");
        DisableStickHalf();
        DisableUnstickHalf();
        return;
    }
    if (!g_stickInstalled.load(std::memory_order_acquire)) InstallStickHalf();
    if (!g_unstickInstalled.load(std::memory_order_acquire)) InstallUnstickHalf();
}

// A join's converge or a live message needs the lane before Install has run: one attempt a session,
// after which Install carries the retries. Game thread.
void InstallOnDemand() {
    if (g_onDemandTried) return;
    if (g_stickInstalled.load(std::memory_order_acquire) && g_unstickInstalled.load(std::memory_order_acquire))
        return;
    g_onDemandTried = true;
    InstallHalves();
}

bool BroadcastStick(coop::net::Session* s, void* prop, bool frozen, bool statiq) {
    coop::net::PropStickStatePayload p{};
    std::wstring keyW;
    if (const Fill f = FillIdentityAndPose(p, prop, keyW); f != Fill::Ok) {
        return false;
    }
    p.flags = (frozen ? 1u : 0u) | (statiq ? 2u : 0u);
    if (!FillSupportIdentity(p, prop)) {
        UE_LOGW("prop_stick_sync: cannot publish STICK key='%ls' -- exact support identity is unavailable",
                keyW.c_str());
        return false;
    }
    // The hold this stick ends, when the stuck prop is the one this peer holds (the release edge
    // runs after this pass); 0 closes nothing.
    p.holdGen = (coop::local_streams::LastHeldActor() == prop) ? coop::local_streams::CurrentHoldGen()
                                                                : uint16_t{0};
    if (!s->SendReliable(coop::net::ReliableKind::PropStickState, &p, sizeof(p))) return false;
    UE_LOGI("prop_stick_sync: broadcast STICK key='%ls' eid=%u flags=%u pose=(%.0f,%.0f,%.0f)",
            keyW.c_str(), p.elementId, p.flags, p.locX, p.locY, p.locZ);
    return true;
}

bool SendCurrentToSlotImpl(coop::net::Session* s, void* prop, int peerSlot) {
    if (!s || !prop || !(ue_wrap::prop::IsFrozen(prop) || ue_wrap::prop::IsStatic(prop))) return true;
    if (!WallAttachCompOf(prop)) return true;
    void* supportActor = nullptr;
    if (!E::GetAttachParentActor(prop, supportActor) || !supportActor) return true;
    coop::net::PropStickStatePayload p{};
    std::wstring keyW;
    if (FillIdentityAndPose(p, prop, keyW) != Fill::Ok) return false;
    p.flags = (ue_wrap::prop::IsFrozen(prop) ? 1u : 0u) | (ue_wrap::prop::IsStatic(prop) ? 2u : 0u);
    if (!FillSupportIdentity(p, prop)) {
        UE_LOGW("prop_stick_sync: snapshot STICK key='%ls' has no exact support identity", keyW.c_str());
        return false;
    }
    if (!s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::PropStickState, &p, sizeof(p))) {
        UE_LOGW("prop_stick_sync: snapshot STICK send refused key='%ls' slot=%d", keyW.c_str(), peerSlot);
        return false;
    }
    return true;
}

bool BroadcastUnstick(coop::net::Session* s, void* prop) {
    coop::net::PropStickStatePayload p{};
    std::wstring keyW;
    if (const Fill f = FillIdentityAndPose(p, prop, keyW); f != Fill::Ok) {
        return false;
    }
    p.flags = 0;
    p.holdGen = 0;  // an unstick ends no hold; a grab's starts one, which its poses carry
    const ue_wrap::prop::VelocityState v = ue_wrap::prop::GetPhysicsVelocity(prop);
    if (v.ok) {
        p.linVelX = v.linearCmS.X;   p.linVelY = v.linearCmS.Y;   p.linVelZ = v.linearCmS.Z;
        p.angVelX = v.angularDegS.X; p.angVelY = v.angularDegS.Y; p.angVelZ = v.angularDegS.Z;
    }
    if (!s->SendReliable(coop::net::ReliableKind::PropStickState, &p, sizeof(p))) return false;
    UE_LOGI("prop_stick_sync: broadcast UNSTICK key='%ls' eid=%u pose=(%.0f,%.0f,%.0f) angVel=(%.0f,%.0f,%.0f) deg/s",
            keyW.c_str(), p.elementId, p.locX, p.locY, p.locZ, p.angVelX, p.angVelY, p.angVelZ);
    return true;
}

// The copy is freed by the component's own unstick. Unless another owner moves it -- a peer's
// hold, the host's driven-prop channel, this peer's own grab -- it lets go from the unsticking
// peer's pose with that peer's velocity, a pry's kick included.
void ApplyUnstick(void* prop, const coop::net::PropStickStatePayload& p, const std::wstring& keyW,
                  uint8_t senderPeerSlot) {
    // A copy already free -- the hold's first pose ran the unstick before this message came -- gets no
    // second one: its init() would switch the copy's simulation on under the drive that owns it.
    const bool wasStuck = ue_wrap::prop::IsFrozen(prop) || ue_wrap::prop::IsStatic(prop);
    const bool driven = coop::remote_prop::IsActorUnderAnyDrive(prop) || coop::prop_drive_stream::IsParked(prop);
    ue_wrap::CachedObjRef ref;
    ref.Set(prop);
    if (wasStuck && !ReplayUnstick(prop)) {
        UE_LOGW("prop_stick_sync: UNSTICK key='%ls' eid=%u -- the native freeing verb did not run here; the copy "
                "stays as it is", keyW.c_str(), p.elementId);
        return;
    }
    // Native init() is virtual and can tear down/replace a subclass's actor.
    if (!ref.Get()) return;
    if (wasStuck && (ue_wrap::prop::IsFrozen(prop) || ue_wrap::prop::IsStatic(prop))) return;
    if (HasSupportAttachment(prop)) {
        void* parent = nullptr;
        if (E::GetAttachParentComponent(prop, parent) && parent && !E::DetachActorFromParent(prop))
            UE_LOGW("prop_stick_sync: UNSTICK key='%ls' could not detach its exact support", keyW.c_str());
        ForgetSupportAttachment(prop);
    }
    // init() recomputes simulation. A late unstick under a network drive must
    // restore its kinematic body; a real local physics-handle grab stays simulating.
    const bool localHeld = coop::local_prop_hold::IsHeld(prop);
    if (wasStuck && driven && !localHeld) E::SetActorSimulatePhysics(prop, false);
    const bool owned = driven || localHeld;
    if (!owned) {
        E::SetActorLocation(prop, ue_wrap::FVector{p.locX, p.locY, p.locZ});
        E::SetActorRotation(prop, ue_wrap::FRotator{p.rotPitch, p.rotYaw, p.rotRoll});
        if (void* mesh = ue_wrap::prop::GetStaticMesh(prop)) {
            E::SetComponentLinearVelocity(mesh, p.linVelX, p.linVelY, p.linVelZ);
            E::SetComponentAngularVelocity(mesh, p.angVelX, p.angVelY, p.angVelZ);
        }
    }
    UE_LOGI("prop_stick_sync: UNSTICK applied key='%ls' eid=%u (was %s here; %s; slot %u)", keyW.c_str(),
            p.elementId, wasStuck ? "stuck" : "already free",
            owned ? "another hold moves it" : "let go from the sender's pose", senderPeerSlot);
}

}  // namespace

bool SendCurrentToSlot(coop::net::Session* session, void* prop, int peerSlot) {
    return SendCurrentToSlotImpl(session, prop, peerSlot);
}

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    EnsureSupportSink();
    if (!g_pryWatchInstalled)
        g_pryWatchInstalled = sg::WatchName(L"crowbarOpen", kTagPry, &OnPryPre, nullptr);
    if (g_stickInstalled.load(std::memory_order_acquire) && g_unstickInstalled.load(std::memory_order_acquire))
        return;
    // One index lookup a call until the class is found; it is loaded at the main menu, before any
    // session, so the first call finds it.
    if (!g_compClass) g_compClass = ue_wrap::object_index::ClassByName(L"comp_wallAttachable_C");
    if (!g_compClass) return;
    InstallHalves();
}

void Tick() {
    TickSupportAttachments();
    DrainPendingSupport();
    // Drain every record since the last pass. Game thread; MUST run before local_streams' release
    // edge in the pump pass (header note on the stick-before-release ordering).
    Pending ready[kMaxPending];
    int nReady = 0;
    {
        std::lock_guard<std::mutex> lk(g_pendingMutex);
        nReady = g_pendingCount;
        for (int i = 0; i < g_pendingCount; ++i) ready[i] = g_pending[i];
        g_pendingCount = 0;
    }
    auto* s = LoadSession();
    if (!s || !s->connected()) return;
    for (int i = 0; i < nReady; ++i) {
        if (ready[i].worldGeneration != ue_wrap::world_identity::Generation()) continue;
        void* prop = ready[i].prop.Get();
        if (!prop) continue;  // destroyed, recycled slot or another world since the record
        const bool frozen = ue_wrap::prop::IsFrozen(prop);
        const bool statiq = ue_wrap::prop::IsStatic(prop);
        bool sent = false;
        if (ready[i].unstick) {
            // Still stuck: the body refused, as a hand grab of a pried-on prop does (the "Tool
            // required" hint), and there is nothing to mirror.
            if (frozen || statiq) continue;
            sent = BroadcastUnstick(s, prop);
        } else {
            if (!frozen && !statiq) continue;  // commit bailed (the 45 body re-traces and can exit before the flag write)
            sent = BroadcastStick(s, prop, frozen, statiq);
        }
        if (!sent) {
            const uint64_t now = NowMs();
            if (now < ready[i].queuedAt || now - ready[i].queuedAt >= 2000) {
                UE_LOGW("prop_stick_sync: could not publish %s of %p within 2s; outcome dropped",
                        ready[i].unstick ? "unstick" : "stick", prop);
                continue;
            }
            // Send failure is backpressure. Retain the original lifetime/deadline
            // and rebuild from live state on retry; never replay an obsolete edge.
            std::lock_guard<std::mutex> lk(g_pendingMutex);
            bool duplicate = false;
            for (int j = 0; j < g_pendingCount; ++j)
                duplicate |= g_pending[j].prop.Is(prop) && g_pending[j].unstick == ready[i].unstick;
            if (!duplicate && g_pendingCount < kMaxPending) g_pending[g_pendingCount++] = ready[i];
        }
    }
}

void OnStickState(const coop::net::PropStickStatePayload& payload, uint8_t senderPeerSlot) {
    // Game thread (event_feed drain). Resolve key-first, eid fallback (the PropDestroy shape).
    UE_ASSERT_GAME_THREAD("prop_stick_sync::OnStickState");
    const bool unstick = payload.flags == 0;
    if (senderPeerSlot >= coop::net::kMaxPeers || (payload.flags & ~3u) ||
        (!unstick && !ValidSupportIdentity(payload)) || (unstick && payload.holdGen != 0)) {
        UE_LOGW("prop_stick_sync: invalid %s state from slot %u -- dropped",
                unstick ? "UNSTICK" : "STICK", senderPeerSlot);
        return;
    }
    CancelPendingSupport(payload, senderPeerSlot);
    coop::remote_prop::CloseHold(senderPeerSlot, payload.holdGen);
    const std::wstring keyW = coop::remote_prop::KeyToWString(payload.key);
    void* prop = nullptr;
    if (!keyW.empty() && keyW != L"None")
        prop = coop::prop_key_index::Find(keyW);
    if (!prop && payload.elementId != 0) {
        auto* el = coop::element::MirrorManager<coop::element::Prop>::Instance().Get(
            static_cast<coop::element::ElementId>(payload.elementId));
        if (el) {
            void* a = el->GetActor();
            if (a && R::IsLiveByIndex(a, el->GetInternalIdx())) prop = a;
        }
    }
    // The prop's spawn waits for the snapshot bracket's end: the newest stick state is kept for the bind. The relay
    // to the other clients ran before this function.
    if (!prop && coop::props::bracket_position_wait::KeepIfWaiting(
                     payload.key, coop::props::bracket_position_wait::Kept::Stick, &payload, sizeof(payload),
                     senderPeerSlot))
        return;
    if (!prop) {
        UE_LOGW("prop_stick_sync: %s for key='%ls' eid=%u -- no local match (slot %u)",
                unstick ? "UNSTICK" : "STICK", keyW.c_str(), payload.elementId, senderPeerSlot);
        return;
    }
    if (!ue_wrap::prop::IsDescendantOfProp(prop) || coop::hand_item::IsHandAxisActor(prop)) return;
    void* comp = WallAttachCompOf(prop);  // the component, checked against the index's class
    if (!comp) {
        if (unstick && !HasWallAttachMember(prop)) {
            coop::prop_drive_host::OverrideClientSyncer(prop, "pry replay");
            ApplyUnstick(prop, payload, keyW, senderPeerSlot);
            return;
        }
        UE_LOGW("prop_stick_sync: %s target %p carries no wall-attach component -- dropped",
                unstick ? "UNSTICK" : "STICK", prop);
        return;
    }
    if (unstick) {
        if (!g_unstickInstalled.load(std::memory_order_acquire)) InstallOnDemand();
        if (!g_unstickInstalled.load(std::memory_order_acquire) ||
            g_unstickDisabled.load(std::memory_order_acquire)) {
            UE_LOGW("prop_stick_sync: UNSTICK for key='%ls' -- unstick verb is unavailable", keyW.c_str());
            return;
        }
        coop::prop_drive_host::OverrideClientSyncer(prop, "unstick replay");
        ApplyUnstick(prop, payload, keyW, senderPeerSlot);
        return;
    }

    void* supportActor = nullptr;
    void* supportComponent = nullptr;
    const SupportResolution support = ResolveSupport(payload, supportActor, supportComponent);
    if (support == SupportResolution::Missing) {
        QueuePendingSupport(payload, senderPeerSlot);
        return;
    }
    if (support == SupportResolution::Invalid) {
        UE_LOGW("prop_stick_sync: exact support identity is ambiguous or invalid for key='%ls' -- dropped",
                keyW.c_str());
        return;
    }
    coop::prop_drive_host::OverrideClientSyncer(prop, "exact-support stick");
    ApplySupportStick(prop, payload, supportActor, supportComponent, keyW);
}

void ConvergeStuck(void* actor, uint8_t physFlags) {
    namespace pf = coop::net::propspawn_flags;
    if (!(physFlags & pf::kLiveState) || !actor || !ue_wrap::prop::IsDescendantOfProp(actor)) return;
    const bool hostStuck = (physFlags & (pf::kFrozen | pf::kStatic)) != 0;
    const bool copyStuck = ue_wrap::prop::IsFrozen(actor) || ue_wrap::prop::IsStatic(actor);
    const bool wantStatic = (physFlags & pf::kStatic) != 0;
    const bool staticDiffers = wantStatic != ue_wrap::prop::IsStatic(actor);
    if (hostStuck == copyStuck && !staticDiffers) return;
    void* comp = WallAttachCompOf(actor);
    if (!comp) {
        // Ordinary pryable props have no component verb. A join/converge still
        // needs their live static bit; frozen and sleep converge in the caller.
        if (!HasWallAttachMember(actor) && staticDiffers && !ue_wrap::prop::CallBaseSetPropProps(actor, wantStatic,
                ue_wrap::prop::IsFrozen(actor), ue_wrap::prop::IsSleeping(actor)))
            UE_LOGW("prop_stick_sync: static converge did not dispatch on %p", actor);
        return;
    }
    if (hostStuck == copyStuck) return;
    const std::wstring keyW = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!hostStuck) {
        InstallOnDemand();  // a join's snapshot can arrive before Install has run
        const bool ok = ReplayUnstick(actor);
        UE_LOGI("prop_stick_sync: join converge key='%ls' -- stuck here, free on the host: the component's unstick "
                "%s", keyW.c_str(), ok ? "ran" : "did not run");
        return;
    }
    UE_LOGW("prop_stick_sync: join converge key='%ls' -- waiting for the snapshot's exact support state",
            keyW.c_str());
}

bool IsWallAttachable(void* actor) {
    UE_ASSERT_GAME_THREAD("prop_stick_sync::IsWallAttachable");
    return WallAttachCompOf(actor) != nullptr;
}

bool ReplayUnstick(void* actor) {
    if (!actor || !ue_wrap::prop::IsDescendantOfProp(actor)) return false;
    void* comp = WallAttachCompOf(actor);
    if (!comp && HasWallAttachMember(actor)) return false;
    if (!comp)
        return ue_wrap::prop::CallBaseSetPropProps(actor, false, false,
                                                  ue_wrap::prop::IsSleeping(actor));
    if (!g_unstickInstalled.load(std::memory_order_acquire)) InstallOnDemand();
    if (!g_unstickInstalled.load(std::memory_order_acquire) ||
        g_unstickDisabled.load(std::memory_order_acquire)) return false;
    unsigned char frame[16] = {};
    frame[g_withToolOff] = 1;
    ++g_replayDepth;
    const bool ok = R::CallFunction(comp, g_unstickFn, frame);
    --g_replayDepth;
    return ok;
}

void OnDisconnect() {
    g_onDemandTried = false;
    {
        std::lock_guard<std::mutex> lk(g_pendingMutex);
        g_pendingCount = 0;
    }
    {
        std::lock_guard<std::mutex> lk(g_pendingSupportMutex);
        g_pendingSupportCount = 0;
    }
}

}  // namespace coop::prop_stick_sync
