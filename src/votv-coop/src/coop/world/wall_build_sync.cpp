#include "coop/world/wall_build_sync.h"
#include "coop/world/placed_save_sync.h"
#include "coop/world/wall_creative_sync.h"
#include "coop/net/session.h"
#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/script_gate.h"
#include <chrono>
#include <cstring>
#include <deque>

namespace coop::wall_build_sync {
namespace {
namespace SG = ue_wrap::script_gate;
namespace R = ue_wrap::reflection;
net::Session* g_session = nullptr;
bool g_watched = false;
void* g_feeding = nullptr;
bool g_allowFeed = false;
bool g_draining = false;
ue_wrap::CachedObjRef g_feedMaterial;
net::Session* g_feedSession = nullptr;
uint64_t g_feedEpoch = 0;
uint32_t g_feedWorld = 0;
uint32_t g_world = 0;
uint64_t g_epoch = 0;
struct Pending { ue_wrap::CachedObjRef wall, material; uint64_t until = 0; };
std::deque<Pending> g_pending;
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool Active() { return g_session && g_session->running(); }
bool Host() { return g_session && g_session->role() == net::Role::Host; }
void World() {
    const auto w = ue_wrap::world_identity::Generation();
    if (w != g_world) { g_world = w; g_pending.clear(); ++g_epoch; }
}
bool FeedCurrent() {
    return g_feedSession == g_session && g_feedEpoch == g_epoch &&
        g_feedWorld == ue_wrap::world_identity::Generation() && Active() && Host();
}
bool PrepareHostWall(void* actor) {
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    auto* session = g_session;
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    placed_save_sync::TrackWall(actor);
    // Enrollment calls native getData/getKey, which may destroy the actor or
    // tear down the session. Never resume a mutation through stale context.
    return epoch == g_epoch && world == ue_wrap::world_identity::Generation() &&
        session == g_session && Active() && Host() && ref.Get();
}
SG::Verdict BeforeOverlap(const SG::Call& c) {
    World();
    // An overlap check can stop the session before the deferred native body is
    // invoked. Such an invocation must not fall through as offline gameplay.
    if (g_feeding && !FeedCurrent()) return SG::Verdict::Cancel;
    if (!Active()) return SG::Verdict::Run;
    if (!Host()) return SG::Verdict::Cancel;
    if (!PrepareHostWall(c.object)) return SG::Verdict::Cancel;
    // The placement pump is idle until a peer connects. A solo host must keep
    // consuming materials natively rather than queuing work nobody drains.
    if (!g_session->connected()) return SG::Verdict::Run;
    // FinishSpawn/loadData can generate this event before size/material is final.
    // Defer the host's native overlap; recheck collision after construction ends.
    static R::ParamOffset other(L"OtherActor");
    const int32_t off = other.Of(c.function), size = R::FunctionFrameSize(c.function);
    if (!c.locals || off < 0 || off > size || size - off < int(sizeof(void*))) return SG::Verdict::Cancel;
    void* material = nullptr; std::memcpy(&material, c.locals + off, sizeof(material));
    if (!material || !R::IsLive(material)) return SG::Verdict::Cancel;
    if (g_allowFeed && g_feeding == c.object && g_feedMaterial.Is(material) && c.fromOurCode && FeedCurrent()) {
        g_allowFeed = false;
        return SG::Verdict::Run;
    }
    // The active pair and unprocessed queue remain visible while native code
    // runs, so recursive overlap notifications cannot enqueue a second spend.
    if (g_feeding == c.object && g_feedMaterial.Is(material)) return SG::Verdict::Cancel;
    for (const auto& p : g_pending)
        if (p.wall.Is(c.object) && p.material.Is(material)) return SG::Verdict::Cancel;
    if (g_pending.size() < 256) {
        Pending p; p.wall.Set(c.object); p.material.Set(material); p.until = Now() + 3000;
        g_pending.push_back(p);
    }
    return SG::Verdict::Cancel;
}
SG::Verdict BeforeMutation(const SG::Call& c) {
    // Mirrors cannot consume material, construct a second successor or refund
    // native break debris. loadData/init/visual updates remain enabled.
    World();
    if(!wall_creative_sync::NativeCurrent(c.object))return SG::Verdict::Cancel;
    if (g_feeding && !FeedCurrent()) return SG::Verdict::Cancel;
    if (Active() && Host() && !PrepareHostWall(c.object)) return SG::Verdict::Cancel;
    if(!wall_creative_sync::NativeCurrent(c.object))return SG::Verdict::Cancel;
    return Active() && !Host() ? wall_creative_sync::BeforeBuild(c) : SG::Verdict::Run;
}
SG::Verdict BeforeMirrorOnly(const SG::Call&) {
    // Destruction notifications can arrive after the actor is kill-flagged.
    // Host debris must still run; enrollment belongs before damage/build.
    return Active() && !Host() ? SG::Verdict::Cancel : SG::Verdict::Run;
}
SG::Verdict BeforeRepair(const SG::Call& c) {
    World();
    const bool staleFeed = g_feeding && !FeedCurrent();
    if (!staleFeed && !Active()) return SG::Verdict::Run;
    if (!staleFeed && Host() && PrepareHostWall(c.object)) return SG::Verdict::Run;
    // Native trowel use spends cement when fix returns isFine=false. Until its
    // host transaction is implemented, refusal must report no repair needed;
    // cancelling the body alone leaves a false out value and wastes cement.
    int32_t off = -1; uint8_t mask = 0;
    const int size = c.function ? R::FunctionFrameSize(c.function) : 0;
    const int param = c.function ? R::FindParamOffset(c.function, L"isFine") : -1;
    if (c.locals && param >= 0 && R::FindBoolProperty(c.function, L"isFine", off, mask) &&
        mask && off >= param && off < size) {
        c.locals[off] |= mask;
        if (auto* out = SG::OutParamPtr(c, param)) out[off - param] |= mask;
    }
    return SG::Verdict::Cancel;
}
void Changed(const SG::Call& c) {
    if (Active() && Host() && (!g_feeding || FeedCurrent())) placed_save_sync::MarkChanged(c.object);
}
}
void Install(net::Session* session) {
    wall_creative_sync::Install(session);
    if (g_session != session) { g_session = session; g_pending.clear(); ++g_epoch; }
    if (g_watched) return;
    constexpr int tag = 0x57414C4C;
    g_watched = SG::WatchClassName(L"customWall_unfinished_C", ue_wrap::wall_build::kOverlap, tag, &BeforeOverlap, &Changed) &&
        SG::WatchClassName(L"customWall_unfinished_C", L"buildWall", tag, &BeforeMutation, nullptr) &&
        SG::WatchClassName(L"customWall_C", L"damage", tag, &BeforeMutation, &Changed) &&
        SG::WatchClassName(L"customWall_C", L"fix", tag, &BeforeRepair, &Changed) &&
        SG::WatchClassName(L"customWall_wetConcrete_C", L"ReceiveTick", tag, &BeforeMirrorOnly, nullptr) &&
        SG::WatchClassName(L"customWall_C", L"ReceiveDestroyed", tag, &BeforeMirrorOnly, nullptr);
}
void Tick() {
    wall_creative_sync::Tick();
    World();
    if (!Active() || !Host() || g_draining || g_pending.empty()) return;
    struct Draining {Draining(){g_draining=true;}~Draining(){g_draining=false;}} draining;
    const auto epoch = g_epoch, now = Now();
    const size_t count = g_pending.size();
    int work = 0;
    for (size_t visited=0;visited<count && work<8 && !g_pending.empty();++visited) {
        if (epoch != g_epoch || !Active() || !Host()) return;
        const auto p = g_pending.front();g_pending.pop_front();
        void* wall = p.wall.Get(); void* material = p.material.Get();
        if (!wall || !material || now > p.until) continue;
        ++work;
        struct Feeding {
            Feeding(void* w,const ue_wrap::CachedObjRef& m){
                g_feeding=w;g_feedMaterial=m;g_allowFeed=true;g_feedSession=g_session;
                g_feedEpoch=g_epoch;g_feedWorld=ue_wrap::world_identity::Generation();
            }
            ~Feeding(){g_feeding=nullptr;g_feedMaterial.Reset();g_feedSession=nullptr;g_allowFeed=false;}
        } feeding(wall,p.material);
        const bool handled = ue_wrap::wall_build::Feed(wall, material);
        World();
        if (epoch != g_epoch || !Active() || !Host()) return;
        if (!handled && p.wall.Get() && p.material.Get() && g_pending.size() < 256) g_pending.push_back(p);
    }
}
void OnDisconnect() { g_session = nullptr; g_pending.clear(); ++g_epoch;wall_creative_sync::OnDisconnect(); }
}
