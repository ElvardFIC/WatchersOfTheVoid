#include "coop/world/world_sound_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>

namespace coop::world_sound_sync {
namespace {
namespace R = ue_wrap::reflection;
namespace H = ue_wrap::ufunction_hook;
UE_FACT_FN(kRowPlaySound2D, "world_sound_sync", L"GameplayStatics", L"PlaySound2D", Dispatch);
UE_FACT_FN(kRowSpawnSoundAttached, "world_sound_sync", L"GameplayStatics", L"SpawnSoundAttached", Dispatch);
UE_FACT_FN(kRowPlaySoundAtLocation, "world_sound_sync", L"GameplayStatics", L"PlaySoundAtLocation", Dispatch);
struct Sound { const wchar_t* name; const wchar_t* type; float volume, pitch; bool spatial; };
constexpr Sound kSounds[] = {
    {L"earthTP_in", L"SoundWave", 1.5f, 0.f, false},
    {L"earthTP_out", L"SoundWave", 1.5f, 0.f, false},
    {L"outsideAlien_Cue", L"SoundCue", 2.f, 1.f, true},
    {L"plantGather_Cue", L"SoundCue", 1.f, 1.f, true},
    {L"door0_open", L"SoundWave", .25f, 1.f, false},
};
struct Pending { net::EventCuePayload payload{}; uint64_t expires = 0; };
net::Session* g_session = nullptr;
ue_wrap::CachedObjRef g_2dHook, g_attachedHook, g_locationHook;
std::array<std::deque<Pending>, players::kMaxPeers> g_out;
std::array<uint64_t, players::kMaxPeers> g_peerEpoch{};
std::deque<Pending> g_in;
uint32_t g_world = 0;
uint64_t g_epoch = 0;
bool g_flushing = false;
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
void Clear() {
    for (auto& q : g_out) q.clear();
    g_in.clear(); ++g_epoch; g_world = ue_wrap::world_identity::Generation();
}
void World() { if (g_world != ue_wrap::world_identity::Generation()) Clear(); }
bool As(net::Role role) { return g_session && g_session->running() && g_session->connected() && g_session->role() == role; }
bool Source(void* actor, const wchar_t* cls) {
    return As(net::Role::Host) && actor && R::IsLive(actor) && (!cls || R::ClassNameOf(actor) == cls) &&
        ue_wrap::world_identity::CurrentWorld() && ue_wrap::world_identity::WorldOf(actor) == ue_wrap::world_identity::CurrentWorld();
}
void Queue(uint32_t id, const ue_wrap::FVector& pos = {}) {
    World();
    Pending p{{id, pos.X, pos.Y, pos.Z}, Now() + 10000};
    for (int slot = 1; slot < players::kMaxPeers; ++slot) {
        if (!g_session->HasPeerConn(slot) || !g_session->IsSlotWorldReady(slot)) continue;
        if (g_out[slot].size() >= 32) { UE_LOGW("world_sound: peer %d cue queue full", slot); continue; }
        g_out[slot].push_back(p);
    }
}
// Observe the completed native call, rather than evaluating its bytecode arguments
// twice in a native pre-hook. The only two PlaySound2D calls in newsky are cooked
// constants: tp's entry 5062 and the latent continuation 437 (0.9.0n).
void Sound2DPost(void*, void* source, void*) {
    if (Source(source, L"faucet_C")) {
        const auto frame = H::CurrentCallerFrame();
        void* owner = frame.function ? R::OuterOf(frame.function) : nullptr;
        // The faucet ubergraph has exactly one PlaySound2D: action 5's click,
        // after active/upd. Observing it also covers host-validated guest use.
        if (owner && R::NameEquals(R::NameOf(owner), L"faucet_C") &&
            R::NameEquals(R::NameOf(frame.function), L"ExecuteUbergraph_faucet")) Queue(kFaucet);
        return;
    }
    if (!Source(source, L"newsky_C")) return;
    const auto frame = H::CurrentCallerFrame();
    if (!frame.function || !frame.locals || !R::NameEquals(R::NameOf(frame.function), L"ExecuteUbergraph_newsky")) return;
    void* owner = R::OuterOf(frame.function);
    if (!owner || !R::NameEquals(R::NameOf(owner), L"newsky_C")) return;
    const int off = R::FindParamOffset(frame.function, L"EntryPoint"), size = R::FunctionFrameSize(frame.function);
    if (off < 0 || off > size || size - off < int(sizeof(int32_t))) return;
    int32_t entry = 0; std::memcpy(&entry, frame.locals + off, sizeof(entry));
    if (entry == 5062) Queue(kEarthIn);
    else if (entry == 437) Queue(kEarthOut);
}
// noiser.timer's sole SpawnSoundAttached uses outsideAlien_Cue, volume 2,
// pitch 1 and zero offset on the capsule. Capture the actual host position.
void AttachedPost(void*, void* source, void* component) {
    if (!component || !Source(source, L"noiser_C")) return;
    World(); const auto epoch = g_epoch;
    ue_wrap::CachedObjRef ref; ref.Set(source);
    ue_wrap::FVector pos{};
    if (!ue_wrap::engine::TryGetActorLocation(source, pos) || epoch != g_epoch || !As(net::Role::Host) || !ref.Get() ||
        !std::isfinite(pos.X) || !std::isfinite(pos.Y) || !std::isfinite(pos.Z)) return;
    if (g_world != ue_wrap::world_identity::Generation()) { World(); return; }
    Queue(kAlien, pos);
}
void LocationPost(void*, void* source, void*) {
    if (!Source(source, nullptr) || ue_wrap::placed_save::Type(source) != ue_wrap::placed_save::Kind::Plant) return;
    const auto frame = H::CurrentCallerFrame();
    if (!frame.function || !R::NameEquals(R::NameOf(frame.function), L"harvestSound")) return;
    void* owner = R::OuterOf(frame.function);
    if (!owner || !R::NameEquals(R::NameOf(owner), L"growingPlant_C")) return;
    World(); const auto epoch = g_epoch;
    ue_wrap::CachedObjRef ref; ref.Set(source);
    ue_wrap::FVector pos{};
    if (!ue_wrap::engine::TryGetActorLocation(source, pos) || epoch != g_epoch || !As(net::Role::Host) || !ref.Get() ||
        !std::isfinite(pos.X) || !std::isfinite(pos.Y) || !std::isfinite(pos.Z)) return;
    if (g_world != ue_wrap::world_identity::Generation()) { World(); return; }
    pos.Z += 25.f; // growingPlant.harvestSound's cooked offset
    Queue(kPlantGather, pos);
}
bool Play(const net::EventCuePayload& p) {
    const auto& sound = kSounds[p.cueId - kEarthIn];
    const auto epoch = g_epoch;
    void* local = players::Registry::Get().Local();
    if (!local) return false;
    ue_wrap::CachedObjRef ref; ref.Set(local);
    void* asset = R::FindObject(sound.name, sound.type);
    void* cdo = R::FindClassDefaultObject(L"GameplayStatics");
    void* attenuation = sound.spatial ? R::FindObject(L"att_default", L"SoundAttenuation") : nullptr;
    if (!asset || !cdo || (sound.spatial && !attenuation)) return false;
    ue_wrap::ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(cdo), sound.spatial ? L"PlaySoundAtLocation" : L"PlaySound2D"));
    if (!f.valid() || !f.Set<void*>(L"WorldContextObject", local) || !f.Set<void*>(L"Sound", asset) ||
        !f.Set<float>(L"VolumeMultiplier", sound.volume) || !f.Set<float>(L"PitchMultiplier", sound.pitch) ||
        !f.Set<float>(L"StartTime", 0.f) || !f.Set<void*>(L"ConcurrencySettings", nullptr) || !f.Set<void*>(L"OwningActor", nullptr)) return false;
    if (sound.spatial) {
        if (!f.Set<ue_wrap::FVector>(L"Location", {p.x, p.y, p.z}) || !f.Set<ue_wrap::FRotator>(L"Rotation", {}) ||
            !f.Set<void*>(L"AttenuationSettings", attenuation)) return false;
    } else if (!f.Set<bool>(L"bIsUISound", true)) return false;
    if (epoch != g_epoch || g_world != ue_wrap::world_identity::Generation() || !As(net::Role::Client) || !ref.Get()) return false;
    return ue_wrap::Call(cdo, f);
}
void Flush() {
    if (g_flushing) return;
    g_flushing = true; struct Busy { ~Busy() { g_flushing = false; } } busy;
    World(); const auto epoch = g_epoch, now = Now();
    if (As(net::Role::Host)) {
        // FIFO per peer; a full send queue never blocks another recipient.
        for (int slot = 1, work = 0; slot < players::kMaxPeers && work < 8; ++slot) {
            auto& queue = g_out[slot];
            if (!g_session->HasPeerConn(slot) || !g_session->IsSlotWorldReady(slot)) { queue.clear(); continue; }
            const auto peerEpoch = g_peerEpoch[slot];
            for (int peerWork = 0; !queue.empty() && work < 8 && peerWork < 2; ++peerWork) {
                const auto p = queue.front();
                if (now >= p.expires) { queue.pop_front(); continue; }
                ++work;
                const bool sent = g_session->SendReliableToSlot(slot, net::ReliableKind::EventCue, &p.payload, sizeof(p.payload));
                if (epoch != g_epoch || !As(net::Role::Host)) return;
                if (peerEpoch != g_peerEpoch[slot]) break;
                if (!sent) break;
                queue.pop_front();
            }
        }
    } else if (As(net::Role::Client)) {
        for (int work = 0; !g_in.empty() && work < 4; ++work) {
            const auto p = g_in.front();
            if (now >= p.expires) { g_in.pop_front(); continue; }
            const bool played = Play(p.payload);
            if (epoch != g_epoch || !As(net::Role::Client)) return;
            if (!played) break;
            g_in.pop_front();
        }
    }
}
}
void Install(net::Session* session) {
    if (!ue_wrap::game_thread::IsGameThread()) return;
    if (g_session != session) { g_session = session; Clear(); }
    if (!g_2dHook.Get()) {
        auto fn = ue_wrap::facts::Resolve(kRowPlaySound2D);
        if (fn && H::InstallPostHook(fn, &Sound2DPost)) g_2dHook.Set(fn.Fn());
    }
    if (!g_attachedHook.Get()) {
        auto fn = ue_wrap::facts::Resolve(kRowSpawnSoundAttached);
        if (fn && H::InstallPostHook(fn, &AttachedPost)) g_attachedHook.Set(fn.Fn());
    }
    if (!g_locationHook.Get()) {
        auto fn = ue_wrap::facts::Resolve(kRowPlaySoundAtLocation);
        if (fn && H::InstallPostHook(fn, &LocationPost)) g_locationHook.Set(fn.Fn());
    }
    Flush();
}
bool OnReliable(const net::EventCuePayload& p) {
    if (p.cueId < kEarthIn || p.cueId > kFaucet) return false;
    if (!ue_wrap::game_thread::IsGameThread() || !As(net::Role::Client)) return true;
    World();
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
        std::fabs(p.x) > 2'000'000.f || std::fabs(p.y) > 2'000'000.f || std::fabs(p.z) > 2'000'000.f ||
        ((p.cueId <= kEarthOut || p.cueId == kFaucet) && (p.x || p.y || p.z))) return true;
    if (g_in.size() >= 32) { UE_LOGW("world_sound: playback queue full"); return true; }
    g_in.push_back({p, Now() + 10000});
    Flush(); return true;
}
void OnPeerLeftSlot(int slot) { if (slot > 0 && slot < players::kMaxPeers) { g_out[slot].clear(); ++g_peerEpoch[slot]; } }
void OnDisconnect() { g_session = nullptr; Clear(); }
}
