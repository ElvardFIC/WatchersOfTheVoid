#include "ue_wrap/actors/prop_fire.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::prop_fire {
namespace {
namespace R=reflection;
template<class T> bool ReadField(void* a,const wchar_t* name,T& v) {
    if(!a||!R::IsLive(a))return false;
    const int off=R::FindPropertyOffset(R::ClassOf(a),name);if(off<0)return false;
    std::memcpy(&v,static_cast<uint8_t*>(a)+off,sizeof(v));return true;
}
bool Bit(void* a,const wchar_t* name,bool& v) {
    int off=-1;uint8_t mask=0;
    if(!a||!R::IsLive(a)||!R::FindBoolProperty(R::ClassOf(a),name,off,mask)||off<0||!mask)return false;
    v=(static_cast<uint8_t*>(a)[off]&mask)!=0;return true;
}
void* Flame(void* a) {
    void* c=nullptr;
    return ReadField(a,L"flameBase",c)&&c&&R::IsLive(c)?c:nullptr;
}
bool Verb(void* a,const wchar_t* name,const float* fuel=nullptr,void* cls=nullptr) {
    ParamFrame f(R::FindDispatchFunctionCached(cls?cls:R::ClassOf(a),name));
    return f.valid()&&(!fuel||f.Set<float>(L"fuel",*fuel))&&Call(a,f);
}
}
bool Valid(const State& s) {
    return std::isfinite(s.fuel)&&s.fuel>=0&&s.fuel<=60&&
        std::isfinite(s.fuelSpeed)&&s.fuelSpeed>=0&&s.fuelSpeed<=100&&(!s.burning||s.fuel>0);
}
bool IsProp(void* a) {
    return a&&R::IsLive(a)&&prop::IsDescendantOfProp(a)&&world_identity::CurrentWorld()&&
        world_identity::WorldOf(a)==world_identity::CurrentWorld()&&
        !R::NameStartsWith(R::NameOf(a),L"Default__");
}
void* Owner(void* c) {
    void* a=nullptr;
    return c&&R::IsLive(c)&&R::ClassNameOf(c)==L"comp_flammable_C"&&
        ReadField(c,L"owner",a)&&IsProp(a)&&Flame(a)==c?a:nullptr;
}
bool Read(void* a,State& out) {
    if(!IsProp(a))return false;State s{};void* c=Flame(a);
    if(!c){out=s;return true;}
    if(Owner(c)!=a||!Bit(c,L"isBurning",s.burning))return false;
    if(s.burning) {
        if(!ReadField(c,L"fuel",s.fuel)||!ReadField(c,L"fuelSpeed",s.fuelSpeed))return false;
        // Native expiry can briefly leave a negative fuel before destroying the component.
        if(std::isfinite(s.fuel)&&s.fuel<=0){s.burning=false;s.fuel=0;}
    }
    if(!Valid(s))return false;out=s;return true;
}
void CollectBurning(std::vector<CachedObjRef>& actors) {
    actors.clear();void* cls=object_index::ClassByName(L"comp_flammable_C");if(!cls)return;
    object_index::ForEachInstance(cls,[](void* ctx,void* c,int32_t){
        void* a=Owner(c);State s{};if(!a||!Read(a,s)||!s.burning)return;
        auto& rows=*static_cast<std::vector<CachedObjRef>*>(ctx);
        if(rows.size()<512){CachedObjRef ref;ref.Set(a);rows.push_back(ref);}
    },&actors);
}
bool Ignite(void* a,float fuel) {
    return IsProp(a)&&std::isfinite(fuel)&&fuel>0&&fuel<=60&&Verb(a,L"ignite",&fuel);
}
bool Extinguish(void* a) {
    if(!IsProp(a))return false;void* c=Flame(a);
    return !c||Verb(c,L"extinguish");
}
bool Apply(void* a,const State& state,const std::function<bool()>& current) {
    if(!Valid(state)||!IsProp(a)||!current())return false;
    CachedObjRef actor;actor.Set(a);const auto world=world_identity::Generation();
    const auto valid=[&]{return actor.Is(a)&&world==world_identity::Generation()&&IsProp(a)&&current();};
    if(!state.burning){const bool ok=Extinguish(a);return ok&&valid();}
    State old{};if(!Read(a,old))return false;
    if(!old.burning) {
        // Base prop's ignite creates flameBase if absent. Its ignited callback is
        // suppressed by the client controller; subclass gameplay belongs to host.
        void* base=object_index::ClassByName(L"prop_C");
        if(!base||!Verb(a,L"ignite",&state.fuel,base)||!valid())return false;
    }
    void* c=Flame(a);if(!c||Owner(c)!=a)return false;
    int fuel=R::FindPropertyOffset(R::ClassOf(c),L"fuel");
    int speed=R::FindPropertyOffset(R::ClassOf(c),L"fuelSpeed");
    if(fuel<0||speed<0)return false;
    std::memcpy(static_cast<uint8_t*>(c)+fuel,&state.fuel,sizeof(float));
    std::memcpy(static_cast<uint8_t*>(c)+speed,&state.fuelSpeed,sizeof(float));
    State after{};return valid()&&Read(a,after)&&after.burning;
}
}
