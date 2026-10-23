#include "ue_wrap/world/ariral_ship.h"
#include "ue_wrap/world/active_events.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include <cmath>
#include <cstring>

namespace ue_wrap::ariral_ship {
namespace {
namespace R = reflection;
struct Schema {
    CachedObjRef cls;
    int32_t visible=-1, alpha=-1, mesh=-1, light=-1, radar=-1, air=-1, timeline=-1, gamemode=-1;
    uint8_t visibleMask=0;
} schema;
constexpr const wchar_t* kAlpha = L"a_a_2C6617ED4975EC563C73D5B0DBE0638B";
bool Resolve(void* actor) {
    if (!IsActor(actor)) return false;
    void* cls=R::ClassOf(actor);
    if (schema.cls.Is(cls)) return true;
    Schema next;
    if (!R::FindBoolProperty(cls,L"visible",next.visible,next.visibleMask) || next.visible<0 || !next.visibleMask) return false;
    next.alpha=R::FindPropertyOffset(cls,kAlpha);
    next.mesh=R::FindPropertyOffset(cls,L"StaticMesh");
    next.light=R::FindPropertyOffset(cls,L"PointLight");
    next.radar=R::FindPropertyOffset(cls,L"radarPoint");
    next.air=R::FindPropertyOffset(cls,L"air");
    next.timeline=R::FindPropertyOffset(cls,L"a");
    next.gamemode=R::FindPropertyOffset(cls,L"gamemode");
    if (next.alpha<0 || next.mesh<0 || next.light<0 || next.radar<0 || next.air<0 || next.timeline<0 || next.gamemode<0) return false;
    next.cls.Set(cls);schema=next;return true;
}
void* Object(void* actor,int32_t offset) {
    void* value=nullptr;std::memcpy(&value,static_cast<uint8_t*>(actor)+offset,sizeof(value));
    return value && R::IsLive(value) ? value : nullptr;
}
bool HasSender(void* actor,bool& has) {
    active_events::Senders senders{};
    if (!active_events::ReadSenders(senders)) return false;
    has=false;for(int i=0;i<senders.num;++i)has|=senders.data[i]==actor;
    return true;
}
void* DefaultObject(const wchar_t* cls) {return R::FindClassDefaultObject(cls);}
}
bool IsActor(void* actor) {
    void* cls=actor && R::IsLive(actor) ? R::ClassOf(actor) : nullptr;
    return cls && R::NameEquals(R::NameOf(cls),L"arirShipAppear_C");
}
std::vector<void*> Actors() {
    std::vector<void*> result;
    void* cls=object_index::ClassByName(L"arirShipAppear_C");
    if (!cls || !world_identity::CurrentWorld()) return result;
    object_index::ForEachInstance(cls,[](void* ctx,void* actor,int32_t index) {
        auto& result=*static_cast<std::vector<void*>*>(ctx);
        if(result.size()<32 && R::IsLiveByIndex(actor,index) &&
           world_identity::WorldOf(actor)==world_identity::CurrentWorld())result.push_back(actor);
    },&result);
    return result;
}
bool Read(void* actor,Appearance& out) {
    if(!Resolve(actor))return false;
    CachedObjRef ref;ref.Set(actor);const auto world=world_identity::Generation();
    Appearance value;value.name=R::ToString(R::NameOf(actor));
    value.visible=(static_cast<uint8_t*>(actor)[schema.visible]&schema.visibleMask)!=0;
    std::memcpy(&value.alpha,static_cast<uint8_t*>(actor)+schema.alpha,sizeof(value.alpha));
    if(!std::isfinite(value.alpha) || value.alpha<0 || value.alpha>1 ||
       !engine::TryGetActorLocation(actor,value.at) || !ref.Get() || world!=world_identity::Generation())return false;
    out=std::move(value);return true;
}
bool Paint(void* actor,const Appearance& state,const std::function<bool()>& current) {
    if(!current || !current() || !Resolve(actor) || !std::isfinite(state.alpha) || state.alpha<0 || state.alpha>1)return false;
    const Schema fields=schema;
    void* mesh=Object(actor,fields.mesh),*light=Object(actor,fields.light),*radar=Object(actor,fields.radar),*timeline=Object(actor,fields.timeline);
    if(!mesh || !light || !radar || !timeline)return false;
    CachedObjRef a,m,l,r,t;a.Set(actor);m.Set(mesh);l.Set(light);r.Set(radar);t.Set(timeline);
    const auto live=[&]{return current() && a.Get() && m.Get() && l.Get() && r.Get() && t.Get() &&
        Object(a.Get(),fields.mesh)==m.Get() && Object(a.Get(),fields.light)==l.Get() &&
        Object(a.Get(),fields.radar)==r.Get() && Object(a.Get(),fields.timeline)==t.Get();};
    int32_t radarOffset=-1;uint8_t radarMask=0;
    if(!R::FindBoolProperty(R::ClassOf(radar),L"active",radarOffset,radarMask) || radarOffset<0 || !radarMask)return false;
    ParamFrame material(R::FindDispatchFunctionCached(R::ClassOf(mesh),L"SetScalarParameterValueOnMaterials"));
    ParamFrame intensity(R::FindDispatchFunctionCached(R::ClassOf(light),L"SetIntensity"));
    ParamFrame stop(R::FindDispatchFunctionCached(R::ClassOf(timeline),L"Stop"));
    const auto alphaName=fname_utils::StringToFName(L"alpha");
    if(!live() || !alphaName.ComparisonIndex || !material.valid() || !intensity.valid() || !stop.valid() ||
       !material.Set<R::FName>(L"ParameterName",alphaName) || !material.Set<float>(L"ParameterValue",state.alpha) ||
       !intensity.Set<float>(L"NewIntensity",std::pow(state.alpha,10.f)*100000.f))return false;
    // A timeline begun before joining writes its float track even when its BP
    // Update callback is refused. Stop that producer before painting host alpha.
    if(!Call(timeline,stop) || !live())return false;
    auto* bytes=static_cast<uint8_t*>(actor);
    bytes[fields.visible]=(bytes[fields.visible]&~fields.visibleMask)|(state.visible?fields.visibleMask:0);
    std::memcpy(bytes+fields.alpha,&state.alpha,sizeof(state.alpha));
    auto* radarBytes=static_cast<uint8_t*>(radar);
    radarBytes[radarOffset]=(radarBytes[radarOffset]&~radarMask)|(state.visible?radarMask:0);
    if(!live() || !Call(mesh,material) || !live() || !Call(light,intensity) || !live())return false;
    return true;
}
bool PhotoSubscription(void* actor,bool active,const std::function<bool()>& current) {
    if(!current || !current() || !Resolve(actor))return false;
    const auto fields=schema;
    void* gm=Object(actor,fields.gamemode);if(!gm)return false;
    CachedObjRef ref,mode;ref.Set(actor);mode.Set(gm);
    const auto live=[&]{return current() && ref.Get() && mode.Get() && Object(ref.Get(),fields.gamemode)==mode.Get();};
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(actor),L"runTrigger"));
    return f.valid() && f.Set<void*>(L"owner",nullptr) && f.Set<int32_t>(L"index",active?0:1) &&
        live() && Call(actor,f) && live();
}
bool Activity(void* actor,bool active,const std::function<bool()>& current) {
    if(!current || !current() || !IsActor(actor))return false;
    bool present=false;if(!HasSender(actor,present))return false;
    if(present==active)return current();
    CachedObjRef ref;ref.Set(actor);
    void* cdo=DefaultObject(L"lib_C");if(!cdo)return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(cdo),L"setEvent"));
    if(!f.valid() || !f.Set<bool>(L"isEventActive",active) || !f.Set<bool>(L"deactivateAmbientTrack",false) ||
       !f.Set<void*>(L"__WorldContext",actor) || !current() || !ref.Get() || !Call(cdo,f) || !current() || !ref.Get())return false;
    return HasSender(actor,present) && present==active && current();
}
bool Air(void* actor,const std::function<bool()>& current) {
    if(!current || !current() || !Resolve(actor))return false;
    void* attach=Object(actor,schema.air);if(!attach)return false;
    static CachedObjRef particle;
    if(!particle.Get()) {
        void* cls=object_index::ClassByName(L"ParticleSystem");if(!cls)return false;
        object_index::ForEachInstance(cls,[](void* ctx,void* object,int32_t index) {
            auto& out=*static_cast<CachedObjRef*>(ctx);
            if(!out.Get() && R::IsLiveByIndex(object,index) && R::NameEquals(R::NameOf(object),L"eff_arirShipAir"))out.Set(object);
        },&particle);
    }
    void* cdo=DefaultObject(L"GameplayStatics");if(!cdo || !particle.Get())return false;
    const auto fields=schema;
    CachedObjRef a,t,asset=particle;a.Set(actor);t.Set(attach);
    const auto live=[&]{return current() && a.Get() && t.Get() && asset.Get() && Object(a.Get(),fields.air)==t.Get();};
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(cdo),L"SpawnEmitterAttached"));
    if(!f.valid() || !f.Set<void*>(L"EmitterTemplate",asset.Get()) || !f.Set<void*>(L"AttachToComponent",attach) ||
       !f.Set<R::FName>(L"AttachPointName",{}) || !f.Set<FVector>(L"Location",{}) || !f.Set<FRotator>(L"Rotation",{}) ||
       !f.Set<FVector>(L"Scale",{1,1,1}) || !f.Set<uint8_t>(L"LocationType",0) || !f.Set<bool>(L"bAutoDestroy",true) ||
       !f.Set<uint8_t>(L"PoolingMethod",0) || !f.Set<bool>(L"bAutoActivateSystem",true) || !live())return false;
    return Call(cdo,f) && f.Get<void*>(L"ReturnValue") && live();
}
}
