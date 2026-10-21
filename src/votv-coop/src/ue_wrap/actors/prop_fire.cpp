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
    CachedObjRef actor,cls;actor.Set(a);cls.Set(R::ClassOf(a));const auto world=world_identity::Generation();
    const int off=R::FindPropertyOffset(cls.Get(),name);
    if(off<0 || actor.Get()!=a || !cls.Get() || R::ClassOf(a)!=cls.Raw() || world!=world_identity::Generation())return false;
    std::memcpy(&v,static_cast<uint8_t*>(a)+off,sizeof(v));return true;
}
bool Bit(void* a,const wchar_t* name,bool& v) {
    int off=-1;uint8_t mask=0;
    if(!a||!R::IsLive(a))return false;
    CachedObjRef actor,cls;actor.Set(a);cls.Set(R::ClassOf(a));const auto world=world_identity::Generation();
    if(!R::FindBoolProperty(cls.Get(),name,off,mask)||off<0||!mask || actor.Get()!=a || !cls.Get() ||
       R::ClassOf(a)!=cls.Raw() || world!=world_identity::Generation())return false;
    v=(static_cast<uint8_t*>(a)[off]&mask)!=0;return true;
}
bool TryFlame(void* a,void*& c){
    c=nullptr;if(!ReadField(a,L"flameBase",c))return false;
    if(c && !R::IsLive(c))c=nullptr;return true;
}
void* Flame(void* a) {
    void* c=nullptr;return TryFlame(a,c)?c:nullptr;
}
bool Verb(void* a,const wchar_t* name,const float* fuel=nullptr,void* cls=nullptr,const std::function<bool()>& current={},bool mayDestroy=false,bool* attempted=nullptr) {
    if(attempted)*attempted=false;
    if(!a || !R::IsLive(a) || (current && !current()))return false;
    CachedObjRef actor,source,declaring,fn;actor.Set(a);source.Set(R::ClassOf(a));declaring.Set(cls?cls:source.Get());
    const auto world=world_identity::Generation();
    const auto valid=[&]{return actor.Get()==a && source.Get() && R::ClassOf(a)==source.Raw() && declaring.Get() &&
        world==world_identity::Generation() && (!current || current());};
    if(!valid())return false;fn.Set(R::FindDispatchFunctionCached(declaring.Get(),name));ParamFrame f(fn.Get());
    if(!f.valid() || (fuel && !f.Set<float>(L"fuel",*fuel)) || !valid() || !fn.Get())return false;
    if(attempted)*attempted=true;
    if(!Call(a,f))return false;
    return world==world_identity::Generation() && (!current || current()) && (mayDestroy || valid());
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
bool IsGasoline(void* a) {
    if(!a || !R::IsLive(a))return false;CachedObjRef actor,cls;actor.Set(a);cls.Set(object_index::ClassByName(L"grime_gasoline_C"));
    return actor.Get()==a && cls.Get() && R::ClassOf(a)==cls.Raw() &&
        world_identity::CurrentWorld() && world_identity::WorldOf(a)==world_identity::CurrentWorld() &&
        !R::NameStartsWith(R::NameOf(a),L"Default__");
}
namespace { bool FuelActor(void* a){return IsProp(a)||IsGasoline(a);} }
void* Owner(void* c) {
    void* a=nullptr;
    return c&&R::IsLive(c)&&R::ClassNameOf(c)==L"comp_flammable_C"&&
        ReadField(c,L"owner",a)&&FuelActor(a)&&Flame(a)==c?a:nullptr;
}
bool Read(void* a,State& out) {
    if(!FuelActor(a))return false;CachedObjRef actor,cls;actor.Set(a);cls.Set(R::ClassOf(a));const auto world=world_identity::Generation();
    const auto valid=[&]{return actor.Get()==a && cls.Get() && R::ClassOf(a)==cls.Raw() && world==world_identity::Generation();};
    State s{};void* c=nullptr;if(!TryFlame(a,c) || !valid())return false;
    if(!c){out=s;return true;}CachedObjRef component;component.Set(c);
    if(Owner(c)!=a||!Bit(c,L"isBurning",s.burning))return false;
    if(s.burning) {
        if(!ReadField(c,L"fuel",s.fuel)||!ReadField(c,L"fuelSpeed",s.fuelSpeed))return false;
        // Native expiry can briefly leave a negative fuel before destroying the component.
        if(std::isfinite(s.fuel)&&s.fuel<=0){s.burning=false;s.fuel=0;}
    }
    if(!Valid(s) || !valid() || component.Get()!=c || Flame(a)!=c || Owner(c)!=a || !valid())return false;out=s;return true;
}
void CollectBurning(std::vector<CachedObjRef>& actors) {
    actors.clear();void* cls=object_index::ClassByName(L"comp_flammable_C");if(!cls)return;
    object_index::ForEachInstance(cls,[](void* ctx,void* c,int32_t){
        void* a=Owner(c);State s{};if(!a||!Read(a,s)||!s.burning)return;
        auto& rows=*static_cast<std::vector<CachedObjRef>*>(ctx);
        if(rows.size()<512){CachedObjRef ref;ref.Set(a);rows.push_back(ref);}
    },&actors);
}
bool Ignite(void* a,float fuel,bool* attempted,const std::function<bool()>& current) {
    if(attempted)*attempted=false;
    return FuelActor(a)&&std::isfinite(fuel)&&fuel>0&&fuel<=60&&Verb(a,L"ignite",&fuel,nullptr,current,false,attempted);
}
bool Extinguish(void* a,bool* attempted,const std::function<bool()>& current) {
    if(attempted)*attempted=false;
    if(!FuelActor(a))return false;void* c=nullptr;if(!TryFlame(a,c))return false;
    return (!c && (!current || current()))||Verb(c,L"extinguish",nullptr,nullptr,current,true,attempted);
}
bool Apply(void* a,const State& state,const std::function<bool()>& current) {
    if(!Valid(state)||!FuelActor(a)||!current())return false;
    CachedObjRef actor;actor.Set(a);const auto world=world_identity::Generation();
    const auto valid=[&]{return actor.Is(a)&&world==world_identity::Generation()&&FuelActor(a)&&current();};
    if(!state.burning){void* c=nullptr;if(!TryFlame(a,c))return false;return valid() && (!c || Verb(c,L"extinguish",nullptr,nullptr,valid,true)) && valid();}
    State old{};if(!Read(a,old))return false;
    if(!old.burning) {
        // Props bypass subclass effects; gasoline needs its exact native Box,
        // fuelSpeed and igniteScale setup rather than an unrelated prop body.
        void* base=IsGasoline(a)?R::ClassOf(a):object_index::ClassByName(L"prop_C");
        if(!base||!Verb(a,L"ignite",&state.fuel,base,valid)||!valid())return false;
    }
    void* c=Flame(a);if(!c||Owner(c)!=a)return false;
    CachedObjRef component,cls;component.Set(c);cls.Set(R::ClassOf(c));
    int fuel=R::FindPropertyOffset(R::ClassOf(c),L"fuel");
    int speed=R::FindPropertyOffset(R::ClassOf(c),L"fuelSpeed");
    if(fuel<0||speed<0 || !valid() || component.Get()!=c || !cls.Get() || R::ClassOf(c)!=cls.Raw() || Flame(a)!=c || Owner(c)!=a)return false;
    std::memcpy(static_cast<uint8_t*>(c)+fuel,&state.fuel,sizeof(float));
    std::memcpy(static_cast<uint8_t*>(c)+speed,&state.fuelSpeed,sizeof(float));
    State after{};return valid()&&Read(a,after)&&after.burning;
}
}
