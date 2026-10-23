#include "coop/world/wall_build_sync.h"
#include "coop/world/placed_save_sync.h"
#include "coop/world/wall_creative_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/script_gate.h"
#include <chrono>
#include <cstring>
#include <deque>
#include <algorithm>
#include <vector>

namespace coop::wall_build_sync {
namespace {
namespace SG = ue_wrap::script_gate;
namespace R = ue_wrap::reflection;
net::Session* g_session = nullptr;
bool g_watched = false;
bool g_running=false;
void* g_feeding = nullptr;
bool g_allowFeed = false;
bool g_draining = false;
bool g_registered[6]{};
bool g_feedEntered=false,g_feedMarked=false;
uint64_t g_feedInvocation=0;
ue_wrap::CachedObjRef g_feedMaterial;
net::Session* g_feedSession = nullptr;
uint64_t g_feedEpoch = 0;
uint32_t g_feedWorld = 0;
uint32_t g_feedSerial=0,g_serial=0;
uint32_t g_world = 0;
net::Role g_role=net::Role::Host;
uint64_t g_epoch = 0;
struct Pending { ue_wrap::CachedObjRef wall, material,wallType,materialType; uint64_t until = 0,next=0; };
std::deque<Pending> g_pending;
const Pending* g_feedPair=nullptr;
struct Body {net::Session* session=nullptr;uint64_t epoch=0,invocation=0;ue_wrap::CachedObjRef actor,type;};
std::vector<Body> g_bodies;
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool Active() { return g_session && g_session->running(); }
bool Host() { return g_session && g_session->role() == net::Role::Host; }
void World() {
    const auto w = ue_wrap::world_identity::Generation();
    const auto serial=net::session_serial::Current();
    const bool running=g_session && g_session->running();
    if (w != g_world || serial!=g_serial || running!=g_running || (g_session && g_session->role()!=g_role)) {
        g_running=running;
        g_world=w;g_serial=serial;if(g_session)g_role=g_session->role();g_pending.clear();++g_epoch;
    }
    std::erase_if(g_bodies,[](const Body& body){return !SG::IsInvocationActive(body.invocation);});
}
bool Current(net::Session* session,uint64_t epoch) {
    return session && session==g_session && epoch==g_epoch && Active() && session->role()==g_role &&
        g_world==ue_wrap::world_identity::Generation() && g_serial==net::session_serial::Current();
}
bool PairCurrent(const Pending& p) {
    return p.wall.Get() && p.material.Get() && p.wallType.Get() && p.materialType.Get() &&
        R::ClassOf(p.wall.Raw())==p.wallType.Raw() && R::ClassOf(p.material.Raw())==p.materialType.Raw();
}
SG::Verdict TrackBody(const SG::Call& c) {
    if(!c.invocation)return SG::Verdict::Run; // native gate calls always carry an invocation
    if(g_bodies.size()>=64)return SG::Verdict::Cancel;
    Body body;body.session=g_session;body.epoch=g_epoch;body.invocation=c.invocation;
    body.actor.Set(c.object);body.type.Set(R::ClassOf(c.object));g_bodies.push_back(body);
    return SG::Verdict::Run;
}
bool FeedCurrent() {
    return Current(g_feedSession,g_feedEpoch) && g_feedSerial==net::session_serial::Current() &&
        g_feedWorld==ue_wrap::world_identity::Generation() && Host();
}
bool PrepareHostWall(void* actor) {
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    auto* session = g_session;
    if(!Current(session,epoch) || !Host())return false;
    ue_wrap::CachedObjRef ref,type; ref.Set(actor);type.Set(R::ClassOf(actor));
    placed_save_sync::TrackWall(actor);
    // Enrollment calls native getData/getKey, which may destroy the actor or
    // tear down the session. Never resume a mutation through stale context.
    return epoch == g_epoch && world == ue_wrap::world_identity::Generation() &&
        Current(session,epoch) && Host() && ref.Get() && type.Get() && R::ClassOf(actor)==type.Raw();
}
SG::Verdict BeforeOverlap(const SG::Call& c) {
    World();
    // An overlap check can stop the session before the deferred native body is
    // invoked. Such an invocation must not fall through as offline gameplay.
    const bool staleFeed=g_feeding && !FeedCurrent();
    if (staleFeed && !Active()) return SG::Verdict::Cancel;
    if (!Active()) return SG::Verdict::Run;
    if (!Host()) return SG::Verdict::Cancel;
    // FinishSpawn/loadData can generate this event before size/material is final.
    // Defer the host's native overlap; recheck collision after construction ends.
    static R::ParamOffset other(L"OtherActor");
    const int32_t off = other.Of(c.function), size = R::FunctionFrameSize(c.function);
    if (!c.locals || off < 0 || off > size || size - off < int(sizeof(void*))) return SG::Verdict::Cancel;
    void* material = nullptr; std::memcpy(&material, c.locals + off, sizeof(material));
    if (!material || !R::IsLive(material)) return SG::Verdict::Cancel;
    if(staleFeed && g_feeding==c.object && g_feedMaterial.Raw()==material)return SG::Verdict::Cancel;
    const auto epoch=g_epoch;auto* session=g_session;
    Pending selected;selected.wall.Set(c.object);selected.material.Set(material);
    selected.wallType.Set(R::ClassOf(c.object));selected.materialType.Set(R::ClassOf(material));
    if (g_allowFeed && g_feeding == c.object && g_feedMaterial.Is(material) && c.fromOurCode && FeedCurrent()) {
        if(!g_feedPair || !PairCurrent(*g_feedPair))return SG::Verdict::Cancel;
        g_allowFeed = false;
        const auto verdict=TrackBody(c);
        if(verdict==SG::Verdict::Run){g_feedEntered=true;g_feedInvocation=c.invocation;}
        return verdict;
    }
    // The active pair and unprocessed queue remain visible while native code
    // runs, so recursive overlap notifications cannot enqueue a second spend.
    if (g_feeding == c.object && g_feedMaterial.Raw()==material) return SG::Verdict::Cancel;
    if(!PrepareHostWall(c.object) || !Current(session,epoch) || !PairCurrent(selected))return SG::Verdict::Cancel;
    // Solo hosts retain native ownership, including last-peer departure.
    if(!g_session->connected())return TrackBody(c);
    std::erase_if(g_pending,[](const Pending& p){return !PairCurrent(p);});
    for (const auto& p : g_pending)
        if (p.wall.Is(c.object) && p.material.Is(material)) return SG::Verdict::Cancel;
    if (g_pending.size() < 256) {
        selected.until=Now()+3000;g_pending.push_back(selected);
    }
    return SG::Verdict::Cancel;
}
SG::Verdict BeforeMutation(const SG::Call& c) {
    // Mirrors cannot consume material, construct a second successor or refund
    // native break debris. loadData/init/visual updates remain enabled.
    World();
    auto* session=g_session;const auto epoch=g_epoch;
    if(!wall_creative_sync::NativeCurrent(c.object))return SG::Verdict::Cancel;
    if (g_feeding && !FeedCurrent()) return SG::Verdict::Cancel;
    if (Active() && Host() && !PrepareHostWall(c.object)) return SG::Verdict::Cancel;
    if(session && !Current(session,epoch))return SG::Verdict::Cancel;
    if(!wall_creative_sync::NativeCurrent(c.object))return SG::Verdict::Cancel;
    if(Active() && !Host())return wall_creative_sync::BeforeBuild(c);
    return Active()?TrackBody(c):SG::Verdict::Run;
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
    if (!staleFeed && Host() && PrepareHostWall(c.object) && TrackBody(c)==SG::Verdict::Run) return SG::Verdict::Run;
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
    const auto found=std::find_if(g_bodies.begin(),g_bodies.end(),[&](const Body& body){return body.invocation==c.invocation && body.actor.Raw()==c.object;});
    if(found==g_bodies.end())return;
    const auto body=*found;g_bodies.erase(found);
    if(Current(body.session,body.epoch) && Host() && body.actor.Get() && body.type.Get() && R::ClassOf(c.object)==body.type.Raw()) {
        if(g_feeding==c.object && g_feedInvocation==c.invocation)g_feedMarked=true;
        placed_save_sync::MarkChanged(c.object);
    }
}
}
void Install(net::Session* session) {
    if (g_session != session) { g_session = session; g_pending.clear(); ++g_epoch; }
    World();
    const auto epoch=g_epoch;
    wall_creative_sync::Install(session);
    if(g_session!=session || g_epoch!=epoch)return;
    if (g_watched) return;
    constexpr int tag = 0x57414C4C;
    struct Watch {const wchar_t* cls;const wchar_t* fn;SG::PreFn pre;SG::PostFn post;};
    const Watch watches[]{
        {L"customWall_unfinished_C",ue_wrap::wall_build::kOverlap,BeforeOverlap,Changed},
        {L"customWall_unfinished_C",L"buildWall",BeforeMutation,nullptr},
        {L"customWall_C",L"damage",BeforeMutation,Changed},{L"customWall_C",L"fix",BeforeRepair,Changed},
        {L"customWall_wetConcrete_C",L"ReceiveTick",BeforeMirrorOnly,nullptr},{L"customWall_C",L"ReceiveDestroyed",BeforeMirrorOnly,nullptr}};
    for(size_t i=0;i<6;++i)if(!g_registered[i])g_registered[i]=SG::WatchClassName(watches[i].cls,watches[i].fn,tag,watches[i].pre,watches[i].post);
    g_watched=std::all_of(std::begin(g_registered),std::end(g_registered),[](bool ready){return ready;});
}
void Tick() {
    wall_creative_sync::Tick();
    World();
    if (!Active() || !Host() || g_draining || g_pending.empty()) return;
    struct Draining {Draining(){g_draining=true;}~Draining(){g_draining=false;}} draining;
    const auto epoch = g_epoch, now = Now();
    auto* session=g_session;
    const size_t count = g_pending.size();
    int work = 0;
    for (size_t visited=0;visited<count && work<8 && !g_pending.empty();++visited) {
        if (!Current(session,epoch) || !Host()) return;
        const auto p = g_pending.front();g_pending.pop_front();
        void* wall = p.wall.Get(); void* material = p.material.Get();
        if (!wall || !material || !PairCurrent(p) || now > p.until) continue;
        if(now<p.next){g_pending.push_back(p);continue;}
        ++work;
        struct Feeding {
            Feeding(void* w,const Pending& p){
                g_feeding=w;g_feedMaterial=p.material;g_feedPair=&p;g_allowFeed=false;g_feedSession=g_session;
                g_feedEpoch=g_epoch;g_feedWorld=ue_wrap::world_identity::Generation();g_feedSerial=g_serial;
                g_feedEntered=g_feedMarked=false;g_feedInvocation=0;
            }
            ~Feeding(){g_feeding=nullptr;g_feedMaterial.Reset();g_feedSession=nullptr;g_feedPair=nullptr;g_allowFeed=false;}
        } feeding(wall,p);
        if(!PrepareHostWall(wall) || !PairCurrent(p)) {
            if(!Current(session,epoch))return;
            continue;
        }
        g_allowFeed=true;
        const bool handled = ue_wrap::wall_build::FeedWhile(wall,material,[&]{return FeedCurrent() && PairCurrent(p);});
        const bool entered=g_feedEntered;
        if(entered && !g_feedMarked && FeedCurrent() && p.wall.Get() && p.wallType.Get() && R::ClassOf(wall)==p.wallType.Raw())
            placed_save_sync::MarkChanged(wall);
        World();
        if (!Current(session,epoch) || !Host()) return;
        if (!handled && !entered && PairCurrent(p) && g_pending.size() < 256) {
            auto retry=p;retry.next=Now()+250;g_pending.push_back(retry);
        }
    }
}
void OnDisconnect() { g_session = nullptr; g_pending.clear(); ++g_epoch;wall_creative_sync::OnDisconnect(); }
}
