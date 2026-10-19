#include "coop/creatures/kavotia_sync.h"
#include "coop/creatures/reliable_action_queue.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/types.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

namespace coop::kavotia_sync {
namespace {
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
using Packet = net::KavotiaStatePayload;
net::Session* g_session = nullptr;
uint32_t g_world = 0, g_sequence = 0, g_seen = 0;
uint64_t g_epoch = 0;
bool g_ticking = false;
constexpr size_t kLimit = 128;
constexpr uint64_t kLifetime = 1500, kInterval = 100, kHeartbeat = 500;
constexpr const wchar_t* kAuthorityNames[] = {
    L"ExecuteUbergraph_kavotia", L"alert", L"updateTarget", L"sensePawns"
};
std::array<bool, std::size(kAuthorityNames)> g_watched{};
bool g_animationWatched = false;
bool g_actorTickWatched = false;
struct Host {
    Packet packet{}, published{};
    ue_wrap::CachedObjRef actor;
    uint64_t sampled = 0, next = 0, sent = 0;
};
struct Client {
    Packet packet{};
    ue_wrap::CachedObjRef actor, audio, windup;
    uint64_t received = 0;
    bool audioKnown = false, charging = false, windupKnown = false, winding = false;
    bool pitchKnown = false;
    float pitch = 0;
};
struct Cue {
    Packet packet{};
    uint64_t expires = 0;
    uint32_t peers = 0;
    ue_wrap::CachedObjRef actor;
};
std::vector<Host> g_host;
std::vector<Client> g_client;
std::deque<Cue> g_out;
size_t g_cursor = 0;
uint64_t Now() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
uint64_t Age(uint64_t now, uint64_t then) {return now >= then ? now - then : 0;}
bool Current() {
    return g_session && g_session->running() && g_session->connected() &&
        g_world == ue_wrap::world_identity::Generation();
}
bool IsType(void* object, const wchar_t* name) {
    if (!object || !R::IsLive(object)) return false;
    void* cls = R::ClassOf(object);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls))
        if (R::NameEquals(R::NameOf(cls), name)) return true;
    return false;
}
template<class T> bool Read(void* object, const wchar_t* name, T& out) {
    const auto off = R::FindPropertyOffset(R::ClassOf(object), name);
    if (off < 0) return false;
    std::memcpy(&out, static_cast<uint8_t*>(object) + off, sizeof(out));
    return true;
}
void* Object(void* owner, const wchar_t* name) {
    void* out = nullptr;
    return owner && Read(owner, name, out) && out && R::IsLive(out) ? out : nullptr;
}
bool Bool(void* object, const wchar_t* name, bool& out) {
    int32_t off = -1; uint8_t mask = 0;
    if (!object || !R::FindBoolProperty(R::ClassOf(object), name, off, mask) || off < 0 || !mask) return false;
    out = (static_cast<uint8_t*>(object)[off] & mask) != 0;
    return true;
}
bool PutBool(void* object, const wchar_t* name, bool value) {
    int32_t off = -1; uint8_t mask = 0;
    if (!R::FindBoolProperty(R::ClassOf(object), name, off, mask) || off < 0 || !mask) return false;
    auto& byte = static_cast<uint8_t*>(object)[off];
    if (value) byte |= mask; else byte &= static_cast<uint8_t>(~mask);
    return true;
}
bool Valid(const Packet& p) {
    return p.eid && p.eid < element::kHostRangeSize && !(p.flags & ~uint8_t{15}) &&
        std::isfinite(p.chargePitch) && p.chargePitch >= 0.5f && p.chargePitch <= 2.f &&
        std::isfinite(p.targetX) && std::abs(p.targetX) <= net::kMaxCoord &&
        std::isfinite(p.targetY) && std::abs(p.targetY) <= net::kMaxCoord &&
        std::isfinite(p.targetZ) && std::abs(p.targetZ) <= net::kMaxCoord;
}
bool Same(const Packet& a, const Packet& b) {
    return a.flags == b.flags && a.targetX == b.targetX && a.targetY == b.targetY &&
        a.targetZ == b.targetZ && std::abs(a.chargePitch - b.chargePitch) < 0.01f;
}
bool Owned(uint32_t eid, void* actor, bool mirror) {
    auto* entity = element::Registry::Get().Get(eid);
    return actor && entity && entity->IsMirror() == mirror && entity->LiveActor() == actor;
}
void* Mirror(uint32_t eid) {
    auto* entity = element::Registry::Get().Get(eid);
    return entity && entity->IsMirror() ? entity->LiveActor() : nullptr;
}
bool Retired(const Client& row) {
    return row.actor.Raw() && (!row.actor.Get() || Mirror(row.packet.eid) != row.actor.Raw());
}
SG::Verdict OnAuthority(const SG::Call& call) {
    if (!Current() || g_session->role() != net::Role::Client || !IsType(call.object, L"kavotia_C"))
        return SG::Verdict::Run;
    // Kavotia is shared. This also covers FinishSpawningActor before the mirror
    // has an eid, and already-armed timers on an adopted copy. BeginPlay keeps
    // its mesh/AnimInstance setup; every later latent graph entry is refused,
    // including its move retries and the seven-shot loop. No personal class is watched.
    if (call.tag == 0) {
        int32_t entry = -1;
        const auto off = call.function ? R::FindParamOffset(call.function, L"EntryPoint") : -1;
        const auto size = call.function ? R::FunctionFrameSize(call.function) : -1;
        if (call.locals && off >= 0 && size >= 4 && off <= size - 4)
            std::memcpy(&entry, call.locals + off, sizeof(entry));
        if (entry == 3961) return SG::Verdict::Run;
    }
    return SG::Verdict::Cancel;
}
void AfterAnimation(const SG::Call& call) {
    if (!Current() || !IsType(call.object, L"kavotia_Skeleton_AnimBlueprint_C")) return;
    void* actor = Object(call.object, L"kavotia");
    void* mesh = Object(actor, L"Mesh");
    if (!IsType(actor, L"kavotia_C") || !mesh || Object(call.object, L"mesh") != mesh) return;
    const auto eid = element::Registry::Get().EidForActor(actor);
    if (!eid || eid >= element::kHostRangeSize) return;
    const auto now = Now();
    if (g_session->role() == net::Role::Host) {
        if (!Owned(eid, actor, false)) return;
        Packet p{}; p.eid = eid;
        bool laying = false, aiming = false, charging = false, winding = false;
        auto* audio = Object(actor, L"charging");
        auto* windup = Object(actor, L"eff_tentacleBallShootWindup");
        ue_wrap::FVector target{};
        if (!Bool(call.object, L"isLaying", laying) || !Bool(call.object, L"isShooting", aiming) ||
            !Read(call.object, L"shootTarget", target) || !Bool(audio, L"bIsActive", charging) ||
            !Bool(windup, L"bIsActive", winding) || !Read(audio, L"PitchMultiplier", p.chargePitch)) return;
        p.flags = (laying ? net::kKavotiaLaying : 0) | (aiming ? net::kKavotiaAiming : 0) |
            (charging ? net::kKavotiaCharging : 0) | (winding ? net::kKavotiaWindup : 0);
        p.targetX = target.X; p.targetY = target.Y; p.targetZ = target.Z;
        if (!Valid(p)) return;
        auto it = std::find_if(g_host.begin(), g_host.end(), [=](const Host& r){return r.packet.eid == eid;});
        if (it == g_host.end()) {
            if (g_host.size() >= kLimit) return;
            g_host.push_back({}); it = g_host.end() - 1;
        }
        if (!it->actor.Is(actor)) { *it = {}; it->actor.Set(actor); }
        it->packet = p; it->sampled = now;
        return;
    }
    if (!Owned(eid, actor, true)) return;
    auto it = std::find_if(g_client.begin(), g_client.end(), [=](const Client& r){return r.packet.eid == eid;});
    const bool fresh = it != g_client.end() && Age(now, it->received) <= kLifetime && !Retired(*it);
    if (fresh && !it->actor.Raw()) it->actor.Set(actor);
    // Native locomotion still evaluates; only the host's stance/aim inputs are
    // supplied. There is no target actor assignment or attack function replay.
    const Packet p = fresh ? it->packet : Packet{};
    PutBool(call.object, L"isLaying", (p.flags & net::kKavotiaLaying) != 0);
    PutBool(call.object, L"isShooting", (p.flags & net::kKavotiaAiming) != 0);
    const ue_wrap::FVector target{p.targetX, p.targetY, p.targetZ};
    const auto off = R::FindPropertyOffset(R::ClassOf(call.object), L"shootTarget");
    if (off >= 0) std::memcpy(static_cast<uint8_t*>(call.object) + off, &target, sizeof(target));
    if (void* movement = Object(actor, L"CharacterMovement")) {
        const auto speedOff = R::FindPropertyOffset(R::ClassOf(movement), L"MaxWalkSpeed");
        const float speed = (p.flags & net::kKavotiaAiming) ? 450.f : 200.f;
        if (speedOff >= 0) std::memcpy(static_cast<uint8_t*>(movement) + speedOff, &speed, sizeof(speed));
    }
}
void AfterActorTick(const SG::Call& call) {
    if (!Current() || g_session->role() != net::Role::Host || !IsType(call.object, L"kavotia_C")) return;
    // Native actor ticks update shootTarget even when the host's mesh is culled
    // and its animation update is skipped. Other viewers must still get state.
    SG::Call sample{}; sample.object = Object(call.object, L"anim");
    AfterAnimation(sample);
}
// Only component cosmetics are called here. A copied row avoids retaining a
// vector reference across reflection, which can receive a packet or disconnect.
bool Effects(Client& row, const Packet& p) {
    void* actor = row.actor.Get();
    if (!actor || !Owned(p.eid, actor, true)) return false;
    const auto epoch = g_epoch;
    auto current = [&]{
        if (epoch != g_epoch || !Current() || g_session->role() != net::Role::Client ||
            !row.actor.Get() || !Owned(p.eid, actor, true)) return false;
        const auto it = std::find_if(g_client.begin(), g_client.end(),
            [&](const Client& r){return r.packet.eid == p.eid;});
        return it != g_client.end() && it->packet.sequence == p.sequence;
    };
    void* audio = Object(actor, L"charging");
    void* windup = Object(actor, L"eff_tentacleBallShootWindup");
    if (!row.audio.Is(audio)) {row.audio.Set(audio); row.audioKnown = row.pitchKnown = false;}
    if (!row.windup.Is(windup)) {row.windup.Set(windup); row.windupKnown = false;}
    auto sound = [&] {
        if (!audio) return false;
        auto attached = [&]{return current() && row.audio.Get() && Object(actor, L"charging") == audio;};
        const bool charging = (p.flags & net::kKavotiaCharging) != 0;
        // Stopping a stale loop must not depend on a pitch setter or on the
        // unrelated particle component being available.
        if (charging && (!row.pitchKnown || row.pitch != p.chargePitch)) {
            ue_wrap::ParamFrame pitch(R::FindDispatchFunctionCached(R::ClassOf(audio), L"SetPitchMultiplier"));
            if (!pitch.valid() || !pitch.Set<float>(L"NewPitchMultiplier", p.chargePitch) || !attached() ||
                !ue_wrap::Call(audio, pitch)) return false;
            row.pitchKnown = true; row.pitch = p.chargePitch;
            if (!attached()) return false;
        }
        if (!row.audioKnown || row.charging != charging) {
            ue_wrap::ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(audio), charging ? L"Play" : L"Stop"));
            if (!frame.valid() || (charging && !frame.Set<float>(L"StartTime", 0.f)) || !attached() ||
                !ue_wrap::Call(audio, frame)) return false;
            row.audioKnown = true; row.charging = charging;
            if (!attached()) return false;
        }
        return true;
    };
    const bool audioDone = sound();
    if (!current() || !windup) return false;
    auto attached = [&]{return current() && row.windup.Get() &&
        Object(actor, L"eff_tentacleBallShootWindup") == windup;};
    const bool winding = (p.flags & net::kKavotiaWindup) != 0;
    if (!row.windupKnown || row.winding != winding) {
        ue_wrap::ParamFrame effect(R::FindDispatchFunctionCached(R::ClassOf(windup), winding ? L"Activate" : L"Deactivate"));
        if (!effect.valid() || (winding && !effect.Set<uint8_t>(L"bReset", 1)) || !attached() ||
            !ue_wrap::Call(windup, effect)) return false;
        row.windupKnown = true; row.winding = winding;
        if (!attached()) return false;
    }
    return audioDone;
}
} // namespace

void Install(net::Session* session) {
    const auto world = ue_wrap::world_identity::Generation();
    if (session != g_session || world != g_world) {OnDisconnect(); g_session = session; g_world = world;}
    for (size_t i = 0; i < g_watched.size(); ++i)
        if (!g_watched[i]) g_watched[i] = SG::WatchClassName(L"kavotia_C", kAuthorityNames[i],
            static_cast<int>(i), &OnAuthority, nullptr);
    if (!g_animationWatched) g_animationWatched = SG::WatchClassName(L"kavotia_Skeleton_AnimBlueprint_C",
        L"BlueprintUpdateAnimation", 0, nullptr, &AfterAnimation);
    if (!g_actorTickWatched) g_actorTickWatched = SG::WatchClassName(L"kavotia_C",
        L"ReceiveTick", 0, nullptr, &AfterActorTick);
}
void Receive(const Packet& packet, int sender) {
    if (!Current() || g_session->role() != net::Role::Client || sender != 0 || !Valid(packet) ||
        !creature_actions::IsNewerSequence(packet.sequence, g_seen)) return;
    std::erase_if(g_client, [](const Client& r){return Retired(r);});
    auto it = std::find_if(g_client.begin(), g_client.end(), [&](const Client& r){return r.packet.eid == packet.eid;});
    if (it == g_client.end()) {
        if (g_client.size() >= kLimit) return;
        g_client.push_back({}); it = g_client.end() - 1;
    }
    it->packet = packet; it->received = Now();
    if (!it->actor.Raw()) {
        void* actor = Mirror(packet.eid);
        if (IsType(actor, L"kavotia_C")) it->actor.Set(actor);
    }
    g_seen = packet.sequence;
}
void Tick() {
    if (!Current() || g_ticking) return;
    struct Guard {Guard(){g_ticking = true;} ~Guard(){g_ticking = false;}} guard;
    const auto now = Now(), epoch = g_epoch;
    if (g_session->role() == net::Role::Client) {
        for (size_t i = 0; i < g_client.size();) {
            auto row = g_client[i];
            if (Retired(row)) {g_client.erase(g_client.begin() + i); continue;}
            const bool stale = Age(now, row.received) > kLifetime;
            if (!row.actor.Raw()) {
                void* actor = Mirror(row.packet.eid);
                if (IsType(actor, L"kavotia_C")) row.actor.Set(actor);
            }
            auto p = row.packet;
            if (stale) {p.flags = 0; p.chargePitch = 0.5f;}
            const bool done = Effects(row, p);
            if (epoch != g_epoch || !Current() || g_session->role() != net::Role::Client) return;
            // Reentrant receive may replace the packet. Preserve only completed
            // component operations; the new packet still needs applying.
            auto it = std::find_if(g_client.begin(), g_client.end(), [&](const Client& r){return r.packet.eid == p.eid;});
            if (it == g_client.end()) return;
            if (it->actor.Raw() && !it->actor.Is(row.actor.Raw())) {++i; continue;}
            if (!it->actor.Raw()) it->actor = row.actor;
            it->audio = row.audio; it->windup = row.windup;
            it->audioKnown = row.audioKnown; it->charging = row.charging;
            it->windupKnown = row.windupKnown; it->winding = row.winding;
            it->pitchKnown = row.pitchKnown; it->pitch = row.pitch;
            if ((stale && done && it->packet.sequence == p.sequence) || Age(now, it->received) > 2 * kLifetime)
                g_client.erase(it);
            else ++i;
        }
        return;
    }
    std::erase_if(g_host, [=](const Host& r){return !r.actor.Get() || !Owned(r.packet.eid, r.actor.Get(), false) ||
        Age(now, r.sampled) > kLifetime;});
    int budget = 4;
    const size_t count = g_host.size();
    for (size_t visited = 0; visited < count && budget; ++visited) {
        auto& row = g_host[g_cursor++ % count];
        if (now < row.next || (row.sent && Same(row.packet, row.published) && Age(now, row.sent) < kHeartbeat)) continue;
        // State coalesces: a slow peer gets the newest phase, not a backlog of
        // obsolete charge pitches. Moving this eid to the tail preserves wire sequence order.
        std::erase_if(g_out, [&](const Cue& c){return c.packet.eid == row.packet.eid;});
        const auto peers = creature_actions::QueueablePeers<net::kMaxPeers>(g_out, now, 32,
            [](int slot){return g_session->IsSlotWorldReady(slot);});
        if (!peers) continue;
        Cue cue{}; cue.packet = row.packet; cue.packet.sequence = creature_actions::NextSequence(g_sequence);
        cue.actor = row.actor; cue.expires = now + kLifetime; cue.peers = peers;
        g_out.push_back(cue); row.published = row.packet; row.sent = now; row.next = now + kInterval; --budget;
    }
    creature_actions::FlushPerPeerFifo<net::kMaxPeers>(g_out, now,
        [](const Cue& c){return c.actor.Get() && Owned(c.packet.eid, c.actor.Get(), false);},
        [](int slot){return g_session->IsSlotWorldReady(slot);},
        [epoch](const Cue& c, int slot) {
            const bool sent = g_session->SendReliableToSlot(slot, net::ReliableKind::KavotiaState, &c.packet, sizeof(c.packet));
            if (epoch != g_epoch || !Current() || g_session->role() != net::Role::Host)
                return creature_actions::SendResult::Abort;
            return sent ? creature_actions::SendResult::Sent : creature_actions::SendResult::Retry;
        });
}
void QueueConnect(int slot) {
    if (!Current() || g_session->role() != net::Role::Host || slot <= 0 || slot >= net::kMaxPeers) return;
    for (auto& row : g_host) row.sent = row.next = 0;
}
void OnPeerLeft(int slot) {
    if (slot <= 0 || slot >= net::kMaxPeers) return;
    ++g_epoch;
    creature_actions::DropPeer(g_out, slot);
}
void OnDisconnect() {
    ++g_epoch; g_session = nullptr; g_world = g_sequence = g_seen = 0;
    g_host.clear(); g_client.clear(); g_out.clear(); g_cursor = 0;
}
} // namespace coop::kavotia_sync
