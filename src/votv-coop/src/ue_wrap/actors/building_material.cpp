#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine_component.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::building_material {
namespace {
namespace R=reflection;
bool Family(void* actor,const wchar_t* name) {
    if(!actor || !R::IsLive(actor))return false;
    for(void* cls=R::ClassOf(actor);cls;cls=R::SuperStructOf(cls))
        if(R::NameEquals(R::NameOf(cls),name))return true;
    return false;
}
template<class T> bool Read(void* actor,const wchar_t* name,T& value) {
    const int off=R::FindPropertyOffset(R::ClassOf(actor),name);
    if(off<0)return false;
    std::memcpy(&value,static_cast<uint8_t*>(actor)+off,sizeof(T));return true;
}
bool Verb(void* actor,const wchar_t* name) {
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(actor),name));
    return f.valid() && Call(actor,f);
}
}
bool IsBucket(void* actor){return Family(actor,L"prop_bucket_C");}
bool IsTrowel(void* actor){return Family(actor,L"prop_wallfixer_C");}
bool IsConcreteBucket(void* actor){return Family(actor,L"prop_concreteBucket_C");}
bool SetRemoteConcreteBucketDisplay(void* actor,int32_t units,bool dry,const std::function<bool()>& current) {
    if((current && !current()) || units<0 || units>20 || !IsConcreteBucket(actor))return false;
    const auto world=world_identity::Generation();
    void* cls=R::ClassOf(actor);void* contents=nullptr;
    const int amount=R::FindPropertyOffset(cls,L"units");
    int32_t dryOff=-1;uint8_t dryMask=0;
    if(amount<0 || !R::FindBoolProperty(cls,L"dry",dryOff,dryMask) || dryOff<0 || !dryMask ||
       !Read(actor,L"StaticMesh1",contents) || !R::IsLive(contents))return false;
    CachedObjRef live,mesh;live.Set(actor);mesh.Set(contents);
    const auto valid=[&]{
        if(!live.Get() || !mesh.Get() || world!=world_identity::Generation() || (current && !current()))return false;
        void* assigned=nullptr;return Read(actor,L"StaticMesh1",assigned) && assigned==contents;
    };
    if(!valid())return false;
    auto* data=static_cast<uint8_t*>(actor);
    std::memcpy(data+amount,&units,sizeof(units));
    data[dryOff]=dry?uint8_t(data[dryOff]|dryMask):uint8_t(data[dryOff]&~dryMask);
    // updStage also calls updDry. Native stock replacement can briefly leave an
    // empty selected-hand mirror; hide its fill instead of displaying stage one.
    return Verb(actor,L"updStage") && valid() && engine::SetComponentVisible(contents,units>0,false) && valid();
}
bool ReadStock(void* actor,Stock& state) {
    state={};
    if(Family(actor,L"prop_concreteBucket_C"))state.kind=StockKind::ConcreteBucket;
    else if(Family(actor,L"prop_cementBag_C"))state.kind=StockKind::CementBag;
    else if(IsTrowel(actor))state.kind=StockKind::Trowel;
    else if(Family(actor,L"prop_sponge_C")) {
        state.kind=StockKind::Sponge;
        return Read(actor,L"power",state.wetness) && std::isfinite(state.wetness) && state.wetness>=0 &&
            Read(actor,L"soapAmount",state.soapAmount) && std::isfinite(state.soapAmount) &&
            state.soapAmount>=-5.f && state.soapAmount<=1'000'000.f;
    }
    else return false;
    if(!Read(actor,state.kind==StockKind::Trowel?L"concrete":L"units",state.units))return false;
    if(state.kind==StockKind::ConcreteBucket) {
        int32_t off=-1;uint8_t mask=0;
        if(!R::FindBoolProperty(R::ClassOf(actor),L"dry",off,mask) || off<0 || !mask)return false;
        state.dry=(static_cast<uint8_t*>(actor)[off]&mask)!=0;
        if (!Read(actor,L"dryTimer",state.dryTimer) || !std::isfinite(state.dryTimer) ||
            state.dryTimer<0 || state.dryTimer>1'000'000.f) return false;
    }
    // Native pouring can briefly leave units below zero before replacing the
    // bucket. Keep that observable; do not turn an exhausted bucket into a full one.
    return true;
}
bool ReadConcrete(void* actor,int32_t& units) {
    return IsTrowel(actor) && Read(actor,L"concrete",units) && units>=0 && units<=5;
}
bool RefreshTrowelRecord(void* actor,save_record::SaveRecord& record) {
    int32_t units=0;
    if(record.className!=L"prop_wallfixer_C" || record.key.empty() || record.key==L"None" ||
       !ReadConcrete(actor,units) || R::ClassNameOf(actor)!=record.className)return false;
    const int keyOff=R::FindPropertyOffset(R::ClassOf(actor),L"key");
    if(keyOff<0 || save_record::ReadFNameAt(actor,keyOff)!=record.key)return false;
    // prop_wallfixer.getData writes exactly one integer group [concrete].
    // hold[0].data may still describe the last equip/switch, not current usage.
    record.ints={{units}};
    return true;
}
bool RefreshHeldMaterialRecord(void* actor,save_record::SaveRecord& record) {
    const auto& cls=record.className;
    if(cls==L"prop_wallfixer_C")return RefreshTrowelRecord(actor,record);
    const bool water=cls==L"prop_bucket_C",concrete=cls==L"prop_concreteBucket_C";
    const bool bag=cls==L"prop_cementBag_C",sponge=cls==L"prop_sponge_C";
    if(!water && !concrete && !bag && !sponge)return true;
    if(!actor || !R::IsLive(actor) || R::ClassNameOf(actor)!=cls || record.key.empty() || record.key==L"None")return false;
    const int keyOff=R::FindPropertyOffset(R::ClassOf(actor),L"key");
    if(keyOff<0 || save_record::ReadFNameAt(actor,keyOff)!=record.key)return false;
    // Preserve base prop/container groups and all unrelated data. These slots are the exact
    // leaf getData/loadData schemas; never call native use/getData or mutate live inventory.
    if(water) {
        Water state{};
        if(record.floats.empty() || record.bools.empty() || !ReadWater(actor,state) || state.soap<0)return false;
        if(record.floats.size()<2)record.floats.resize(2);
        if(record.bools.size()<2)record.bools.resize(2);
        record.floats[1]={state.height<0?0.f:state.height}; // native getData uses max(height,0)
        record.bools[1]={uint8_t(state.alcohol)};
        record.ints={{state.soap}};
    } else {
        Stock state{};
        if(!ReadStock(actor,state))return false;
        if(sponge) {
            if(record.floats.empty())return false;
            if(record.floats.size()<2)record.floats.resize(2);
            record.floats[1]={state.wetness,state.soapAmount};
        } else {
            int32_t maximum=20;
            if(bag && (!Read(actor,L"maxUnits",maximum) || maximum<0))return false;
            if(state.units<0 || state.units>maximum || (concrete && record.bools.empty()))return false;
            if(concrete) {
                if(record.floats.size()<2)record.floats.resize(2);
                record.floats[1]={state.dryTimer};
                if(record.bools.size()<2)record.bools.resize(2);
                record.bools[1]={uint8_t(state.dry)};
                if(record.ints.empty())record.ints.resize(1);
                record.ints[0]={state.units};
            } else record.ints={{state.units}};
        }
    }
    return true;
}
bool SetRemoteConcrete(void* actor,int32_t units,const std::function<bool()>& current) {
    if((current && !current()) || units<0 || units>5 || !IsTrowel(actor))return false;
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    const auto valid=[&]{return live.Get() && world==world_identity::Generation() && (!current || current());};
    const int off=R::FindPropertyOffset(R::ClassOf(actor),L"concrete");
    if(off<0 || !valid())return false;
    std::memcpy(static_cast<uint8_t*>(actor)+off,&units,sizeof(units));
    return Verb(actor,L"updConcrete") && valid(); // visibility only; no native use/repair/spending
}
bool ReadWater(void* actor,Water& state) {
    if(!IsBucket(actor) || !Read(actor,L"height",state.height) ||
       !Read(actor,L"soap",state.soap) || !std::isfinite(state.height))return false;
    int32_t off=-1;uint8_t mask=0;
    if(!R::FindBoolProperty(R::ClassOf(actor),L"alcohol",off,mask) || off<0 || !mask)return false;
    state.alcohol=(static_cast<uint8_t*>(actor)[off]&mask)!=0;return true;
}
bool RepairBucketEmpty(void* actor) {
    Water state{};if(!ReadWater(actor,state))return false;
    int32_t off=-1;uint8_t mask=0;
    if(!R::FindBoolProperty(R::ClassOf(actor),L"empty",off,mask) || off<0 || !mask)return false;
    auto& flags=static_cast<uint8_t*>(actor)[off];
    flags=state.height<=0 ? uint8_t(flags|mask) : uint8_t(flags&~mask);
    return true;
}
bool RepaintBucket(void* actor,const std::function<bool()>& current) {
    if((current && !current()) || !actor || !R::IsLive(actor))return false;
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    const auto valid=[&]{return live.Get() && world==world_identity::Generation() && (!current || current());};
    Water state{};if(!ReadWater(actor,state) || !valid())return false;
    const int off=R::FindPropertyOffset(R::ClassOf(actor),L"height_interp");
    if(off<0 || !valid())return false;
    std::memcpy(static_cast<uint8_t*>(actor)+off,&state.height,sizeof(state.height));
    return Verb(actor,L"calcPoints") && valid() && Verb(actor,L"upd") && valid();
}
bool SetRemoteBucketDisplay(void* actor,float height,bool soap,bool alcohol,const std::function<bool()>& current) {
    // The native bucket's maxHeight is38; world/inventory amounts remain on the
    // owner's save lane. Only the disposable hand mirror receives these fields.
    if((current && !current()) || !std::isfinite(height) || height<0 || height>38 || !IsBucket(actor))return false;
    void* cls=R::ClassOf(actor);
    const int level=R::FindPropertyOffset(cls,L"height"), suds=R::FindPropertyOffset(cls,L"soap");
    int32_t alcoholOff=-1,emptyOff=-1;uint8_t alcoholMask=0,emptyMask=0;
    if(level<0 || suds<0 || !R::FindBoolProperty(cls,L"alcohol",alcoholOff,alcoholMask) ||
        alcoholOff<0 || !alcoholMask || !R::FindBoolProperty(cls,L"empty",emptyOff,emptyMask) ||
        emptyOff<0 || !emptyMask)return false;
    auto* data=static_cast<uint8_t*>(actor);const int32_t soapDisplay=soap?1:0;
    std::memcpy(data+level,&height,sizeof(height));
    std::memcpy(data+suds,&soapDisplay,sizeof(soapDisplay));
    data[alcoholOff]=alcohol && height>0 ? uint8_t(data[alcoholOff]|alcoholMask) : uint8_t(data[alcoholOff]&~alcoholMask);
    data[emptyOff]=height<=0 ? uint8_t(data[emptyOff]|emptyMask) : uint8_t(data[emptyOff]&~emptyMask);
    return RepaintBucket(actor,current);
}
bool TickBucketContents(void* actor,float seconds) {
    if(!IsBucket(actor) || !std::isfinite(seconds) || seconds<0)return false;
    void* cls=R::ClassOf(actor);
    while(cls && !R::NameEquals(R::NameOf(cls),L"prop_bucket_C"))cls=R::SuperStructOf(cls);
    void* parent=cls?R::SuperStructOf(cls):nullptr;
    if(!parent)return false;
    ParamFrame f(R::FindDispatchFunctionCached(parent,L"ReceiveTick"));
    return f.valid() && f.Set<float>(L"DeltaSeconds",seconds) && Call(actor,f);
}
}
