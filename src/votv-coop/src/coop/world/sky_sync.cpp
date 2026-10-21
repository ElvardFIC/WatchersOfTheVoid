// coop/world/sky_sync.cpp -- see coop/world/sky_sync.h. Host-authoritative night-sky sync
// (star-dome world rotation, moon phase, sky eye and black hole).

#include "coop/world/sky_sync.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"  // coop::players::kMaxPeers
#include "coop/session/net_pump.h"          // IsInAnnouncedWorld: a client's own world load runs natively

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/skysphere.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/core/types.h"  // FRotator

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>

namespace coop::sky_sync {
namespace {

namespace SKY = ue_wrap::skysphere;
namespace sg  = ue_wrap::script_gate;

UE_FACT_FN(kRowSetEye, "sky_sync", L"newsky_C", L"setEye", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};
std::chrono::steady_clock::time_point g_lastBroadcast{};

// The dome spins very slowly (dt/32 deg/frame ~ 0.03 deg/s) and moonPhase is near-static, so a
// ~1 Hz push keeps both visually locked while staying quiet on the wire.
constexpr auto kPushInterval = std::chrono::milliseconds(1000);

// The eye: the day's noon roll runs on every peer and calls the sky's setEye, so a client refuses its own
// and shows the host's, which the lane carries. Game thread.
constexpr int kSetEyeTag = 0x534b4559;  // 'SKEY'
constexpr int kBlackHoleTag = 0x534b4248; // 'SKBH'
constexpr int kBlackHoleVisibilityTag = 0x534b4256;
bool g_eyeWatch = false, g_startWatch = false, g_visualWatch = false, g_graphWatch = false;
bool     g_applyingEye = false;          // the lane's own apply of the host's eye runs
bool     g_applyingBlackHole = false;
uint64_t g_blackHoleApplyFailed = 0;
uint64_t g_eyeRefused = 0;               // a client's own setEye calls refused this session
bool     g_saidEyeRefused = false;
uint64_t g_eyeApplyFailed = 0;           // the host's eye samples whose setEye did not run this session

// The lane's own apply, for the length of one setEye call: the gate lets exactly this call run.
struct ApplyingEye {
    ApplyingEye() { g_applyingEye = true; }
    ~ApplyingEye() { g_applyingEye = false; }
    ApplyingEye(const ApplyingEye&) = delete;
    ApplyingEye& operator=(const ApplyingEye&) = delete;
};

// newsky.tp starts the earthTp black-hole sequence (sound, latent delays,
// emails, skyVisibility), not a pawn teleport. Refuse local client starts even
// during loading; a late join receives the host's CURRENT presentation.
sg::Verdict OnBlackHolePre(const sg::Call&) {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() && s->role() == coop::net::Role::Client ?
        sg::Verdict::Cancel : sg::Verdict::Run;
}
sg::Verdict OnBlackHoleVisibilityPre(const sg::Call& call) {
    return g_applyingBlackHole ? sg::Verdict::Run : OnBlackHolePre(call);
}

sg::Verdict OnBlackHoleGraphPre(const sg::Call& call) {
    if (OnBlackHolePre(call) == sg::Verdict::Run || !call.function || !call.locals) return sg::Verdict::Run;
    const int off = ue_wrap::reflection::FindParamOffset(call.function,L"EntryPoint");
    const int size = ue_wrap::reflection::FunctionFrameSize(call.function);
    if (off < 0 || off > size || size-off < int(sizeof(int32_t))) return sg::Verdict::Run;
    int32_t entry = 0; std::memcpy(&entry,call.locals+off,sizeof(entry));
    // 0.9.0n tp and its six Delay continuations. A chain begun before this peer
    // joined must not later emit its own sound/mail or change world event state.
    // BeginPlay/init, permanent-hole setup, sky tick and moon timers stay native.
    switch (entry) {
    case 5062: case 3222: case 70: case 437: case 552: case 759: case 1023:
        return sg::Verdict::Cancel;
    default: return sg::Verdict::Run;
    }
}

// CLIENT, before newsky's setEye: the host's sky owns the eye. The lane's apply runs, and so does a call
// in a world this client has not announced ready, its own load of the host's save.
sg::Verdict OnSetEyePre(const sg::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() == coop::net::Role::Host) return sg::Verdict::Run;
    if (g_applyingEye || !call.object) return sg::Verdict::Run;
    if (!coop::net_pump::IsInAnnouncedWorld(call.object)) return sg::Verdict::Run;
    ++g_eyeRefused;
    if (!g_saidEyeRefused) {
        g_saidEyeRefused = true;
        UE_LOGI("sky_sync: a client refused its own setEye -- the host's sky owns the eye (first refusal; the "
                "rest are counted)");
    }
    return sg::Verdict::Cancel;
}

bool Finite(const coop::net::SkyStatePayload& p) {
    using namespace coop::net::sky_flags;
    return std::isfinite(p.skyPitch) && std::isfinite(p.skyYaw) &&
           std::isfinite(p.skyRoll) && std::isfinite(p.moonPhase) && p.eye <= 1 &&
           !(p.blackHoleFlags & ~kKnown) &&
           (!p.blackHoleFlags || (p.blackHoleFlags & kBlackHoleValid)) && !p.pad[0] && !p.pad[1];
}

bool MakePayload(coop::net::SkyStatePayload& out) {
    out = {};
    ue_wrap::FRotator rot{};
    float moon = 0.f;
    if (!SKY::ReadSky(rot, moon)) return false;  // sky not streamed in yet
    out.skyPitch = rot.Pitch;
    out.skyYaw   = rot.Yaw;
    out.skyRoll  = rot.Roll;
    out.moonPhase = moon;
    bool eye = false;
    SKY::ReadEye(eye);  // unread, the sky shows no eye
    out.eye = eye ? 1 : 0;
    bool hidden = false, light = false;
    if (SKY::ReadBlackHole(hidden, light)) {
        using namespace coop::net::sky_flags;
        out.blackHoleFlags = kBlackHoleValid | (hidden ? kHidden : 0) | (light ? kLightVisible : 0);
    }
    return Finite(out);  // never push NaN/inf onto the wire
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    SKY::EnsureResolved();  // retried via Tick until newsky_C loads
    if (!g_eyeWatch) g_eyeWatch = sg::WatchName(kRowSetEye,kSetEyeTag,&OnSetEyePre,nullptr);
    if (!g_startWatch) g_startWatch = sg::WatchClassName(L"newsky_C",L"tp",kBlackHoleTag,&OnBlackHolePre,nullptr);
    if (!g_visualWatch) g_visualWatch = sg::WatchClassName(L"newsky_C",L"skyVisibility",kBlackHoleVisibilityTag,&OnBlackHoleVisibilityPre,nullptr);
    if (!g_graphWatch) g_graphWatch = sg::WatchClassName(L"newsky_C",L"ExecuteUbergraph_newsky",kBlackHoleTag,&OnBlackHoleGraphPre,nullptr);
    static bool warned = false;
    if ((!g_eyeWatch || !g_startWatch || !g_visualWatch || !g_graphWatch) && !warned) {
        warned = true;
        UE_LOGE("sky_sync: authority watch registration failed; retrying");
    }
}

void OnReliable(const coop::net::SkyStatePayload& payload) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Client) return;
    if (!Finite(payload)) { UE_LOGW("sky_sync: dropping non-finite payload"); return; }
    const auto world = ue_wrap::world_identity::Generation();
    SKY::ApplySky(ue_wrap::FRotator{payload.skyPitch, payload.skyYaw, payload.skyRoll}, payload.moonPhase);
    if (s != g_session.load(std::memory_order_acquire) || !s->connected() ||
        s->role() != coop::net::Role::Client || world != ue_wrap::world_identity::Generation()) return;
    if (payload.blackHoleFlags & coop::net::sky_flags::kBlackHoleValid) {
        struct Applying {
            bool previous = g_applyingBlackHole;
            Applying() { g_applyingBlackHole = true; }
            ~Applying() { g_applyingBlackHole = previous; }
        } scope;
        if (!SKY::ApplyBlackHole((payload.blackHoleFlags & coop::net::sky_flags::kHidden) != 0,
                                (payload.blackHoleFlags & coop::net::sky_flags::kLightVisible) != 0) &&
            g_blackHoleApplyFailed++ == 0)
            UE_LOGW("sky_sync: black-hole presentation not ready; next host sample retries");
    }
    if (s != g_session.load(std::memory_order_acquire) || !s->connected() ||
        s->role() != coop::net::Role::Client || world != ue_wrap::world_identity::Generation()) return;
    // The eye through the sky's own setEye, which swaps the moon's texture; the gate lets this call run.
    // A call that did not run is said once and counted; each next sample tries again.
    const bool want = payload.eye != 0;
    bool cur = false;
    if (SKY::ReadEye(cur) && cur != want) {
        bool ok = false;
        {
            ApplyingEye scope;
            ok = SKY::CallSetEye(want);
        }
        if (ok)
            UE_LOGI("sky_sync: applied the host's eye %d", want ? 1 : 0);
        else if (g_eyeApplyFailed++ == 0)
            UE_LOGW("sky_sync: the host's eye %d did not apply -- the sky's setEye did not run (first failure; the "
                    "rest are counted, and each next sample tries again)", want ? 1 : 0);
    }
    static int s_n = 0;
    if ((s_n++ % 10) == 0)  // ~every 10s -- confirm convergence, not spam
        UE_LOGI("sky_sync: applied host sky yaw=%.1f moonPhase=%.3f", payload.skyYaw, payload.moonPhase);
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;
    if (peerSlot < 0 || peerSlot >= static_cast<int>(coop::players::kMaxPeers)) return;
    coop::net::SkyStatePayload p{};
    if (!MakePayload(p)) return;
    s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::SkyState, &p, sizeof(p));
    UE_LOGI("sky_sync: connect-snapshot -- sent sky (yaw=%.1f moonPhase=%.3f eye=%d) to slot %d",
            p.skyYaw, p.moonPhase, p.eye, peerSlot);
}

void Tick() {
    if ((!g_eyeWatch || !g_startWatch || !g_visualWatch || !g_graphWatch) && g_session.load(std::memory_order_acquire))
        Install(g_session.load(std::memory_order_acquire));
    sg::ResolvePendingNames();  // the eye gate's name, on the game thread
    if (!SKY::EnsureResolved()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;  // host broadcasts; client applies on receipt
    const auto now = std::chrono::steady_clock::now();
    if (now - g_lastBroadcast < kPushInterval) return;
    // A joiner is connected for tens of seconds before it has a world, and SkyState is not on the
    // pre-world allowlist, so the fan-out drops it per slot and reports no delivery. That is a
    // vacuous success for this lane, not a failure: QueueConnectBroadcastForSlot seeds the sky the
    // moment the slot goes world-ready. Asking with nobody eligible is what turned a real warning
    // into 24 false alarms per join. Gated after the interval so it costs nothing per frame.
    if (!s->AnyWorldReadyPeer()) return;
    coop::net::SkyStatePayload p{};
    if (!MakePayload(p)) return;
    g_lastBroadcast = now;
    if (s->SendReliable(coop::net::ReliableKind::SkyState, &p, sizeof(p))) {
        static int s_n = 0;
        if ((s_n++ % 10) == 0)  // ~every 10s
            UE_LOGI("sky_sync: host sky yaw=%.1f moonPhase=%.3f eye=%d", p.skyYaw, p.moonPhase, p.eye);
    } else {
        UE_LOGW("sky_sync: SendReliable(SkyState) failed");
    }
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_lastBroadcast = {};
    if (g_eyeRefused)
        UE_LOGI("sky_sync: session end -- a client refused %llu setEye call(s) of its own",
                static_cast<unsigned long long>(g_eyeRefused));
    if (g_eyeApplyFailed)
        UE_LOGW("sky_sync: session end -- %llu sample(s) of the host's eye did not apply",
                static_cast<unsigned long long>(g_eyeApplyFailed));
    g_eyeRefused = 0;
    g_saidEyeRefused = false;
    g_eyeApplyFailed = 0;
    g_blackHoleApplyFailed = 0;
}

}  // namespace coop::sky_sync
