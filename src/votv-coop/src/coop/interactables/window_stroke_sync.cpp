// coop/interactables/window_stroke_sync.cpp -- see coop/interactables/window_stroke_sync.h.

#include "coop/interactables/window_stroke_sync.h"

#include "coop/config/config.h"
#include "coop/element/object_scan_hub.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/devices/window_canvas.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace coop::window_stroke_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace WC = ue_wrap::window_canvas;
using Clock = std::chrono::steady_clock;

UE_FACT_FN(kRowK2DrawMaterial, "window_stroke_sync", L"Canvas", L"K2_DrawMaterial", Declared);

// A sponge dabs every 0.25 s, so these bounds are minutes of wiping, never reached in play.
constexpr size_t kMaxQueued = 256;
constexpr auto kInboundTtl = std::chrono::seconds(20);
constexpr auto kOutboundTtl = std::chrono::seconds(20);
// Dabs replayed per tick. Four peers wiping flat out produce sixteen a second between them; this
// is the frame rate times that, so a backlog drains in well under a second and no frame pays for
// more than eight canvas draws.
constexpr int kDrainPerTick = 8;

bool Verbose() {
    static const bool s = ::coop::config::ResolveFlag(::coop::config_registry::rows::window_stroke_log);
    return s;
}

std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<bool> g_armed{false};  // connected: the observer ignores every draw otherwise
int  g_wireApplyDepth = 0;         // GT: our own replay's draw is not a local stroke
bool g_hookInstalled = false;
bool g_hookArmed = false;
ue_wrap::facts::Resolved g_drawHook;  // the function the post-hook is installed on, armed and disarmed there
int  g_resolveRetry = 0;   // frames left before the next resolve attempt (GT only)
ue_wrap::CachedObjRef g_hookIdentity;
uint64_t g_epoch = 0, g_sequence = 0;
uint32_t g_world = 0, g_serial = 0;
coop::net::Role g_role = coop::net::Role::Host;
bool g_running = false, g_connected = false;
bool g_ticking = false, g_observing = false;

struct Source {
    coop::net::Session* session = g_session.load();
    uint64_t epoch = g_epoch;
    bool Current() const {
        return session && session == g_session.load() && epoch == g_epoch &&
            g_world == ue_wrap::world_identity::Generation() &&
            g_serial == coop::net::session_serial::Current() &&
            session->running() && session->connected() && session->role() == g_role;
    }
};

bool Valid(const WC::Dab& d) {
    return std::isfinite(d.x) && std::isfinite(d.y) && std::isfinite(d.size) &&
        std::isfinite(d.opac) && std::isfinite(d.col) && d.size > 0 && d.size <= 512 &&
        d.x >= -512 && d.x <= 2157 && d.y >= -512 && d.y <= 1024 &&
        d.opac >= 0 && d.opac <= 4 && d.col >= 0 && d.col <= 1;
}

// Outbound: stroke corners taken inside the Func hook, sent from Tick (both game thread).
struct Outbound {
    WC::Dab dab;
    Clock::time_point deadline;
    uint64_t sequence = 0;
    bool ready = false;
};
std::deque<Outbound> g_outbound;

// Inbound: dabs waiting for the window to be present and the canvas to be free.
struct Pending {
    WC::Dab dab;
    Clock::time_point deadline;
    uint64_t sequence=0;
};
std::deque<Pending> g_inbound;

uint64_t g_sent = 0, g_applied = 0, g_unsent = 0, g_expired = 0;

// ---- the window singleton, found by the shared scan ----
ue_wrap::CachedObjRef g_window;
uint32_t g_windowGen = 0;
ue_wrap::CachedObjRef g_scanFound;
uint32_t g_scanWorld = 0;

void HubPassBegin(void*, bool) { g_scanFound.Reset(); g_scanWorld = ue_wrap::world_identity::Generation(); }

void HubMatch(void*, void* obj) {
    if (!obj || !R::IsLive(obj)) return;
    ue_wrap::CachedObjRef identity; identity.Set(obj);
    if (R::NameStartsWith(R::NameOf(obj), L"Default__") || !identity.Get() ||
        g_scanWorld != ue_wrap::world_identity::Generation()) return;
    g_scanFound = identity;
}

size_t HubPassComplete(void*, bool isFull, uint32_t worldGen) {
    if (worldGen != ue_wrap::world_identity::Generation() || g_scanWorld != worldGen) {
        g_scanFound.Reset(); return 0;
    }
    const void* before = g_window.Raw();
    if (g_scanFound.Get()) {
        g_window = g_scanFound;
    } else if (isFull || !g_window.Get()) {
        g_window.Reset();
    }
    g_windowGen = worldGen;
    g_scanFound.Reset();
    if (g_window.Raw() != before) UE_LOGI("window_stroke: window %p (world gen %u)", g_window.Raw(), worldGen);
    return g_window.Get() ? 1 : 0;
}

void RegisterWithScanHub() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::element::scan_hub::Register(coop::element::scan_hub::Consumer{
        "window_stroke", nullptr, &WC::EnsureResolved, &WC::IsWindow,
        &HubPassBegin, &HubMatch, &HubPassComplete, /*settleScans*/ 15});
}

void* ResolveWindow() {
    if (g_windowGen != ue_wrap::world_identity::Generation()) return nullptr;
    return g_window.Get();
}

// Retire old work before releasing engine resources: a teardown callback may
// install a new session, whose queues must survive the old completion.
void Reset(coop::net::Session* next) {
    ++g_epoch;
    g_session.store(next, std::memory_order_release);
    g_world = ue_wrap::world_identity::Generation();
    g_serial = coop::net::session_serial::Current();
    g_role = next ? next->role() : coop::net::Role::Host;
    g_running = next && next->running();
    g_connected = next && next->connected();
    g_armed.store(false, std::memory_order_relaxed);
    g_outbound.clear(); g_inbound.clear();
    // A same-world reconnect must not wait for the scan hub's full backstop:
    // queued dabs can expire before that pass. Retain only the serial-validated
    // physical window; session work and unfinished scan scratch still retire.
    if (g_windowGen != g_world || !g_window.Get()) {g_window.Reset(); g_windowGen=0;}
    g_scanFound.Reset(); g_scanWorld=0;
    g_resolveRetry = 0;
    g_sent = g_applied = g_unsent = g_expired = 0;
    WC::ReleaseBrush();
}

void Reconcile() {
    auto* s = g_session.load();
    if (g_world != ue_wrap::world_identity::Generation() || g_serial != coop::net::session_serial::Current() ||
        g_running != bool(s && s->running()) || g_connected != bool(s && s->connected()) ||
        (s && g_role != s->role())) Reset(s);
}

// Func post-hook on UCanvas::K2_DrawMaterial: game thread, deep inside the engine call, so field
// reads and a queue push only.
void OnDrawPost(void* /*canvas*/, void* sourceObject, void* /*result*/) {
    const Source source;
    if (g_wireApplyDepth > 0 || g_observing || !g_armed.load(std::memory_order_relaxed) ||
        !source.Current() || g_outbound.size() >= kMaxQueued || !sourceObject || !R::IsLive(sourceObject)) return;
    struct Guard { Guard(){g_observing=true;} ~Guard(){g_observing=false;} } guard;
    ue_wrap::CachedObjRef actor; actor.Set(sourceObject);
    const auto current = [&]{return source.Current() && actor.Get()==sourceObject;};
    if (!WC::IsStrokeDraw(sourceObject) || !current()) return;
    float x = 0.f, y = 0.f;
    if (!WC::ReadStrokeCorner(sourceObject, x, y) || !current() || g_outbound.size() >= kMaxQueued ||
        !std::isfinite(x) || !std::isfinite(y) || x < -512 || x > 2157 || y < -512 || y > 1024) return;
    g_outbound.push_back({WC::Dab{x,y}, Clock::now()+kOutboundTtl, ++g_sequence});
}

void SendOutbound(coop::net::Session* s) {
    const Source source;
    if (!source.Current() || source.session != s || g_outbound.empty()) return;
    // Seal only the rows already observed before these native reads. Accepted
    // payloads retain their original brush across backpressure and hand changes.
    const uint64_t last = g_outbound.back().sequence;
    float size = 0.f, opac = 0.f, col = 0.f;
    bool needBrush = false;
    for (const auto& row : g_outbound) if (!row.ready && row.sequence <= last) needBrush = true;
    bool brush = true;
    if (needBrush) {
        brush = WC::ReadHeldBrush(coop::players::Registry::Get().Local(), size, opac, col,
                                  [&]{return source.Current();});
        if (!source.Current()) return;
        brush = brush && Valid(WC::Dab{0,0,size,opac,col});
    }
    for (auto it=g_outbound.begin(); it!=g_outbound.end();) {
        if (it->sequence>last) break;
        if (!it->ready) {
            if (!brush) {it=g_outbound.erase(it); ++g_unsent; continue;}
            it->dab.size=size; it->dab.opac=opac; it->dab.col=col; it->ready=true;
        }
        ++it;
    }
    for (int work=0; work<kDrainPerTick && !g_outbound.empty(); ++work) {
        const auto row=g_outbound.front();
        if (row.sequence>last || !row.ready || !source.Current()) return;
        if (Clock::now()>=row.deadline) {g_outbound.pop_front(); ++g_expired; --work; continue;}
        coop::net::WindowStrokePayload p{};
        p.x=row.dab.x; p.y=row.dab.y; p.size=row.dab.size; p.opac=row.dab.opac; p.col=row.dab.col;
        const bool sent=s->SendReliable(coop::net::ReliableKind::WindowStroke, &p, sizeof(p));
        if (!source.Current()) return;
        if (g_outbound.empty() || g_outbound.front().sequence!=row.sequence) return;
        if (!sent) return;  // preserve FIFO: the next dab cannot overtake a refused one
        g_outbound.pop_front();
        ++g_sent;
        if (Verbose() || g_sent <= 3 || g_sent % 200 == 0)
            UE_LOGI("window_stroke: sent #%llu at (%.1f,%.1f) size=%.0f opac=%.2f col=%.2f",
                    static_cast<unsigned long long>(g_sent), p.x, p.y, p.size, p.opac, p.col);
    }
}

void ApplyInbound() {
    const Source source;
    if (!source.Current() || g_inbound.empty()) return;
    const auto now = Clock::now();
    const auto identity=g_window;
    void* win = ResolveWindow();
    const bool signal = win && WC::IsSignalPlaying();
    if (!source.Current()) return;
    if (!win || signal) {
        while (!g_inbound.empty() && now >= g_inbound.front().deadline) {
            g_inbound.pop_front();
            ++g_expired;
        }
        return;
    }
    // A budget, because the queue really does fill: while the window is unresolved or a signal is
    // playing, nothing leaves it but an expired dab, and a signal playing is ordinary play. A dab
    // costs a canvas open, two scalar writes and a draw -- six engine dispatches -- so draining a
    // full queue in one frame would spend some fifteen hundred of them in that frame. The rest keep
    // their place and their deadline and go out on the next tick, which at this budget is four
    // times the rate three peers can fill it at.
    for (int drawn = 0; drawn < kDrainPerTick && !g_inbound.empty(); ++drawn) {
        if (!source.Current() || identity.Get()!=win || ResolveWindow()!=win) return;
        if (drawn && WC::IsSignalPlaying()) return;
        if (!source.Current() || identity.Get()!=win || ResolveWindow()!=win) return;
        // The deadline holds on this path as well as on the parked one: a dab that waited out the
        // window is a wipe from a minute ago, and drawing it now paints over what has happened
        // since. Expiring here costs the draw, not the tick's budget.
        if (now >= g_inbound.front().deadline) {
            g_inbound.pop_front();
            ++g_expired;
            --drawn;
            continue;
        }
        const auto row=g_inbound.front();
        const WC::Dab dab=row.dab;
        const auto current=[&]{return source.Current() && identity.Get()==win && ResolveWindow()==win &&
            !g_inbound.empty() && g_inbound.front().sequence==row.sequence;};
        struct Replay { Replay(){++g_wireApplyDepth;} ~Replay(){--g_wireApplyDepth;} } replay;
        const auto result=WC::DrawDab(win,dab,current);
        if (!current()) return;
        if(result==WC::DrawResult::Retry)return;
        g_inbound.pop_front();
        if (result!=WC::DrawResult::Applied) {
            // An admitted native draw may have painted despite an uncertain
            // result. Consume it once; unresolved preparation keeps its row.
            UE_LOGW("window_stroke: replay failed at (%.1f,%.1f) -- holding the rest for next tick",
                    dab.x, dab.y);
            return;
        }
        ++g_applied;
        if (Verbose() || g_applied <= 3 || g_applied % 200 == 0)
            UE_LOGI("window_stroke: applied #%llu at (%.1f,%.1f) size=%.0f opac=%.2f",
                    static_cast<unsigned long long>(g_applied), dab.x, dab.y, dab.size, dab.opac);
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    if (g_session.load()!=session) Reset(session);
    Reconcile();
    RegisterWithScanHub();
}

void OnReliable(const coop::net::WindowStrokePayload& payload, uint8_t senderPeerSlot) {
    const auto incoming=payload;
    Reconcile();
    const Source source;
    if (!source.Current()) return;
    // Clients receive the host-stamped original slot for relayed strokes, not
    // necessarily zero. Only the host checks that the originating seat is ready.
    if (senderPeerSlot>=coop::net::kMaxPeers || (g_role==coop::net::Role::Host &&
        (senderPeerSlot==0 || !source.session->IsSlotWorldReady(senderPeerSlot)))) return;
    WC::Dab dab;
    dab.x = incoming.x;
    dab.y = incoming.y;
    dab.size = incoming.size;
    dab.opac = incoming.opac;
    dab.col = incoming.col;
    if (!Valid(dab) || g_inbound.size() >= kMaxQueued) return;
    g_inbound.push_back(Pending{dab, Clock::now() + kInboundTtl, ++g_sequence});
    if (Verbose())
        UE_LOGI("window_stroke: received (%.1f,%.1f) from slot %u", dab.x, dab.y,
                static_cast<unsigned>(senderPeerSlot));
}

void Tick() {
    if (g_ticking) return;
    struct Guard { Guard(){g_ticking=true;} ~Guard(){g_ticking=false;} } guard;
    Reconcile();
    const Source source;
    auto* s = g_session.load(std::memory_order_acquire);
    const bool connected = source.Current();
    g_armed.store(connected, std::memory_order_relaxed);
    // The resolve walks three classes and two functions, and a miss is not memoised, so a world
    // without the bay window would pay those walks every frame. Sixty frames between tries, the
    // shape the neighbouring installs use; the first success latches inside EnsureResolved.
    if (!connected) {
        if (g_hookInstalled && !g_hookIdentity.Get()) {g_hookInstalled=false; g_hookArmed=false;}
        if (g_hookArmed && ue_wrap::ufunction_hook::SetArmed(g_drawHook.Fn(), &OnDrawPost, false)) g_hookArmed=false;
        return;
    }
    if (g_resolveRetry > 0) { --g_resolveRetry; return; }
    if (!WC::EnsureResolved()) {
        if (source.Current()) g_resolveRetry = 60;
        return;
    }
    if (!source.Current()) return;
    RegisterWithScanHub();
    if (g_hookInstalled && !g_hookIdentity.Get()) {g_hookInstalled=false; g_hookArmed=false;}
    if (!g_hookInstalled) {
        // InstallPostHook is idempotent, so a failure is retried rather than latched: it fails when
        // the Func slot reads null or the table is full, and the first of those can be a resolve
        // that has not settled. Without the retry one bad tick left the lane dead for the process.
        g_drawHook = ue_wrap::facts::Resolve(kRowK2DrawMaterial);
        if (!source.Current()) return;
        g_hookIdentity.Set(g_drawHook.Fn());
        const bool ok = ue_wrap::ufunction_hook::InstallPostHook(g_drawHook, &OnDrawPost, /*armed=*/connected);
        if (!source.Current()) return;
        if (ok) {
            g_hookInstalled = true;
            g_hookArmed = connected;
            UE_LOGI("window_stroke: K2_DrawMaterial post-hook installed");
        } else {
            g_resolveRetry = 60;
            static bool s_saidFailed = false;
            if (!s_saidFailed) {
                s_saidFailed = true;
                UE_LOGW("window_stroke: K2_DrawMaterial post-hook FAILED -- retrying; no dab of this "
                        "peer's own will be seen until it installs");
            }
            return;
        }
    }
    // The hook sits on a native the game draws through with no session at all, so it is armed only
    // while there is one: disarmed, the forwarder pays one load and a branch and never calls back.
    if (g_hookInstalled && g_hookArmed != connected) {
        const bool armed=ue_wrap::ufunction_hook::SetArmed(g_drawHook.Fn(), &OnDrawPost, connected);
        if (!source.Current()) return;
        if (!armed) {g_hookInstalled=false; g_hookArmed=false; return;}
        g_hookArmed = connected;
    }
    SendOutbound(s);
    if (source.Current()) ApplyInbound();
}

void OnDisconnect() {
    if (g_sent || g_applied || g_unsent || g_expired)
        UE_LOGI("window_stroke: session end -- sent=%llu applied=%llu unsent=%llu expired=%llu",
                static_cast<unsigned long long>(g_sent), static_cast<unsigned long long>(g_applied),
                static_cast<unsigned long long>(g_unsent), static_cast<unsigned long long>(g_expired));
    Reset(nullptr);
}

}  // namespace coop::window_stroke_sync
