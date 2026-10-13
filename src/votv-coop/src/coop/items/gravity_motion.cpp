#include "coop/items/gravity_motion.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_drive_stream.h"
#include "coop/props/remote_prop.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/devices/kitchen_machine.h"
#include "ue_wrap/engine/engine_physics.h"
#include "ue_wrap/engine/world_identity.h"
#include <cstring>

namespace coop::gravity_motion {
namespace {
namespace R = ue_wrap::reflection;
namespace H = ue_wrap::ufunction_hook;
namespace P = ue_wrap::prop;
net::Session* g_session = nullptr;
ue_wrap::CachedObjRef g_target, g_player;
uint64_t g_until = 0;
uint64_t g_retryHooks = 0;
bool g_unstreamed = false;
uint32_t g_world = 0;
bool g_force = false, g_impulse = false, g_query = false;
bool g_tickWatch = false;
ue_wrap::script_gate::Verdict GunTick(const ue_wrap::script_gate::Call& c) {
    // Actor tick and component tick are independent. The orphan mainPlayer's
    // gravity component calls lib.getMainPlayer (player index 0), consumes OUR
    // input_rotate/input_fire, and can hold OUR target in the orphan's obj slot.
    // It must never execute the single-player weapon graph on a remote body.
    if (g_session && g_session->connected() &&
        players::Registry::Get().IsPuppet(R::OuterOf(c.object)))
        return ue_wrap::script_gate::Verdict::Cancel;
    return ue_wrap::script_gate::Verdict::Run;
}
bool LocalSource(void* source) {
    if (!g_session || !g_session->connected() || !source || !g_player.Alive() ||
        g_world != ue_wrap::world_identity::Generation()) return false;
    static R::InstanceOffset off{L"weaponComp_gravityGun"};
    const int32_t n = off.Of(g_player.Get());
    void* comp = nullptr;
    if (n >= 0) std::memcpy(&comp, static_cast<uint8_t*>(g_player.Get())+n, sizeof(comp));
    return comp == source;
}
void* Eligible(void* component) {
    void* actor = component ? R::OuterOf(component) : nullptr;
    if (!actor || !P::IsDescendantOfProp(actor) || P::IsStatic(actor) || P::IsFrozen(actor) ||
        ue_wrap::atv::IsAtv(actor) || ue_wrap::kitchen_machine::DoorParent(actor) || remote_prop::IsActorUnderAnyDrive(actor) ||
        ue_wrap::world_identity::WorldOf(actor)!=ue_wrap::world_identity::CurrentWorld()) return nullptr;
    return actor;
}
void Force(void* component, void* source, void*) {
    if (!LocalSource(source)) return;
    void* actor = Eligible(component);
    if (!actor) return;
    if (g_target.Get()!=actor) UE_LOGI("gravity: local force target key='%ls'",P::GetInteractableKeyString(actor).c_str());
    const auto now=active_drive::NowMs();
    if (g_target.Get()!=actor || now>=g_until) g_unstreamed=true;
    g_target.Set(actor);
    // A shot can start and finish between network ticks. Keep its author long
    // enough to send a pose, then PropRelease transfers the live flight to host.
    g_until = now+250;
    if (prop_drive_stream::IsParked(actor))
        ue_wrap::engine::SetComponentSimulatePhysics(component,true);
}
ue_wrap::script_gate::Verdict Impulse(void* component,void* source,const uint8_t*) {
    // A one-shot impulse needs its body awake before the native call.
    Force(component,source,nullptr);
    return ue_wrap::script_gate::Verdict::Run;
}
void Query(void* component, void* source, void*) {
    if (!LocalSource(source)) return;
    void* actor = Eligible(component);
    // A receiver parks driven props kinematically; that networking detail must
    // not make the gun reject a perfectly movable prop in hold().
    if (actor && prop_drive_stream::IsParked(actor))
        if (void* result=H::CurrentResult()) *static_cast<bool*>(result)=true;
}
}
void Tick(net::Session& session) {
    g_session = &session;
    if (!g_tickWatch) g_tickWatch=ue_wrap::script_gate::WatchClassName(
        L"comp_gravitygun_C",L"ReceiveTick",0x47520001,&GunTick,nullptr);
    const auto world = ue_wrap::world_identity::Generation();
    void* player = players::Registry::Get().Local();
    if (world!=g_world || g_player.Get()!=player) {
        g_target.Reset(); g_until=0; g_unstreamed=false; g_world=world;
        if (player) g_player.Set(player); else g_player.Reset();
    }
    const auto now=active_drive::NowMs();
    if ((g_force && g_impulse && g_query) || now<g_retryHooks) return;
    g_retryHooks=now+2000;
    void* cls = R::FindClass(L"PrimitiveComponent");
    if (!cls) return;
    if (!g_force) if (void* fn=R::FindFunction(cls,L"AddForce"))
        g_force=H::InstallPostHook(fn,&Force);
    if (!g_impulse) if (void* fn=R::FindFunction(cls,L"AddImpulse"))
        g_impulse=H::InstallPreHook(fn,&Impulse,true);
    if (!g_query) if (void* fn=R::FindDispatchFunctionCached(cls,L"IsSimulatingPhysics"))
        g_query=H::InstallPostHook(fn,&Query);
    UE_LOGI("gravity: native seams force=%d impulse=%d physics-query=%d",g_force,g_impulse,g_query);
}
void* Target() {
    return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation() &&
        (active_drive::NowMs()<g_until || (g_unstreamed && active_drive::NowMs()<g_until+2000)) ? g_target.Get() : nullptr;
}
void NoteStreamed(void* actor) { if (actor && g_target.Get()==actor) g_unstreamed=false; }
void OnDisconnect() { g_session=nullptr; g_target.Reset(); g_player.Reset(); g_until=0; g_world=0; g_unstreamed=false; }
}
