#include "coop/interactables/cord_sync.h"
#include "coop/interactables/cord_wire.h"
#include "coop/interactables/cord_state_cache.h"
#include "coop/element/intent_authority.h"
#include "coop/element/object_scan_hub.h"
#include "coop/element/portable_identity.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/remote_prop.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/cord.h"
#include "ue_wrap/devices/cord_blueprint.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include <atomic>
#include <array>
#include <deque>
#include <cstring>
#include <string>
#include <vector>

namespace coop::cord_sync {
namespace {
namespace C = ue_wrap::cord;
namespace R = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace W = cord_wire;
std::atomic<net::Session*> g_session{nullptr};
std::vector<ue_wrap::CachedObjRef> g_actors, g_scratch;
struct Sent { ue_wrap::CachedObjRef actor; net::CordStatePayload p{}; uint64_t at = 0; };
std::vector<Sent> g_sent;
W::StateCache g_pending;
std::vector<net::CordStatePayload> g_outbox;
uint32_t g_world = 0;
uint64_t g_next = 0;
bool g_installed = false;
bool g_ticking = false;
bool g_applyingIntent = false;
uint64_t g_epoch = 0;
ue_wrap::CachedObjRef g_localDragCord;
bool g_localDragActive = false;
uint16_t g_localDragGen = 0;
net::PropPoseSnapshot g_localDragPose{};
std::deque<net::PropReleasePayload> g_releases;
struct RemoteDrag {
    ue_wrap::CachedObjRef actor;
    net::WireKey key{};
    uint16_t generation = 0;
    bool closed = false;
    uint64_t at = 0;
};
std::array<RemoteDrag, net::kMaxPeers> g_remoteDrags{};
bool AfterHold(uint16_t a, uint16_t b) { return static_cast<int16_t>(a-b) > 0; }
bool RemoteDragging(void* actor) {
    for (const auto& row : g_remoteDrags)
        if (!row.closed && row.actor.Get() == actor) return true;
    return false;
}
bool FrameContains(void* function, int32_t offset, size_t bytes) {
    if (!function || offset < 0 || !bytes) return false;
    const int32_t frameSize = R::FunctionFrameSize(function);
    if (frameSize < 0) return false;
    const auto start = static_cast<size_t>(offset);
    const auto size = static_cast<size_t>(frameSize);
    return start <= size && bytes <= size - start;
}
void RestoreHead(const ue_wrap::CachedObjRef& ref) {
    if (void* a = ref.Get(); a && !C::Dragging(a) && !C::Socket(a) && !RemoteDragging(a))
        C::Disconnect(a); // native reset also works when already unplugged
}
void EndLocalDrag(net::Session& session, bool clearStream) {
    if (!g_localDragActive) return;
    net::PropReleasePayload p{};
    p.key = g_localDragPose.key;
    p.holdGen = g_localDragPose.holdGen;
    // Retain refused releases. Stop starting more holds if this bounded queue fills.
    g_releases.push_back(p);
    if (clearStream) session.SetLocalPropPose(false, {});
    g_localDragCord.Reset(); g_localDragActive = false;
}
constexpr wchar_t kDragPosePrefix[] = L"__mvcord_";
bool Client() {
    auto* s = g_session.load();
    return s && s->connected() && s->role() == net::Role::Client;
}
void ResetWorld() {
    const auto gen = ue_wrap::world_identity::Generation();
    if (g_world == gen) return;
    if (g_localDragActive) { if (auto* s = g_session.load()) s->SetLocalPropPose(false, {}); }
    g_localDragCord.Reset(); g_localDragActive = false;
    g_remoteDrags = {}; g_releases.clear(); g_localDragPose = {}; g_localDragGen = 0;
    g_world = gen; g_next = 0; ++g_epoch;
    g_actors.clear(); g_scratch.clear(); g_sent.clear(); g_pending.Clear(); g_outbox.clear();
}
int32_t GraphEntry(void* fn, uint8_t* locals) {
    if (!fn || !locals || !R::NameEquals(R::NameOf(fn), L"ExecuteUbergraph_cord")) return -1;
    static R::ParamOffset entryOff{L"EntryPoint"};
    const int32_t off = entryOff.Of(fn);
    if (!FrameContains(fn, off, sizeof(int32_t))) return -1;
    int32_t entry = -1;
    if (off >= 0) std::memcpy(&entry, locals+off, sizeof(entry));
    return entry;
}
sg::Verdict OnGraph(const sg::Call& c) {
    auto* s = g_session.load();
    if (!s || !s->connected()) return sg::Verdict::Run;
    // Delay(0) can resume after ANY amount of wall time (pause, a hitch, a slow frame).
    // Cancel only its now-obsolete head reset, never the new connection or the tick.
    const auto entry = GraphEntry(c.function, c.locals);
    if (entry != C::blueprint::kReattachResume) return sg::Verdict::Run;
    return C::blueprint::StaleReattach(entry, C::Socket(c.object) != nullptr)
        ? sg::Verdict::Cancel : sg::Verdict::Run;
}
bool GuardReady() {
    return sg::ClassNameWatchLive(L"cord_C", L"ExecuteUbergraph_cord", 0x434F0005);
}
bool MatchType(void* a) { return C::IsCord(a) || C::IsSocket(a); }
bool Ready() { return true; }
uint64_t Classes() { return ue_wrap::object_index::ClassSetVersion(); }
void Begin(void*, bool full) {
    ResetWorld(); g_scratch.clear();
    if (!full) for (auto& r : g_actors) if (r.Alive()) g_scratch.push_back(r);
}
void Match(void*, void* a) {
    if (R::NameStartsWith(R::NameOf(a), L"Default__")) return;
    for (auto& r : g_scratch) if (r.Get() == a) return;
    ue_wrap::CachedObjRef r; r.Set(a); g_scratch.push_back(r);
}
size_t Complete(void*, bool, uint32_t gen) {
    g_world = gen; g_actors.swap(g_scratch); return g_actors.size();
}
bool CopyName(const std::wstring& name, char (&dest)[64]) {
    if (name.empty() || name.size() >= sizeof(dest)) return false;
    for (size_t i = 0; i < name.size(); ++i) {
        if (name[i] < 32 || name[i] > 126) return false;
        dest[i] = static_cast<char>(name[i]);
    }
    return true;
}
bool Identity(void* a, net::CordEndpoint& out) {
    out = {};
    if (!a || !MatchType(a)) return false;
    std::wstring component;
    void* owner = ue_wrap::engine::ParentActorOf(a, &component);
    void* subject = owner ? owner : a;
    const auto key = ue_wrap::prop::GetInteractableKeyString(subject);
    if (!key.empty() && key != L"None" && key.size() <= sizeof(out.key.data))
        net::WireKeyFromString(key, out.key);
    auto eid = prop_element_tracker::GetPropElementIdForActor(subject);
    if (eid == element::kInvalidId) eid = remote_prop::ResolveMirrorEidByActor(subject);
    if (eid != element::kInvalidId) out.elementId = eid;
    if (owner) {
        out.kind = 3;
        return CopyName(component, out.name) && W::Valid(out);
    }
    if (out.key.len || out.elementId) { out.kind = 1; return W::Valid(out); }
    // Only standalone level sockets may fall back to their placed name. Generated children
    // and unbound runtime props must wait for their parent's network identity.
    if (!C::IsSocket(a)) return false;
    out.kind = 2;
    return CopyName(R::ToString(R::NameOf(a)), out.name) && W::Valid(out);
}
void* Resolve(const net::CordEndpoint& id, bool socket) {
    void* result = nullptr;
    for (auto& r : g_actors) if (void* a = r.Get()) {
        if (ue_wrap::world_identity::WorldOf(a) != ue_wrap::world_identity::CurrentWorld() ||
            (socket ? !C::IsSocket(a) : !C::IsCord(a))) continue;
        net::CordEndpoint candidate{};
        if (Identity(a, candidate) && W::Same(id, candidate)) {
            if (result && result != a) return nullptr; // ambiguous identity fails closed
            result = a;
        }
    }
    return result;
}
// A client reapplies every live connection. Resolving each endpoint by reading
// every actor again made that pass quadratic in expensive engine/name reads.
// Cache identities only within this pass, never across ticks or native mutations.
struct ResolvePass {
    struct Row { ue_wrap::CachedObjRef actor; net::CordEndpoint id{}; bool socket=false; };
    std::vector<Row> rows;
    bool built=false;
    uint64_t epoch=g_epoch;
    uint32_t world=g_world;
    bool Current() const { return epoch==g_epoch && world==ue_wrap::world_identity::Generation() && Client(); }
    void Invalidate() { built=false; rows.clear(); }
    void* Find(const net::CordEndpoint& id,bool socket) {
        if (!Current()) return nullptr;
        if (!built) {
            built=true; rows.reserve(g_actors.size());
            // Identity reads do not dispatch native code; this snapshot cannot
            // mutate g_actors while it is being assembled.
            for (const auto& ref:g_actors) if (void* a=ref.Get()) {
                if (ue_wrap::world_identity::WorldOf(a)!=ue_wrap::world_identity::CurrentWorld()) continue;
                const bool isSocket=C::IsSocket(a);
                if (!isSocket && !C::IsCord(a)) continue;
                Row row;row.actor=ref;row.socket=isSocket;
                if (Identity(a,row.id)) rows.push_back(row);
            }
        }
        void* result=nullptr;
        for (const auto& row:rows) if (row.socket==socket && W::Same(id,row.id)) {
            void* a=row.actor.Get(); if (!a) continue;
            if (result && result!=a) return nullptr; // preserve ambiguous-key rejection
            result=a;
        }
        return result;
    }
};
void Store(std::vector<net::CordStatePayload>& rows, const net::CordStatePayload& p) {
    for (auto& row : rows) if (W::Same(row.cord, p.cord)) { row = p; return; }
    if (rows.size() < 256) rows.push_back(p);
}
bool Snapshot(void* a, net::CordStatePayload& p) {
    p = {};
    if (!C::IsCord(a) || ue_wrap::world_identity::WorldOf(a) != ue_wrap::world_identity::CurrentWorld() ||
        !Identity(a, p.cord)) return false;
    if (void* socket = C::Socket(a)) {
        bool powered = false;
        if (!Identity(socket, p.socket) || !C::Powered(socket, powered)) return false;
        p.powered = powered ? 1 : 0;
    }
    return W::Valid(p);
}
void Publish(void* a, int slot) {
    auto* s = g_session.load();
    net::CordStatePayload p{};
    if (!s || !Snapshot(a, p)) return;
    const auto now = active_drive::NowMs();
    Sent* last = nullptr;
    for (auto& row : g_sent) if (row.actor.Get() == a) { last = &row; break; }
    if (slot < 0 && last && now-last->at < 2000 &&
        W::Same(p.socket, last->p.socket) && p.powered == last->p.powered &&
        W::Same(p.cord, last->p.cord)) return;
    const bool ok = slot < 0 ? s->SendReliable(net::ReliableKind::CordState, &p, sizeof(p))
        : s->SendReliableToSlot(slot, net::ReliableKind::CordState, &p, sizeof(p));
    if (ok && slot < 0) {
        if (!last) { g_sent.emplace_back(); last = &g_sent.back(); last->actor.Set(a); }
        if (!last->at || !W::Same(p.socket, last->p.socket))
            UE_LOGI("cord: state '%ls' connected=%d powered=%u", R::ToString(R::NameOf(a)).c_str(), p.socket.kind != 0, p.powered);
        last->p = p; last->at = now;
    }
}
bool Paint(const net::CordStatePayload& p,ResolvePass& pass) {
    void* a = pass.Find(p.cord, false);
    if (!a || C::Dragging(a) || RemoteDragging(a)) return false; // keep local input responsive while its intent travels
    ue_wrap::CachedObjRef cordRef;cordRef.Set(a);
    if (!p.socket.kind) {
        if (!C::Socket(a)) return true;
        pass.Invalidate();return C::Disconnect(a);
    }
    if (!GuardReady()) return false;
    void* socket = pass.Find(p.socket, true);
    if (!socket) return false;
    ue_wrap::CachedObjRef socketRef;socketRef.Set(socket);
    // A canonical occupant also repairs an older connection loaded on this client.
    // The host never accepts two occupants for one socket.
    if (void* other = C::Occupant(socket); other && other != a) {
        if (C::Dragging(other)) return false;
        pass.Invalidate();
        if (!C::Disconnect(other) || !pass.Current() || !cordRef.Get() || !socketRef.Get()) return false;
    }
    bool power=false;
    if (!C::Powered(socket,power)) return false;
    if (power!=bool(p.powered)) {
        pass.Invalidate();
        if (!C::SetPower(socket,p.powered!=0) || !pass.Current() || !cordRef.Get() || !socketRef.Get()) return false;
    }
    if (C::Socket(a)==socket && C::Occupant(socket)==a) return true;
    pass.Invalidate();return C::Connect(a,socket);
}
sg::Verdict OnVerb(const sg::Call& c) {
    if (!Client() || c.fromOurCode || !net_pump::IsInAnnouncedWorld(c.object)) return sg::Verdict::Run;
    const bool connect = c.tag == 0x434F0001;
    net::CordStatePayload p{};
    if (connect) {
        static R::ParamOffset socketOff{L"socket"}, loadOff{L"load"};
        const auto off = socketOff.Of(c.function), load = loadOff.Of(c.function);
        if (!c.locals || !FrameContains(c.function, off, sizeof(void*)) ||
            !FrameContains(c.function, load, sizeof(uint8_t))) return sg::Verdict::Cancel;
        if (c.locals[load]) return sg::Verdict::Run; // native save reconstruction, no player intent
        void* socket = nullptr;
        std::memcpy(&socket, c.locals+off, sizeof(socket));
        if (!C::IsSocket(socket) || !Identity(socket, p.socket)) return sg::Verdict::Cancel;
    } else {
        const auto entry = c.callerObject == c.object ? GraphEntry(c.callerFunction, c.callerLocals) : -1;
        if (entry == C::blueprint::kDestroyed) return sg::Verdict::Run;
        if (!C::blueprint::UserDisconnect(entry)) return sg::Verdict::Cancel;
    }
    if (!Identity(c.object, p.cord)) return sg::Verdict::Cancel;
    // An optimistic connect runs power callbacks and can call disconnect internally on an
    // occupied LOCAL socket, overwriting this request. The host alone performs connect.
    // tryToConnect still returns its candidate and used() ends dragging after our cancellation.
    // Local disconnect must run so socket clears and the next tryToConnect is possible.
    Store(g_outbox, p);
    return connect ? sg::Verdict::Cancel : sg::Verdict::Run;
}
}
void Install(net::Session* session) {
    g_session.store(session);
    if (g_installed) return;
    g_installed = true;
    element::scan_hub::Register({"cord", nullptr, &Ready, &MatchType, &Begin, &Match, &Complete, 2, &Classes});
    sg::WatchClassName(L"cord_C", L"connect", 0x434F0001, &OnVerb, nullptr);
    sg::WatchClassName(L"cord_C", L"disconnect", 0x434F0002, &OnVerb, nullptr);
    sg::WatchClassName(L"cord_C", L"ExecuteUbergraph_cord", 0x434F0005, &OnGraph, nullptr);
}
void Tick() {
    auto* s = g_session.load();
    if (!s || !s->connected() || g_ticking) return;
    struct Guard { Guard(){g_ticking=true;} ~Guard(){g_ticking=false;} } guard;
    ResetWorld(); sg::ResolvePendingNames();
    // Intents flush before the canonical repaint, so a local connect is never mistaken for
    // the old host state. Send failures retain the latest desired state for retry.
    for (auto it = g_outbox.begin(); it != g_outbox.end();) {
        if (s->SendReliable(net::ReliableKind::CordIntent, &*it, sizeof(*it))) it = g_outbox.erase(it);
        else ++it;
    }
    while (!g_releases.empty()) {
        const auto p = g_releases.front();
        if (!s->SendReliable(net::ReliableKind::PropRelease, &p, sizeof(p))) break;
        g_releases.pop_front();
    }
    const auto now = active_drive::NowMs();
    // Lost stream / disconnected owner: release the display even if no edge arrived.
    // A timeout does not close the generation; a delayed live stream may resume.
    for (auto& row : g_remoteDrags) if (row.actor.Get() && now-row.at > 1500) {
        const auto ref = row.actor;
        row.actor.Reset();
        RestoreHead(ref);
        if (g_session.load() != s || !s->connected()) return;
    }
    if (now < g_next) return;
    g_next = now+250;
    if (Client()) {
        g_pending.Prune(now);
        const auto pending=g_pending.Rows(); // callbacks may replace/clear canonical rows
        ResolvePass pass;
        for (const auto& row : pending) {
            if (!pass.Current()) return;
            Paint(row.p,pass);
        }
    } else {
        for (auto it = g_sent.begin(); it != g_sent.end();)
            if (!it->actor.Alive()) it = g_sent.erase(it); else ++it;
        for (auto& r : g_actors) if (void* a = r.Get()) if (C::IsCord(a)) Publish(a, -1);
    }
}
void Apply(const net::CordStatePayload& p) {
    if (!Client() || !W::Valid(p)) return;
    ResetWorld();
    // Coalesce a heartbeat/join burst before resolving actors. Tick runs after
    // the message drain; no separate world search for every incoming packet.
    if (g_pending.Observe(p, active_drive::NowMs())) g_next=0;
}
void OnIntent(const net::CordStatePayload& p, uint8_t senderSlot) {
    auto* s = g_session.load();
    if (!s || !s->connected() || s->role() != net::Role::Host ||
        senderSlot < 1 || senderSlot >= net::kMaxPeers || !W::Valid(p) || g_applyingIntent) return;
    struct Guard { Guard(){g_applyingIntent=true;} ~Guard(){g_applyingIntent=false;} } guard;
    ResetWorld();
    void* a = Resolve(p.cord, false);
    if (!a) return;
    ue_wrap::CachedObjRef cordRef; cordRef.Set(a);
    const auto epoch = g_epoch;
    const auto world = g_world;
    auto current = [&] {
        net::CordEndpoint id{};
        return g_session.load() == s && s->connected() && s->role() == net::Role::Host &&
            epoch == g_epoch && world == ue_wrap::world_identity::Generation() &&
            cordRef.Get() && Identity(a, id) && W::Same(p.cord, id);
    };
    auto target = element::IntentTarget::ForClientIntent(*s, senderSlot, 500.f);
    bool ok = false;
    if (!C::Dragging(a)) {
        // The player handles the plug at the socket. The appliance may be at the
        // other end of a long cable, beyond the player's ordinary interaction reach.
        void* socket = p.socket.kind ? Resolve(p.socket, true) : C::Socket(a);
        if (!p.socket.kind && !socket) ok = true; // already unplugged, no mutation
        else if (socket && (!p.socket.kind || GuardReady())) {
            ue_wrap::CachedObjRef socketRef; socketRef.Set(socket);
            net::CordEndpoint socketId{};
            if (!Identity(socket, socketId)) return;
            auto endpointsCurrent = [&] {
                net::CordEndpoint id{};
                return current() && socketRef.Get() && Identity(socket, id) &&
                    W::Same(socketId, id) && !C::Dragging(a);
            };
            const bool allowed = bool(target.Authorize(socket));
            if (!endpointsCurrent()) return;
            if (allowed) {
                if (!p.socket.kind) {
                    // A callback must not redirect this unplug to another connection.
                    if (C::Socket(a) == socket) ok = C::Disconnect(a);
                } else {
                    const bool inReach = C::InCableReach(a, socket);
                    if (!endpointsCurrent()) return;
                    if (inReach) {
                        bool detached = true;
                        if (C::Socket(a) && (C::Socket(a) != socket || C::Occupant(socket) != a)) {
                            // Do not unplug a working appliance for an occupied target.
                            if (void* occupant = C::Occupant(socket); occupant && occupant != a) detached = false;
                            else {
                                detached = C::Disconnect(a);
                                if (!endpointsCurrent()) return;
                                detached = detached && !C::Socket(a);
                            }
                        }
                        if (detached) ok = C::Connect(a, socket);
                    }
                }
            }
        }
    }
    if (!current()) return;
    UE_LOGI("cord: peer %u connect=%d applied=%d", senderSlot, p.socket.kind != 0, ok);
    Publish(a, -1);
    if (current()) Publish(a, senderSlot); // refusal also restores the client's local copy
}
void SendAllToSlot(int slot) {
    auto* s = g_session.load();
    if (!s || s->role() != net::Role::Host || slot < 1 || slot >= net::kMaxPeers) return;
    ResetWorld();
    for (auto& r : g_actors) if (void* a = r.Get()) if (C::IsCord(a)) Publish(a, slot);
}
bool IsDragPose(const net::PropPoseSnapshot& pose) {
    constexpr size_t prefixLen = sizeof(kDragPosePrefix) / sizeof(kDragPosePrefix[0]) - 1;
    if (pose.key.len <= prefixLen || pose.key.len > sizeof(pose.key.data)) return false;
    for (size_t i = 0; i < prefixLen; ++i)
        if (pose.key.data[i] != static_cast<char>(kDragPosePrefix[i])) return false;
    return true;
}
bool UpdateLocalDragPose(net::Session& session, bool anotherPropIsHeld) {
    if (anotherPropIsHeld) {
        EndLocalDrag(session, false);
        return false; // the ordinary held-prop stream owns this one-per-player channel
    }
    void* cord = g_localDragActive ? g_localDragCord.Get() : nullptr;
    if (!cord || !C::Dragging(cord)) {
        cord = nullptr;
        for (auto& ref : g_actors) {
            void* candidate = ref.Get();
            if (candidate && C::IsCord(candidate) && C::Dragging(candidate)) {
                cord = candidate;
                break;
            }
        }
    }
    if (!cord) {
        EndLocalDrag(session, true);
        return false;
    }
    if (g_localDragActive && g_localDragCord.Get() != cord) EndLocalDrag(session, true);
    if (!g_localDragActive && g_releases.size() >= 32) return false;
    const std::wstring identity = coop::element::PortableWireKey(cord);
    ue_wrap::FVector location{};
    ue_wrap::FRotator rotation{};
    if (identity.empty() || !C::ReadHeadPose(cord, location, rotation)) return false;
    const bool newHold = !g_localDragActive || g_localDragCord.Get() != cord;
    if (newHold) {
        ++g_localDragGen;
        if (!g_localDragGen) ++g_localDragGen;
        g_localDragCord.Set(cord);
    }
    std::wstring key = kDragPosePrefix + identity;
    if (key.size() > 31) return false;
    net::PropPoseSnapshot pose{};
    net::WireKeyFromString(key, pose.key);
    pose.x = location.X; pose.y = location.Y; pose.z = location.Z;
    pose.pitch = ue_wrap::NormalizeAxis(rotation.Pitch);
    pose.yaw = ue_wrap::NormalizeAxis(rotation.Yaw);
    pose.roll = ue_wrap::NormalizeAxis(rotation.Roll);
    pose.holdGen = g_localDragGen;
    session.SetLocalPropPose(true, pose);
    g_localDragPose = pose;
    g_localDragActive = true;
    return true;
}
void ApplyDragPose(const net::PropPoseSnapshot& pose, uint8_t senderSlot) {
    if (!IsDragPose(pose) || !std::isfinite(pose.x) || !std::isfinite(pose.y) || !std::isfinite(pose.z) ||
        std::fabs(pose.x) > 1.0e6f || std::fabs(pose.y) > 1.0e6f || std::fabs(pose.z) > 1.0e6f ||
        !std::isfinite(pose.pitch) || !std::isfinite(pose.yaw) || !std::isfinite(pose.roll) ||
        std::fabs(pose.pitch) > 180.f || std::fabs(pose.yaw) > 180.f || std::fabs(pose.roll) > 180.f) return;
    auto* s = g_session.load();
    if (!s || !s->connected() || senderSlot >= net::kMaxPeers || !pose.holdGen) return;
    ResetWorld();
    auto& row = g_remoteDrags[senderSlot];
    if (row.generation && pose.holdGen != row.generation && !AfterHold(pose.holdGen, row.generation)) return;
    if (row.generation == pose.holdGen && row.closed) return;
    const bool same = row.generation == pose.holdGen &&
        row.key.len == pose.key.len && !std::memcmp(row.key.data, pose.key.data, pose.key.len);
    if (row.generation == pose.holdGen && row.generation && !same) return;
    std::wstring key;
    for (uint8_t i = sizeof(kDragPosePrefix) / sizeof(kDragPosePrefix[0]) - 1; i < pose.key.len; ++i)
        key.push_back(static_cast<wchar_t>(static_cast<unsigned char>(pose.key.data[i])));
    void* match = same ? row.actor.Get() : nullptr;
    if (!match) for (auto& ref : g_actors) {
        void* candidate = ref.Get();
        if (!candidate || !C::IsCord(candidate) || coop::element::PortableWireKey(candidate) != key) continue;
        if (match && match != candidate) return;
        match = candidate;
    }
    if (!match || C::Dragging(match) || C::Socket(match)) return;
    ue_wrap::CachedObjRef identity; identity.Set(match);
    const auto epoch = g_epoch;
    if (s->role() == net::Role::Host && senderSlot != 0) {
        void* targetActor = C::Parent(match) ? C::Parent(match) : match;
        if (!element::IntentTarget::ForClientIntent(*s, senderSlot, 1200.f).Authorize(targetActor)) return;
    }
    if (epoch != g_epoch || !identity.Get()) return;
    if (!same) {
        const auto old = row.actor;
        row = {};
        RestoreHead(old);
        if (epoch != g_epoch || !identity.Get()) return;
    }
    if (C::WriteHeadPose(match, {pose.x, pose.y, pose.z}, {pose.pitch, pose.yaw, pose.roll}) &&
        epoch == g_epoch && identity.Get()) {
        row.actor = identity; row.key = pose.key;
        row.generation = pose.holdGen; row.closed = false; row.at = active_drive::NowMs();
    }
}
bool ApplyDragRelease(const net::PropReleasePayload& p, uint8_t senderSlot) {
    net::PropPoseSnapshot probe{}; probe.key = p.key;
    if (!IsDragPose(probe)) return false;
    auto* s = g_session.load();
    if (!s || !s->connected() || senderSlot >= net::kMaxPeers || !p.holdGen) return true;
    ResetWorld();
    auto& row = g_remoteDrags[senderSlot];
    if (row.generation && p.holdGen != row.generation && !AfterHold(p.holdGen, row.generation)) return true;
    if (row.generation == p.holdGen && (row.closed || row.key.len != p.key.len ||
        std::memcmp(row.key.data, p.key.data, p.key.len))) return true;
    const auto old = row.actor;
    row = {};
    row.key = p.key; row.generation = p.holdGen; row.closed = true;
    // No pose arrived: nothing was moved. The closed generation still blocks late UDP poses.
    RestoreHead(old);
    return true;
}
void OnPeerLeft(int slot) {
    if (slot < 0 || slot >= net::kMaxPeers) return;
    ++g_epoch; // invalidate any intent interrupted by this slot's departure/reuse
    const auto old = g_remoteDrags[slot].actor;
    g_remoteDrags[slot] = {};
    RestoreHead(old);
}
void OnDisconnect() {
    g_localDragCord.Reset(); g_localDragActive = false; g_localDragGen = 0;
    ++g_epoch;
    g_remoteDrags = {}; g_releases.clear(); g_localDragPose = {};
    g_session.store(nullptr); g_actors.clear(); g_scratch.clear(); g_sent.clear();
    g_pending.Clear(); g_outbox.clear(); g_next = 0; g_world = 0;
}
}
