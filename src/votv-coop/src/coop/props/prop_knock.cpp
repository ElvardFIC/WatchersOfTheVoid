#include "coop/props/prop_knock.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/local_prop_hold.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/remote_prop.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/script_gate.h"
#include <array>
#include <cstring>
namespace coop::prop_knock {
namespace {
namespace P=ue_wrap::prop;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
net::Session* g_session=nullptr;
uint32_t g_world=0;
uint64_t g_epoch=0;
std::array<ue_wrap::CachedObjRef,64> g_pending{};
size_t g_count=0;
bool Current(){return g_session && g_session->connected() && g_session->role()==net::Role::Host &&
    g_world==ue_wrap::world_identity::Generation();}
bool WorldProp(void* actor){return actor && R::IsLive(actor) && P::IsDescendantOfProp(actor) &&
    ue_wrap::world_identity::CurrentWorld() && ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld();}
bool Queued(void* actor){for(size_t i=0;i<g_count;++i)if(g_pending[i].Is(actor))return true;return false;}
bool Source(void* actor) {
    return WorldProp(actor) && !hand_item::IsHandAxisActor(actor) &&
        (local_prop_hold::IsHeld(actor) || remote_prop::IsActorUnderAnyDrive(actor) ||
         prop_drive_host::IsDriven(actor) || Queued(actor));
}
bool Movable(void* actor) {
    return WorldProp(actor) && !P::IsStatic(actor) && !P::IsFrozen(actor) &&
        !P::IsGarbageClump(actor) && !P::IsTrashActor(actor) && !P::IsTrashBitsPile(actor) &&
        !hand_item::IsHandAxisActor(actor) && !local_prop_hold::IsHeld(actor) && !remote_prop::IsActorUnderAnyDrive(actor);
}
SG::Verdict OnKnock(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !c.object || !c.function || !c.locals)return SG::Verdict::Run;
    const int off=R::FindParamOffset(c.function,L"OtherActor"),size=R::FunctionFrameSize(c.function);
    if(off<0 || off>size || sizeof(void*)>size-off)return SG::Verdict::Run;
    void* other=nullptr;std::memcpy(&other,c.locals+off,sizeof(other));
    void* owner=c.object;
    for(int i=0;i<64 && owner && R::IsLive(owner) && !P::IsDescendantOfProp(owner);++i) {
        void* parent=R::OuterOf(owner);
        if(parent==owner)return SG::Verdict::Run;
        owner=parent;
    }
    // Player bodies are not props. Their contacts still start the same bounded,
    // host-owned coasting stream as a prop pushed by a held prop.
    if(WorldProp(owner) && other && R::IsLive(other) &&
       ue_wrap::world_identity::WorldOf(other)==ue_wrap::world_identity::CurrentWorld() &&
       (other==players::Registry::Get().Local() || players::Registry::Get().IsPuppetBody(other))) {
        if(Movable(owner) && !Queued(owner) && g_count<g_pending.size())g_pending[g_count++].Set(owner);
        return SG::Verdict::Run;
    }
    if(!WorldProp(owner) || !WorldProp(other) || owner==other)return SG::Verdict::Run;
    const bool fromOwner=Source(owner),fromOther=Source(other);
    if(fromOwner==fromOther)return SG::Verdict::Run;
    void* victim=fromOwner?other:owner;
    if(Movable(victim) && !Queued(victim) && g_count<g_pending.size())g_pending[g_count++].Set(victim);
    return SG::Verdict::Run;
}
}
void Tick(net::Session& session) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session!=&session || g_world!=world){OnDisconnect();g_session=&session;g_world=world;}
    if(!Current())return;
    // The cooked component binds OnComponentHit to hitCpp; the old hit is empty.
    SG::WatchClassName(L"comp_physicsImpact_C",L"hitCpp",0x4B4E0002,&OnKnock,nullptr);
    SG::ResolvePendingNames();
    const auto pending=g_pending;const auto count=g_count,epoch=g_epoch;g_count=0;g_pending={};
    for(size_t i=0;i<count;++i) {
        void* actor=pending[i].Get();if(Movable(actor))prop_drive_host::Coast(actor,"collision push");
        if(epoch!=g_epoch || !Current())return;
    }
}
void OnDisconnect(){++g_epoch;g_session=nullptr;g_world=0;g_count=0;g_pending={};}
}
