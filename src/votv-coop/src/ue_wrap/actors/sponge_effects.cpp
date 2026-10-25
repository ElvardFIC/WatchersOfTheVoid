#include "ue_wrap/actors/sponge_effects.h"
#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/engine.h"
#include <chrono>
#include <cstring>
namespace ue_wrap::sponge_effects {
namespace {
namespace R=reflection;
void* Template() {
    static CachedObjRef result;static uint64_t retry=0;
    if(result.Get())return result.Get();
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if(now<retry)return nullptr;retry=now+1000;
    void* cls=object_index::ClassByName(L"ParticleSystem");if(!cls)return nullptr;
    object_index::ForEachInstance(cls,[](void* ctx,void* object,int32_t){
        if(!R::NameEquals(R::NameOf(object),L"eff_cleanBubbles"))return;
        void* package=R::OuterOf(object);
        if(package && !R::OuterOf(package) && R::NameEquals(R::NameOf(package),L"/Game/particles/eff_cleanBubbles"))
            static_cast<CachedObjRef*>(ctx)->Set(object);
    },&result);
    return result.Get();
}
}
bool IsSponge(void* actor) {
    if(!actor || !R::IsLive(actor))return false;
    void* base=object_index::ClassByName(L"prop_sponge_C");
    return base && R::IsDescendantOfAny(R::ClassOf(actor),&base,1);
}
bool ReadSoap(void* actor,float& soap) {
    if(!IsSponge(actor))return false;
    building_material::Stock stock{};
    if(!building_material::ReadStock(actor,stock) || stock.kind!=building_material::StockKind::Sponge ||
       !std::isfinite(stock.soapAmount))return false;
    soap=stock.soapAmount;return true;
}
bool ReadUse(void* actor,void* player,Use& out) {
    if(!player || !R::IsLive(player) || !IsSponge(actor))return false;
    CachedObjRef live;live.Set(actor);CachedObjRef user;user.Set(player);
    CachedObjRef actorClass,playerClass;
    actorClass.Set(R::ClassOf(actor));playerClass.Set(R::ClassOf(player));
    const auto world=world_identity::Generation();
    const auto valid=[&]{return live.Get() && user.Get() && actorClass.Get() && playerClass.Get() &&
        world==world_identity::Generation() && R::ClassOf(actor)==actorClass.Raw() && R::ClassOf(player)==playerClass.Raw();};
    static R::InstanceOffset playerField{L"player"},hitField{L"hitResult"};
    const int po=playerField.Of(actor);if(!valid() || po<0)return false;
    const int hit=hitField.Of(player);if(!valid() || hit<0)return false;
    void* actual=nullptr;std::memcpy(&actual,static_cast<uint8_t*>(actor)+po,sizeof(actual));
    if(actual!=player)return false;
    void* inner=R::PropertyInnerStruct(playerClass.Raw(),L"hitResult");if(!valid() || !inner || !R::IsLive(inner))return false;
    CachedObjRef schema;schema.Set(inner);
    const auto bound=[&]{
        if(!valid() || !schema.Get())return false;
        void* assigned=nullptr;std::memcpy(&assigned,static_cast<uint8_t*>(actor)+po,sizeof(assigned));
        if(assigned!=player)return false;
        void* current=R::PropertyInnerStruct(playerClass.Raw(),L"hitResult");
        return current==inner && valid() && schema.Get();
    };
    const auto size=R::StructSize(inner);if(!bound())return false;
    const int location=R::FindPropertyOffset(inner,L"Location");if(!bound())return false;
    const int normal=R::FindPropertyOffset(inner,L"Normal");if(!bound())return false;
    constexpr auto vectorBytes=int32_t(sizeof(FVector));
    if(size<vectorBytes || location<0 || normal<0 || location>size-vectorBytes || normal>size-vectorBytes ||
       !bound())return false;
    Use value{};
    std::memcpy(&value.location,static_cast<uint8_t*>(player)+hit+location,sizeof(FVector));
    std::memcpy(&value.normal,static_cast<uint8_t*>(player)+hit+normal,sizeof(FVector));
    if(!ValidSurface(value.location,value.normal) || !ReadSoap(actor,value.soap) || value.soap<=0 ||
       !bound())return false;
    out=value;return true;
}
bool Surface(const FVector& location,const FVector& normal,const std::function<bool()>& current) {
    if(!ValidSurface(location,normal) || !current())return false;
    const auto world=world_identity::Generation();
    CachedObjRef particle;particle.Set(Template());
    void* library=R::FindClassDefaultObject(L"GameplayStatics");void* context=engine::GetWorldContext();
    CachedObjRef target;target.Set(context);
    const auto valid=[&]{return world==world_identity::Generation() && target.Get() && particle.Get() && current();};
    if(!particle.Get() || !library || !context || !valid())return false;
    constexpr float degrees=57.29577951308232f;
    const FRotator rotation{std::atan2(normal.Z,std::sqrt(normal.X*normal.X+normal.Y*normal.Y))*degrees,
        std::atan2(normal.Y,normal.X)*degrees,0};
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(library),L"SpawnEmitterAtLocation"));
    if(!frame.valid() || !frame.Set<void*>(L"WorldContextObject",context) || !frame.Set<void*>(L"EmitterTemplate",particle.Get()) ||
       !frame.Set<FVector>(L"Location",location) || !frame.Set<FRotator>(L"Rotation",rotation) ||
       !frame.Set<FVector>(L"Scale",{1,1,1}) || !frame.Set<bool>(L"bAutoDestroy",true) ||
       !frame.Set<uint8_t>(L"PoolingMethod",0) || !frame.Set<bool>(L"bAutoActivate",true) || !valid())return false;
    Call(library,frame);return true;
}
bool Burst(void* actor,const std::function<bool()>& current) {
    if((current && !current()) || !IsSponge(actor))return false;
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    static R::InstanceOffset field{L"eff_bubbles"};const auto offset=field.Of(actor);
    if(offset<0 || !live.Get() || world!=world_identity::Generation() || (current && !current()))return false;
    void* effect=nullptr;std::memcpy(&effect,static_cast<uint8_t*>(actor)+offset,sizeof(effect));
    CachedObjRef component;component.Set(effect);if(!component.Get())return false;
    const auto valid=[&]{
        if(!live.Get() || !component.Get() || world!=world_identity::Generation() || (current && !current()))return false;
        void* assigned=nullptr;std::memcpy(&assigned,static_cast<uint8_t*>(actor)+offset,sizeof(assigned));
        return assigned==effect;
    };
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(effect),L"Activate"));
    if(!frame.valid() || !frame.Set<bool>(L"bReset",false) || !valid())return false;
    Call(effect,frame);return true;
}
}
