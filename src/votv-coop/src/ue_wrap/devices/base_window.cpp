#include "ue_wrap/devices/base_window.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::base_window {
namespace {
namespace R=reflection;
struct Layout {CachedObjRef cls;int clean=-1,key=-1,mesh=-1;} g_layout;
template<class T>T Read(void* object,int offset){T value{};std::memcpy(&value,static_cast<uint8_t*>(object)+offset,sizeof(value));return value;}
struct Identity {
    CachedObjRef actor;uint32_t world=world_identity::Generation();
    explicit Identity(void* object){actor.Set(object);}
    bool Current()const{return actor.Get() && world==world_identity::Generation();}
};
}
bool EnsureResolved(){
    void* cls=object_index::ClassByName(L"baseWindow_C");if(!cls){g_layout={};return false;}
    if(g_layout.cls.Is(cls) && g_layout.clean>=0 && g_layout.key>=0 && g_layout.mesh>=0)return true;
    Layout fields;fields.cls.Set(cls);
    fields.clean=R::FindPropertyOffset(cls,L"clean");fields.key=R::FindPropertyOffset(cls,L"Key");
    fields.mesh=R::FindPropertyOffset(cls,L"StaticMesh1");
    if(!fields.cls.Get())return false;g_layout=fields;return fields.clean>=0 && fields.key>=0;
}
bool IsBaseWindow(void* actor){
    if(!actor || !R::IsLive(actor))return false;Identity identity(actor);
    if(!EnsureResolved() || !identity.Current())return false;
    void* base=g_layout.cls.Get();return base && R::IsDescendantOfAny(R::ClassOf(actor),&base,1) && identity.Current();
}
std::wstring GetKeyString(void* actor){
    if(!actor || !R::IsLive(actor))return {};Identity identity(actor);
    if(!IsBaseWindow(actor) || !identity.Current())return {};
    const auto fields=g_layout;const auto name=Read<R::FName>(actor,fields.key);const auto key=R::ToString(name);
    return identity.Current() && fields.cls.Get()?key:std::wstring();
}
bool ReadClean(void* actor,float& out){
    if(!actor || !R::IsLive(actor))return false;Identity identity(actor);
    if(!IsBaseWindow(actor) || !identity.Current())return false;
    const float value=Read<float>(actor,g_layout.clean);if(!std::isfinite(value))return false;out=value;return true;
}
bool WriteCleanAndApply(void* actor,float clean,const std::function<bool()>& current){
    if(!actor || !R::IsLive(actor) || !std::isfinite(clean) || clean<0 || (current && !current()))return false;
    Identity identity(actor);if(!IsBaseWindow(actor) || !identity.Current())return false;
    const auto fields=g_layout;void* cls=R::ClassOf(actor);
    const auto stable=[&]{return identity.Current() && fields.cls.Get() && R::ClassOf(actor)==cls;};
    const auto valid=[&]{return stable() && (!current || current()) && stable();};
    if(fields.mesh<0 || !valid())return false;
    void* mesh=Read<void*>(actor,fields.mesh);if(!mesh || !R::IsLive(mesh))return false;
    CachedObjRef component,function;component.Set(mesh);function.Set(R::FindDispatchFunctionCached(cls,L"setClean"));
    const auto bound=[&]{return valid() && component.Get()==mesh && Read<void*>(actor,fields.mesh)==mesh && function.Get();};
    if(!bound())return false;ParamFrame frame(function.Get());if(!frame.valid() || !bound())return false;
    std::memcpy(static_cast<uint8_t*>(actor)+fields.clean,&clean,sizeof(clean));
    // setClean has no inputs: it reads clean and repaints StaticMesh1. A partial
    // native attempt returns false so an unchanged scalar still retries its paint.
    const bool called=Call(actor,frame);
    return bound() && Read<float>(actor,fields.clean)==clean && called;
}
}
