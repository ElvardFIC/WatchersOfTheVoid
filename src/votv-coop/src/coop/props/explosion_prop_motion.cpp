#include "coop/props/explosion_prop_motion.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/local_prop_hold.h"
#include "coop/props/prop_drive_host.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/radial_impulse.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"
#include <array>

namespace coop::explosion_prop_motion {
namespace {
namespace R=ue_wrap::reflection;
namespace P=ue_wrap::prop;
UE_FACT_FN(kImpulse, "explosion_prop_motion", L"RadialForceComponent", L"FireImpulse", Declared);
struct Target { ue_wrap::CachedObjRef actor,blast; bool ready=false; };
std::array<Target,1024> g_targets{};
std::array<ue_wrap::CachedObjRef,1024> g_blasts{};
size_t g_size=0;
size_t g_blastSize=0;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_serial=0;
uint64_t g_epoch=0;
bool g_ticking=false,g_fullSaid=false,g_querySaid=false;
ue_wrap::CachedObjRef g_hook;

bool Current() {
    return g_session && g_session->connected() && g_session->role()==net::Role::Host &&
        g_world==ue_wrap::world_identity::Generation() && g_serial==net::session_serial::Current();
}
bool WorldActor(void* actor) {
    return actor && R::IsLive(actor) && ue_wrap::world_identity::CurrentWorld() &&
        ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld();
}
bool Movable(void* actor) {
    return WorldActor(actor) && P::IsDescendantOfProp(actor) &&
        !P::IsStatic(actor) && !P::IsFrozen(actor) && !P::IsGarbageClump(actor) &&
        !P::IsTrashActor(actor) && !P::IsTrashBitsPile(actor) &&
        !local_prop_hold::IsHeld(actor) && !prop_drive_host::InAHand(actor);
}
void Clear() { ++g_epoch; g_targets={}; g_blasts={}; g_size=g_blastSize=0; g_fullSaid=g_querySaid=false; }
void OnImpulse(void* component,void*,void*) {
    if (!Current() || !WorldActor(component)) return;
    // This is the actual native impulse, after the Blueprint's latent delay.
    // A different radial component must never release another blast's targets.
    void* blast=R::OuterOf(component);
    if (!WorldActor(blast)) return;
    bool known=false;
    for (size_t i=0;i<g_blastSize;++i) if (g_blasts[i].Is(blast)) { known=true; break; }
    if (!known) return;
    const auto epoch=g_epoch;
    ue_wrap::CachedObjRef source,radial; source.Set(blast); radial.Set(component);
    const auto valid=[&]{return Current() && g_epoch==epoch && source.Get() && radial.Get();};
    // FireImpulse changes velocity synchronously; body positions integrate later.
    // Its own sphere/filter also finds zero-damage blasts and actors whose origin
    // is outside the sphere but whose collision geometry overlaps it.
    std::vector<ue_wrap::CachedObjRef> actors;
    const bool sampled=ue_wrap::radial_impulse::CollectActors(component,actors,valid);
    if (!valid()) return;
    if (sampled) {
        for (const auto& actor:actors) if (void* live=actor.Get()) Observe(live,blast);
    } else if (!g_querySaid) {
        UE_LOGW("explosion motion: native impulse target query unavailable; retaining damage-overlap targets");
        g_querySaid=true;
    }
    for (size_t i=0;i<g_size;++i) if (g_targets[i].blast.Is(blast)) g_targets[i].ready=true;
}
void ResolveHook() {
    if (g_hook.Get()) return;
    auto fn=ue_wrap::facts::Resolve(kImpulse);
    if (fn.Fn() && !g_hook.Is(fn.Fn()) &&
        ue_wrap::ufunction_hook::InstallPostHook(fn,&OnImpulse)) g_hook.Set(fn.Fn());
}
}
void Install(net::Session* session) {
    const auto world=ue_wrap::world_identity::Generation(),serial=net::session_serial::Current();
    if (g_session!=session || g_world!=world || g_serial!=serial) Clear();
    g_session=session; g_world=world; g_serial=serial;
    if (Current()) ResolveHook();
}
void ObserveBlast(void* blast) {
    if (!Current() || !WorldActor(blast)) return;
    ue_wrap::CachedObjRef* free=nullptr;
    for (size_t i=0;i<g_blastSize;++i) {
        if (g_blasts[i].Is(blast)) return;
        if (!free && !g_blasts[i].Get()) free=&g_blasts[i];
    }
    if (!free && g_blastSize<g_blasts.size()) free=&g_blasts[g_blastSize++];
    if (free) free->Set(blast);
}
void Observe(void* actor,void* blast) {
    if (!Current() || !g_hook.Get() || !WorldActor(blast) || !Movable(actor)) return;
    ObserveBlast(blast);
    Target* free=nullptr;
    for (size_t i=0;i<g_size;++i) {
        auto& row=g_targets[i];
        if (row.actor.Is(actor) && row.blast.Is(blast)) return;
        if (!free && (!row.actor.Get() || (!row.ready && !row.blast.Get()))) free=&row;
    }
    if (!free && g_size<g_targets.size()) free=&g_targets[g_size++];
    if (!free) {
        if (!g_fullSaid) UE_LOGW("explosion motion: native overlap target table full");
        g_fullSaid=true; return;
    }
    *free={}; free->actor.Set(actor); free->blast.Set(blast);
}
void Tick() {
    if (g_ticking || !g_session) return;
    struct Guard { Guard(){g_ticking=true;} ~Guard(){g_ticking=false;} } guard;
    Install(g_session);
    if (!Current() || !g_size) return;
    const auto epoch=g_epoch;
    // Detach ready work before Coast can call native code. Never mutate the
    // drive publisher's vector from FireImpulse, or retain a row across callbacks.
    std::array<ue_wrap::CachedObjRef,1024> pending{};
    size_t count=0;
    for (size_t i=0;i<g_size;++i) {
        auto& row=g_targets[i];
        if (row.ready) { pending[count++]=row.actor; row={}; }
        else if (!row.actor.Get() || !row.blast.Get() || !Movable(row.actor.Get())) row={};
    }
    while (g_size && !g_targets[g_size-1].actor.Raw()) --g_size;
    for (size_t i=0;i<count;++i) {
        void* actor=pending[i].Get();
        if (Movable(actor)) prop_drive_host::Coast(actor,"explosion impulse");
        if (g_epoch!=epoch || !Current()) return;
    }
}
void OnDisconnect() { Clear(); g_session=nullptr; g_world=g_serial=0; }
}
