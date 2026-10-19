#include "coop/creatures/grayboar_turn.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

namespace coop::grayboar_turn {
namespace {
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
net::Session* g_session = nullptr;
uint32_t g_world = 0, g_sequence = 0, g_seen = 0;
uint64_t g_epoch = 0;
bool g_watched = false, g_applying = false;
constexpr size_t kLimit = 128;
constexpr uint64_t kLifetime = 1500, kInterval = 100, kHeartbeat = 500;
struct Host {
    uint32_t eid = 0;
    ue_wrap::CachedObjRef actor;
    float turn = 0, published = 0;
    uint64_t sampled = 0, next = 0, sent = 0;
    bool retry = true;
};
struct Client {
    uint32_t eid = 0;
    float from = 0, to = 0;
    uint64_t received = 0;
    ue_wrap::CachedObjRef actor;
};
std::vector<Host> g_host;
std::vector<Client> g_client;
size_t g_cursor = 0;
uint64_t Now() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
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
void* MirrorActor(uint32_t eid) {
    auto* element = element::Registry::Get().Get(eid);
    return element && element->IsMirror() ? element->LiveActor() : nullptr;
}
bool Stale(const Client& row, uint64_t now) {
    return now - row.received > kLifetime || (row.actor.Raw() &&
        (!row.actor.Get() || MirrorActor(row.eid) != row.actor.Raw()));
}
void* Object(void* owner, const wchar_t* name) {
    const int off = R::FindPropertyOffset(R::ClassOf(owner), name);
    void* value = nullptr;
    if (off >= 0) std::memcpy(&value, static_cast<uint8_t*>(owner) + off, sizeof(value));
    return value && R::IsLive(value) ? value : nullptr;
}
float Value(const Client& row, uint64_t now) {
    if (now - row.received > kLifetime) return 0;
    const float alpha = std::min(1.f, float(now - row.received) / float(kInterval));
    return row.from + (row.to - row.from) * alpha;
}
void AfterAnimation(const SG::Call& call) {
    if (g_applying || !Current() || !IsType(call.object, L"grayboar_Skeleton_AnimBlueprint_C")) return;
    void* actor = Object(call.object, L"grayBoar");
    if (!IsType(actor, L"grayboar_C")) return;
    auto& registry = element::Registry::Get();
    const uint32_t eid = registry.EidForActor(actor);
    if (!eid || eid >= element::kHostRangeSize) return;
    auto* element = registry.Get(eid);
    if (!element || element->LiveActor() != actor) return;
    const int off = R::FindPropertyOffset(R::ClassOf(call.object), L"turn");
    if (off < 0) return;
    const auto now = Now();
    if (g_session->role() == net::Role::Host) {
        if (element->IsMirror()) return;
        float turn = 0;
        std::memcpy(&turn, static_cast<uint8_t*>(call.object) + off, sizeof(turn));
        if (!std::isfinite(turn) || std::abs(turn) > 1.f) return;
        auto it = std::find_if(g_host.begin(), g_host.end(), [=](const Host& r){return r.eid == eid;});
        if (it == g_host.end()) {
            if (g_host.size() >= kLimit) return;
            g_host.push_back({}); it = g_host.end() - 1; it->eid = eid; it->actor.Set(actor);
        } else if (!it->actor.Is(actor)) {
            *it = {}; it->eid = eid; it->actor.Set(actor);
        }
        it->turn = turn; it->sampled = now;
        return;
    }
    if (!element->IsMirror()) return;
    auto it = std::find_if(g_client.begin(), g_client.end(), [=](const Client& r){return r.eid == eid;});
    float turn = 0;
    if (it != g_client.end()) {
        if (Stale(*it, now))g_client.erase(it);
        else {
            if (!it->actor.Raw())it->actor.Set(actor);
            turn = Value(*it, now);
        }
    }
    // Native BlueprintUpdateAnimation has computed local steering and its pitch.
    // Replace only this cosmetic scalar before bone evaluation, plus the pitch
    // derived from it. Speed, montage state, AI and physical heading stay native
    // or on their existing authoritative lanes.
    std::memcpy(static_cast<uint8_t*>(call.object) + off, &turn, sizeof(turn));
    void* audio = Object(actor, L"audio_loop_turn");
    if (!audio) return;
    ue_wrap::CachedObjRef body, anim, sound;
    body.Set(actor); anim.Set(call.object); sound.Set(audio);
    const auto epoch = g_epoch;
    struct Guard {Guard(){g_applying=true;} ~Guard(){g_applying=false;}} guard;
    ue_wrap::ParamFrame pitch(R::FindDispatchFunctionCached(R::ClassOf(audio), L"SetPitchMultiplier"));
    if (pitch.valid() && pitch.Set<float>(L"NewPitchMultiplier", 0.5f + 1.5f * std::abs(turn)) &&
        epoch == g_epoch && Current() && body.Get() && anim.Get() && sound.Get())
        ue_wrap::Call(audio, pitch);
}
} // namespace

void Install(net::Session* session) {
    const auto world = ue_wrap::world_identity::Generation();
    if (session != g_session || world != g_world) {
        OnDisconnect(); g_session = session; g_world = world;
    }
    if (!g_watched) g_watched = SG::WatchClassName(L"grayboar_Skeleton_AnimBlueprint_C",
        L"BlueprintUpdateAnimation", 0x47425455, nullptr, &AfterAnimation);
}
void Receive(const net::GrayboarTurnPayload& packet, int sender) {
    if (!Current() || g_session->role() != net::Role::Client || sender != 0 ||
        !packet.eid || packet.eid >= element::kHostRangeSize || !packet.sequence ||
        !std::isfinite(packet.turn) || std::abs(packet.turn) > 1.f ||
        (g_seen && static_cast<int32_t>(packet.sequence - g_seen) <= 0)) return;
    const auto now = Now();
    // Retire stale samples before admitting another body; they must neither
    // consume capacity nor seed a replacement body's interpolation.
    std::erase_if(g_client, [=](const Client& row){return Stale(row, now);});
    auto it = std::find_if(g_client.begin(), g_client.end(), [&](const Client& r){return r.eid == packet.eid;});
    if (it == g_client.end()) {
        if (g_client.size() >= kLimit) return;
        g_client.push_back({}); it = g_client.end() - 1; it->eid = packet.eid;
        it->received = now;
    }
    const float from = Value(*it, now);
    *it = {packet.eid, from, packet.turn, now};
    void* actor = MirrorActor(packet.eid);
    it->actor.Set(IsType(actor, L"grayboar_C") ? actor : nullptr);g_seen = packet.sequence;
}
void Tick() {
    if (!Current()) return;
    const auto now = Now();
    if (g_session->role() == net::Role::Client) {
        std::erase_if(g_client, [=](const Client& r){return Stale(r, now);});
        return;
    }
    std::erase_if(g_host, [=](const Host& r){return !r.actor.Get() || now - r.sampled > kLifetime;});
    const size_t count = g_host.size();
    int budget = 4;
    // Fair bounded publication, latest sample only; failed sends never accumulate
    // a history of obsolete steering. Heartbeat supplies stationary late joiners.
    for (size_t visited = 0; visited < count && budget; ++visited) {
        auto& row = g_host[g_cursor++ % count];
        if (now < row.next || (!row.retry && std::abs(row.turn - row.published) < 0.01f && now - row.sent < kHeartbeat)) continue;
        auto* element = element::Registry::Get().Get(row.eid);
        if (!element || element->IsMirror() || element->LiveActor() != row.actor.Get()) continue;
        uint32_t peers = 0;
        for (int slot = 1; slot < net::kMaxPeers; ++slot)
            if (g_session->IsSlotWorldReady(slot)) peers |= 1u << slot;
        if (!peers) continue;
        if (!++g_sequence) ++g_sequence;
        net::GrayboarTurnPayload packet{row.eid, g_sequence, row.turn};
        row.retry = false; row.next = now + kInterval; --budget;
        for (int slot = 1; slot < net::kMaxPeers; ++slot) if (peers & (1u << slot))
            if (!g_session->SendReliableToSlot(slot, net::ReliableKind::GrayboarTurn, &packet, sizeof(packet))) row.retry = true;
        row.sent = now; row.published = row.turn;
    }
}
void OnDisconnect() {
    ++g_epoch; g_session = nullptr; g_world = g_sequence = g_seen = 0;
    g_host.clear(); g_client.clear(); g_cursor = 0;
}
} // namespace coop::grayboar_turn
