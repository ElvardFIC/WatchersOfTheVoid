#include "ue_wrap/actors/wall_creative.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/object_index.h"
namespace ue_wrap::wall_creative {
namespace R=reflection;
namespace {
void* Library() {
    static CachedObjRef library;if(library.Get())return library.Get();
    void* cls=object_index::ClassByName(L"lib_C");if(!cls)return nullptr;
    object_index::ForEachInstance(cls,[](void* context,void* object,int32_t){
        if(R::NameEquals(R::NameOf(object),L"Default__lib_C"))static_cast<CachedObjRef*>(context)->Set(object);
    },&library);
    return library.Get();
}
}
bool IsBuilderCall(void* tool,void* function) {
    if(!tool || !function || !R::IsLive(tool) || !R::IsLive(function) ||
       !R::NameEquals(R::NameOf(function),L"addWall"))return false;
    void* owner=R::OuterOf(function);
    return owner && R::IsLive(owner) && R::NameEquals(R::NameOf(owner),L"prop_wallbuilder_C") &&
        R::IsDescendantOfAny(R::ClassOf(tool),&owner,1);
}
bool Allowed(void* context,bool& allowed) {
    if(!context || !R::IsLive(context))return false;
    CachedObjRef live;live.Set(context);const auto world=world_identity::Generation();
    void* library=Library();if(!library)return false;
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(library),L"isCustom"));
    bool value=false;
    if(!frame.valid() || !frame.Set<void*>(L"__WorldContext",context) || !live.Get() ||
       world!=world_identity::Generation() || !Call(library,frame) || !live.Get() ||
       world!=world_identity::Generation() || !frame.GetRaw(L"isCustom",&value,sizeof(value)))return false;
    allowed=value;return true;
}
Build Finish(void* wall,const std::function<bool()>& current) {
    if(!current() || placed_save::Type(wall)!=placed_save::Kind::WallOutline)return Build::Retry;
    CachedObjRef live;live.Set(wall);const auto world=world_identity::Generation();
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(wall),L"buildWall"));
    if(!frame.valid() || !frame.Set<bool>(L"isCheat",true) || !current() || !live.Get() ||
       world!=world_identity::Generation())return Build::Retry;
    // Native completion creates/initializes the selected wall and retires this outline.
    // A fault after dispatch is terminal: replay could create a second successor.
    return Call(wall,frame)?Build::Dispatched:Build::Failed;
}
}
