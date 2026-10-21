#include "ue_wrap/actors/water_pour.h"
#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/engine.h"
#include <chrono>
#include <cmath>
#include <cstring>
namespace ue_wrap::water_pour {
namespace {
namespace R=reflection;
bool Point(const FVector& point) {
    for(float value:{point.X,point.Y,point.Z})if(!std::isfinite(value) || std::fabs(value)>1.e7f)return false;
    return true;
}
void* Asset(size_t index) {
    struct Row {const wchar_t* name;const wchar_t* cls;const wchar_t* package;CachedObjRef ref;uint64_t retry=0;};
    static Row rows[]{
        {L"eff_waterThrowSplash",L"ParticleSystem",L"/Game/particles/eff_waterThrowSplash"},
        {L"wade_Cue",L"SoundCue",L"/Game/audio/effects/physics/footsteps/wade_Cue"},
        {L"att_default",L"SoundAttenuation",L"/Game/audio/misc/att_default"},
        {L"water_splash_Cue",L"SoundCue",L"/Game/audio/effects/physics/water/water_splash_Cue"}};
    auto& row=rows[index];if(row.ref.Get())return row.ref.Get();
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if(now<row.retry)return nullptr;row.retry=now+1000;
    if(void* cls=object_index::ClassByName(row.cls))object_index::ForEachInstance(cls,[](void* p,void* object,int32_t){
        auto& row=*static_cast<Row*>(p);if(!R::NameEquals(R::NameOf(object),row.name))return;
        void* package=R::OuterOf(object);
        if(package && !R::OuterOf(package) && R::NameEquals(R::NameOf(package),row.package))row.ref.Set(object);
    },&row);
    return row.ref.Get();
}
struct Context {
    uint32_t world=world_identity::Generation();
    CachedObjRef target;
    std::function<bool()> predicate;
    Context(void* object,const std::function<bool()>& current):predicate(current){target.Set(object);}
    bool Current()const{return world==world_identity::Generation() && target.Get() && predicate && predicate();}
};
}
bool IsPour(void* actor) {
    if(!actor || !R::IsLive(actor))return false;
    void* cls=object_index::ClassByName(L"prop_sponge_bucketPour_C");return cls && R::ClassOf(actor)==cls;
}
bool IsShot(void* actor) {
    if(!actor || !R::IsLive(actor))return false;
    void* cls=object_index::ClassByName(L"prop_sponge_p_C");return cls && R::ClassOf(actor)==cls;
}
bool IsProjectile(void* actor){return IsPour(actor) || IsShot(actor);}
bool Valid(const Birth& state) {
    return (state.kind==Kind::Bucket || state.kind==Kind::Washer) && state.power==1 &&
        (state.kind!=Kind::Washer || state.soap==0) &&
        std::isfinite(state.rotation.Pitch) && std::isfinite(state.rotation.Yaw) && std::isfinite(state.rotation.Roll) &&
        std::fabs(state.rotation.Pitch)<=36000 && std::fabs(state.rotation.Yaw)<=36000 && std::fabs(state.rotation.Roll)<=36000 &&
        Point(state.location) && Point(state.velocity) &&
        state.velocity.X*state.velocity.X+state.velocity.Y*state.velocity.Y+state.velocity.Z*state.velocity.Z<=4.e8f &&
        std::isfinite(state.soap) && state.soap>=0 && state.soap<=1.e6f;
}
bool Valid(const Impact& state) {
    const float length=state.normal.X*state.normal.X+state.normal.Y*state.normal.Y+state.normal.Z*state.normal.Z;
    return Point(state.location) && Point(state.audioLocation) && std::isfinite(length) && length>=.5f && length<=1.5f;
}
bool ReadBirth(void* actor,Birth& out,const std::function<bool()>& current) {
    if(!IsProjectile(actor))return false;Context live(actor,current);if(!live.Current())return false;
    Birth state{};state.kind=IsShot(actor)?Kind::Washer:Kind::Bucket;
    if(!engine::TryGetActorLocation(actor,state.location) || !live.Current())return false;
    if(!engine::TryGetActorRotation(actor,state.rotation) || !live.Current())return false;
    ParamFrame velocity(R::FindDispatchFunctionCached(R::ClassOf(actor),L"GetVelocity"));
    if(!velocity.valid() || !live.Current() || !Call(actor,velocity) || !live.Current() ||
       !velocity.GetRaw(L"ReturnValue",&state.velocity,sizeof(state.velocity)))return false;
    static R::InstanceOffset soap{L"soapAmount"},power{L"power"};
    const int off=state.kind==Kind::Bucket?soap.Of(actor):power.Of(actor);
    if(off<0 || !live.Current())return false;
    if(state.kind==Kind::Bucket)std::memcpy(&state.soap,static_cast<uint8_t*>(actor)+off,sizeof(float));
    else std::memcpy(&state.power,static_cast<uint8_t*>(actor)+off,sizeof(float));
    if(!Valid(state))return false;out=state;return true;
}
bool ReadImpact(void* actor,void* other,void* function,const void* hit,Impact& out) {
    if(!IsProjectile(actor) || !function || !hit)return false;
    void* ignored=object_index::ClassByName(L"prop_sponge_p_C");
    if(other && R::IsLive(other) && ignored && R::IsDescendantOfAny(R::ClassOf(other),&ignored,1))return false;
    Context live(actor,[]{return true;});
    void* inner=R::PropertyInnerStruct(function,IsShot(actor)?L"Hit":L"InputPin5");
    if(!inner || !R::NameEquals(R::NameOf(inner),L"HitResult"))return false;
    const int size=R::StructSize(inner),point=R::FindPropertyOffset(inner,L"Location"),normal=R::FindPropertyOffset(inner,L"Normal");
    if(size<int(sizeof(FVector)) || point<0 || normal<0 || point>size-int(sizeof(FVector)) ||
       normal>size-int(sizeof(FVector)) || !live.Current())return false;
    Impact state{};
    std::memcpy(&state.location,static_cast<const uint8_t*>(hit)+point,sizeof(FVector));
    std::memcpy(&state.normal,static_cast<const uint8_t*>(hit)+normal,sizeof(FVector));
    if(!engine::TryGetActorLocation(actor,state.audioLocation) || !live.Current() || !Valid(state))return false;
    out=state;return true;
}
Spawn Author(const Birth& state,const std::function<bool()>& current) {
    if(!Valid(state) || !current || !current())return {};
    const bool shot=state.kind==Kind::Washer;
    void* cls=object_index::ClassByName(shot?L"prop_sponge_p_C":L"prop_sponge_bucketPour_C");
    const int field=cls?R::FindPropertyOffset(cls,shot?L"power":L"soapAmount"):-1;
    ParamFrame life(shot && cls?R::FindDispatchFunctionCached(cls,L"SetLifeSpan"):nullptr);
    if(!cls || field<0 || (shot && (!life.valid() || !life.Set<float>(L"InLifespan",10))) || !current())return {};
    const auto world=world_identity::Generation();
    void* actor=engine::BeginDeferredSpawn(cls,state.location,state.rotation);CachedObjRef live;live.Set(actor);
    const auto valid=[&]{return world==world_identity::Generation() && live.Get() && current();};
    const auto failed=[&]()->Spawn {
        if(world==world_identity::Generation())if(void* begun=live.Get())engine::DestroyActor(begun);
        return {nullptr,true};
    };
    if(!valid())return failed();
    if(!shot)std::memcpy(static_cast<uint8_t*>(actor)+field,&state.soap,sizeof(float));
    if(!engine::FinishDeferredSpawn(actor,state.location,state.rotation) || !valid())return failed();
    // BeginPlay offsets the actor by its bounds centre. The captured location
    // already contains that offset; restore the final owner transform once.
    if(!engine::SetActorLocation(actor,state.location) || !valid() ||
       !engine::SetActorRotation(actor,state.rotation) || !valid())return failed();
    if(shot){
        std::memcpy(static_cast<uint8_t*>(actor)+field,&state.power,sizeof(float));
        if(!Call(actor,life) || !valid())return failed();
    }
    void* mesh=engine::GetStaticMeshComponent(actor);CachedObjRef component;component.Set(mesh);
    if(!valid() || !component.Get() || !engine::SetComponentLinearVelocity(mesh,state.velocity.X,state.velocity.Y,state.velocity.Z) ||
       !valid() || !component.Get())return failed();
    return {actor,true};
}
bool Surface(const Impact& state,const std::function<bool()>& current) {
    if(!Valid(state) || !current || !current())return false;
    Context live(engine::GetWorldContext(),current);CachedObjRef particle;particle.Set(Asset(0));
    void* cdo=R::FindClassDefaultObject(L"GameplayStatics");if(!cdo || !particle.Get() || !live.Current())return false;
    constexpr float degrees=57.29577951308232f;
    const FRotator rotation{std::atan2(state.normal.Z,std::sqrt(state.normal.X*state.normal.X+state.normal.Y*state.normal.Y))*degrees,
        std::atan2(state.normal.Y,state.normal.X)*degrees,0};
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(cdo),L"SpawnEmitterAtLocation"));
    if(!frame.valid() || !frame.Set<void*>(L"WorldContextObject",live.target.Get()) || !frame.Set<void*>(L"EmitterTemplate",particle.Get()) ||
       !frame.Set(L"Location",state.location) || !frame.Set(L"Rotation",rotation) || !frame.Set(L"Scale",FVector{1,1,1}) ||
       !frame.Set<bool>(L"bAutoDestroy",true) || !frame.Set<uint8_t>(L"PoolingMethod",0) || !frame.Set<bool>(L"bAutoActivate",true) ||
       !live.Current() || !particle.Get())return false;
    Call(cdo,frame);return true;
}
namespace {
bool SoundAt(const Impact& state,const std::function<bool()>& current,size_t asset,float volume,float pitch) {
    if(!Valid(state) || !current || !current())return false;
    Context live(engine::GetWorldContext(),current);CachedObjRef sound,attenuation;sound.Set(Asset(asset));attenuation.Set(Asset(2));
    void* cdo=R::FindClassDefaultObject(L"GameplayStatics");if(!cdo || !sound.Get() || !attenuation.Get() || !live.Current())return false;
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(cdo),L"PlaySoundAtLocation"));
    if(!frame.valid() || !frame.Set<void*>(L"WorldContextObject",live.target.Get()) || !frame.Set<void*>(L"Sound",sound.Get()) ||
       !frame.Set(L"Location",state.audioLocation) || !frame.Set(L"Rotation",FRotator{}) || !frame.Set<float>(L"VolumeMultiplier",volume) ||
       !frame.Set<float>(L"PitchMultiplier",pitch) || !frame.Set<float>(L"StartTime",0) || !frame.Set<void*>(L"AttenuationSettings",attenuation.Get()) ||
       !frame.Set<void*>(L"ConcurrencySettings",nullptr) || !frame.Set<void*>(L"OwningActor",nullptr) || !live.Current() || !sound.Get() || !attenuation.Get())return false;
    Call(cdo,frame);return true;
}
bool SoundAsset(void* object,const wchar_t* name,const wchar_t* package){
    if(!object || !R::IsLive(object) || !R::NameEquals(R::NameOf(object),name))return false;
    void* cls=R::ClassOf(object);void* outer=R::OuterOf(object);
    return cls && R::NameEquals(R::NameOf(cls),L"SoundCue") && outer && !R::OuterOf(outer) && R::NameEquals(R::NameOf(outer),package);
}
}
bool Sound(const Impact& state,const std::function<bool()>& current){return SoundAt(state,current,1,1,state.launch?1.5f:2.f);}
bool BucketSound(const BucketCue& cue,const std::function<bool()>& current){
    if(cue.kind>2 || !Point(cue.location))return false;
    const float volume[]={.25f,1.f,.5f},pitch[]={1.f,1.5f,1.f};
    return SoundAt({cue.location,{0,0,1},cue.location},current,cue.kind==2?3:1,volume[cue.kind],pitch[cue.kind]);
}
bool ReadBucketCue(void* source,void* function,const uint8_t* params,bool twoD,BucketCue& cue,const std::function<bool()>& current){
    if(!function || !params || !current || !current() || !building_material::IsBucket(source))return false;
    Context live(source,current);if(!live.Current())return false;
    const auto size=R::FunctionFrameSize(function);
    const auto read=[&](const wchar_t* name,void* out,int bytes){const int off=R::FindParamOffset(function,name);
        if(off<0 || off>size-bytes || !live.Current())return false;std::memcpy(out,params+off,bytes);return true;};
    void* context=nullptr;void* sound=nullptr;float volume=0,pitch=0;BucketCue value{};
    if(!read(L"WorldContextObject",&context,sizeof(context)) || context!=source || !read(L"Sound",&sound,sizeof(sound)) ||
       !read(L"VolumeMultiplier",&volume,sizeof(volume)) || !read(L"PitchMultiplier",&pitch,sizeof(pitch)))return false;
    CachedObjRef audio;audio.Set(sound);
    const bool wade=SoundAsset(sound,L"wade_Cue",L"/Game/audio/effects/physics/footsteps/wade_Cue");
    if(!twoD && wade && volume==.25f && pitch==1)value.kind=0;
    else if(twoD && wade && volume==1 && pitch==1.5f)value.kind=1;
    else if(twoD && volume==.5f && pitch==1 && SoundAsset(sound,L"water_splash_Cue",L"/Game/audio/effects/physics/water/water_splash_Cue"))value.kind=2;
    else return false;
    if(!live.Current() || !audio.Get())return false;
    if(twoD){if(!engine::TryGetActorLocation(source,value.location) || !live.Current() || !audio.Get())return false;}
    else if(!read(L"Location",&value.location,sizeof(value.location)))return false;
    if(!Point(value.location) || !live.Current() || !audio.Get())return false;cue=value;return true;
}
}
