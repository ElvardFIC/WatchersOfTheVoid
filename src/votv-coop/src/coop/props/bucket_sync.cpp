// Water/cement save fields already have a carrier; publish actual changes instead of
// sending the unchanged construction record back when the next player picks up.
#include "coop/props/bucket_sync.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/local_prop_hold.h"
#include "coop/props/active_drive.h"
#include "coop/props/join_membership_sweep.h"
#include "coop/props/prop_save_data.h"
#include "coop/props/remote_prop.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <cmath>
#include <cstring>
namespace coop::bucket_sync {
namespace {
namespace B=ue_wrap::building_material;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
constexpr int kTag=0x42554B54;
net::Session* g_session=nullptr;
uint32_t g_world=0;
uint64_t g_epoch=0;
bool g_draining=false;
ue_wrap::CachedObjRef g_cementTick,g_takeConcrete,g_bagUse,g_trowelUse;
struct State { B::Water water{}; B::Stock stock{}; };
bool ReadState(void* actor,State& state) {
    state={};
    return B::ReadWater(actor,state.water) || B::ReadStock(actor,state.stock);
}
struct Row {
    ue_wrap::CachedObjRef actor;
    State last{};
    bool have=false,dirty=false;
    uint64_t sampleAt=0,repaintAt=0,sendAt=0,revision=0;
};
std::array<Row,128> g_rows{};
bool Current() {
    if(!g_session || !g_session->connected())return false;
    const auto world=ue_wrap::world_identity::Generation();
    if(world!=g_world){++g_epoch;g_world=world;g_rows={};}
    return true;
}
bool Eligible(void* actor) {
    return actor && !hand_item::IsHandAxisActor(actor) &&
        (g_session->role()==net::Role::Host ||
        (net_pump::IsInAnnouncedWorld(actor) && join_membership_sweep::HasLoadTailQuiesced()));
}
Row* Find(void* actor) {
    Row* free=nullptr;
    for(auto& row:g_rows) {
        if(row.actor.Is(actor))return &row;
        if(!free && !row.actor.Get())free=&row;
    }
    if(free){*free={};free->actor.Set(actor);}return free;
}
bool Same(const State& a,const State& b) {
    if(a.stock.kind!=b.stock.kind)return false;
    if(a.stock.kind!=B::StockKind::None)return a.stock.units==b.stock.units && a.stock.dry==b.stock.dry &&
        std::fabs(a.stock.wetness-b.stock.wetness)<.0001f;
    return std::fabs(a.water.height-b.water.height)<.01f && a.water.soap==b.water.soap && a.water.alcohol==b.water.alcohol;
}
bool NativeOwner(void* actor) {
    if(actor==hand_item::LocalHandActor() || local_prop_hold::IsHeld(actor))return true;
    return g_session->role()==net::Role::Host && !hand_item::IsMirrorActor(actor) &&
        !remote_prop::IsActorUnderAnyDrive(actor);
}
void Observe(void* actor,bool applied,bool force=false) {
    if(!Current() || !Eligible(actor))return;
    Row* row=Find(actor);if(!row)return;
    const auto now=active_drive::NowMs();
    if(!applied && !force && now<row->sampleAt)return;
    row->sampleAt=now+250;
    State state{};if(!ReadState(actor,state))return;
    if(applied){row->last=state;row->have=true;row->dirty=false;++row->revision;return;}
    if(!row->have || !Same(row->last,state)) {
        row->last=state;row->have=true;row->dirty=true;++row->revision;
    }
}
SG::Verdict BeforeTick(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !c.object || NativeOwner(c.object))return SG::Verdict::Run;
    if(g_session->role()==net::Role::Client && !join_membership_sweep::HasLoadTailQuiesced())return SG::Verdict::Run;
    if(!c.function || !c.locals)return SG::Verdict::Run;
    const int off=R::FindParamOffset(c.function,L"DeltaSeconds"),size=R::FunctionFrameSize(c.function);
    if(off<0 || off>size || sizeof(float)>size-off)return SG::Verdict::Run;
    float seconds=0;std::memcpy(&seconds,c.locals+off,sizeof(seconds));
    ue_wrap::CachedObjRef live;live.Set(c.object);const auto epoch=g_epoch;
    // ReceiveTick starts with prop_openContainer::ReceiveTick. Suppressing it
    // wholesale would also break the bucket's ordinary contained-prop handling.
    if(!B::TickBucketContents(c.object,seconds))return SG::Verdict::Run;
    if(epoch!=g_epoch || !live.Get())return SG::Verdict::Cancel;
    // Remote kinematic copies must not spill/ferment against their delayed pose.
    // Never disable the actor tick itself: disconnect immediately restores native behavior.
    Row* row=Find(c.object);
    const auto now=active_drive::NowMs();
    if(row && now>=row->repaintAt) {row->repaintAt=now+100;B::RepaintBucket(c.object);}
    return SG::Verdict::Cancel;
}
void AfterTick(const SG::Call& c) {
    if(Current() && !c.fromOurCode && c.object && NativeOwner(c.object))Observe(c.object,false);
}
SG::Verdict BeforeUpd(const SG::Call& c) {
    if(!Current() || !c.object)return SG::Verdict::Run;
    // Native loadData sets height, not empty. Its upd would hide received water
    // and even clear alcohol before a post-load correction could recover it.
    if(c.callerFunction && R::NameEquals(R::NameOf(c.callerFunction),L"loadData"))
        B::RepairBucketEmpty(c.object);
    return SG::Verdict::Run;
}
void AfterLoad(const SG::Call& c) {
    if(!Current() || !c.object)return;
    B::RepairBucketEmpty(c.object);Observe(c.object,true);
}
void AfterUse(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !c.object)return;
    // Native getFromWater can fill a selected-hand bucket without ticking it.
    ue_wrap::CachedObjRef live;live.Set(c.object);const auto epoch=g_epoch;
    if(!B::RepairBucketEmpty(c.object))return;
    B::RepaintBucket(c.object);
    if(epoch!=g_epoch || !live.Get())return;
    Observe(c.object,false,true);
}
SG::Verdict BeforeCementTick(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !c.object)return SG::Verdict::Run;
    g_cementTick.Set(c.function);
    if(NativeOwner(c.object)) {Observe(c.object,false);return SG::Verdict::Run;}
    if(g_session->role()==net::Role::Client && !join_membership_sweep::HasLoadTailQuiesced())return SG::Verdict::Run;
    // Unlike the water bucket, concreteBucket's cooked ReceiveTick has no parent
    // tick: it ONLY spills/spawns/replaces/dries. Replicas must not do that again.
    return SG::Verdict::Cancel;
}
void AfterStockLoad(const SG::Call& c) {
    if(Current() && c.object)Observe(c.object,true);
}
void AfterStockChange(const SG::Call& c) {
    if(Current() && !c.fromOurCode && c.object)Observe(c.object,false,true);
}
template<class T> bool ReadArg(const SG::Call& c,const wchar_t* name,T& value) {
    if(!c.function || !c.locals)return false;
    const int off=R::FindParamOffset(c.function,name),size=R::FunctionFrameSize(c.function);
    if(off<0 || off>size || sizeof(T)>size-off)return false;
    std::memcpy(&value,c.locals+off,sizeof(T));return true;
}
void AfterBagTransfer(const SG::Call& c) {
    if(!Current() || c.fromOurCode)return;
    // RMB invokes transferUnit on the world bag too; the receiver alone does not
    // identify which of its TWO amounts changed. Do not lose the other endpoint.
    for(const wchar_t* name:{L"from",L"to"}) {
        void* actor=nullptr;
        if(ReadArg(c,name,actor) && actor && R::IsLive(actor))Observe(actor,false,true);
    }
}
SG::Verdict BeforeTake(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !c.object)return SG::Verdict::Run;
    g_takeConcrete.Set(c.function);
    B::Stock stock{};
    if(!B::ReadStock(c.object,stock) || stock.kind!=B::StockKind::ConcreteBucket || stock.units>0)return SG::Verdict::Run;
    // Native takeConcrete checks dry, but not units. Explicitly return failure;
    // Cancel alone leaves a reused false out value and grants five free portions.
    int32_t off=-1;uint8_t mask=0;
    const int size=c.function?R::FunctionFrameSize(c.function):0;
    const int param=c.function?R::FindParamOffset(c.function,L"dry"):-1;
    if(!c.locals || param<0 || !R::FindBoolProperty(c.function,L"dry",off,mask) ||
        !mask || off<param || off>=size)return SG::Verdict::Run;
    c.locals[off]|=mask;
    // OutParmRec is indexed by the property's base offset; a packed bool can
    // have an additional byte offset within that property on either side.
    if(uint8_t* out=SG::OutParamPtr(c,param))out[off-param]|=mask;
    return SG::Verdict::Cancel;
}
SG::Verdict BeforeBagUse(const SG::Call& c) {
    if(Current() && !c.fromOurCode)g_bagUse.Set(c.function);
    return SG::Verdict::Run;
}
SG::Verdict BeforeTrowelUse(const SG::Call& c) {
    if(Current() && !c.fromOurCode)g_trowelUse.Set(c.function);
    return SG::Verdict::Run;
}
}
void Install(net::Session* session) {
    if(g_session!=session)OnDisconnect();g_session=session;
    SG::WatchClassName(L"prop_bucket_C",L"ReceiveTick",kTag,&BeforeTick,&AfterTick);
    SG::WatchClassName(L"prop_bucket_C",L"upd",kTag,&BeforeUpd,nullptr);
    SG::WatchClassName(L"prop_bucket_C",L"loadData",kTag,nullptr,&AfterLoad);
    for(const wchar_t* name:{L"getFromWater",L"dipped",L"playerUsedOn",L"playerHandUse_LMB"})
        SG::WatchClassName(L"prop_bucket_C",name,kTag,nullptr,&AfterUse);
    SG::WatchClassName(L"prop_concreteBucket_C",L"ReceiveTick",kTag,&BeforeCementTick,&AfterTick);
    SG::WatchClassName(L"prop_concreteBucket_C",L"takeConcrete",kTag,&BeforeTake,&AfterStockChange);
    for(const wchar_t* name:{L"updStage",L"updDry"})
        SG::WatchClassName(L"prop_concreteBucket_C",name,kTag,nullptr,&AfterStockChange);
    for(const wchar_t* cls:{L"prop_concreteBucket_C",L"prop_cementBag_C",L"prop_wallfixer_C",L"prop_sponge_C"})
        SG::WatchClassName(cls,L"loadData",kTag,nullptr,&AfterStockLoad);
    SG::WatchClassName(L"prop_cementBag_C",L"transferUnit",kTag,nullptr,&AfterBagTransfer);
    SG::WatchClassName(L"prop_cementBag_C",L"playerHandUse_LMB",kTag,&BeforeBagUse,&AfterStockChange);
    SG::WatchClassName(L"prop_wallfixer_C",L"playerHandUse_LMB",kTag,&BeforeTrowelUse,&AfterStockChange);
    SG::WatchClassName(L"prop_wallfixer_C",L"updConcrete",kTag,nullptr,&AfterStockChange);
    // enteredTheWater and collided are empty in this cook. enterWater fills;
    // wash/clamp spend power; LMB also fills a selected sponge from a bucket.
    for(const wchar_t* name:{L"enterWater",L"wash",L"clamp",L"playerHandUse_LMB"})
        SG::WatchClassName(L"prop_sponge_C",name,kTag,nullptr,&AfterStockChange);
    SG::ResolvePendingNames();
}
void Tick() {
    if(!Current() || g_draining || !g_session->AnyWorldReadyPeer())return;
    struct Guard{Guard(){g_draining=true;}~Guard(){g_draining=false;}}guard;
    const auto epoch=g_epoch,now=active_drive::NowMs();auto* session=g_session;
    unsigned budget=2;
    for(auto& row:g_rows) {
        if(!row.dirty || now<row.sendAt)continue;
        void* actor=row.actor.Get();if(!actor){row={};continue;}
        if(!Eligible(actor))continue;
        // Take the final state after a throttled last mutation (dip can run twice
        // inside250ms, then stop; dropping that tail would restore spent water).
        const auto copy=row;
        const auto key=ue_wrap::prop::GetInteractableKeyString(actor);
        if(epoch!=g_epoch || !copy.actor.Get())return;
        if(key.empty() || key==L"None"){row.sendAt=now+1000;continue;}
        const bool covered=prop_save_data::Covers(actor);
        if(epoch!=g_epoch || !copy.actor.Get())return;
        if(row.revision!=copy.revision)continue;
        if(!covered){row.sendAt=now+1000;continue;}
        State state{};if(!ReadState(actor,state))continue;
        row.sendAt=now+300;
        const bool ok=prop_save_data::Publish(session,actor,key);
        if(epoch!=g_epoch)return;
        if(row.actor.Is(actor) && row.revision==copy.revision && ok) {
            row.last=state;row.have=true;row.dirty=false;
        }
        if(!--budget)break;
    }
}
bool IsMaterialBirth() {
    if(!Current() || g_session->role()!=net::Role::Client || R::InCoopDispatch())return false;
    void* hand=hand_item::LocalHandActor();
    if(SG::IsObjectBodyActive(g_bagUse.Get(),hand))return true;
    if(SG::IsObjectBodyActive(g_trowelUse.Get(),hand) && SG::IsBodyActive(g_takeConcrete.Get()))return true;
    // Spilled wet/dry chunks and the replacement empty bucket belong to the
    // current physical holder. The passive host copy has its tick suppressed.
    void* tick=g_cementTick.Get();
    if(!SG::IsBodyActive(tick))return false;
    for(const auto& row:g_rows) {
        void* actor=row.actor.Get();
        if(actor && SG::IsObjectBodyActive(tick,actor) && NativeOwner(actor) && Eligible(actor))return true;
    }
    // Selected-hand stock is intentionally absent from world save publications.
    return SG::IsObjectBodyActive(tick,hand);
}
void OnDisconnect(){
    ++g_epoch;g_session=nullptr;g_world=0;g_rows={};
    g_cementTick.Reset();g_takeConcrete.Reset();g_bagUse.Reset();g_trowelUse.Reset();
}
}
