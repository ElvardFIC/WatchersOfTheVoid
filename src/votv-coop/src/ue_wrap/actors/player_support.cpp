#include "ue_wrap/actors/player_support.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/log.h"
#include <cstring>
#include <cwchar>

namespace ue_wrap::player_support {
void* Read(void* player) {
    namespace R = reflection;
    if (!player || !R::IsLive(player)) return nullptr;
    static CachedObjRef cls;
    static int32_t basedOff = -1, componentOff = -1;
    void* c = R::ClassOf(player);
    if (cls.Get() != c) {
        cls.Set(c); componentOff = -1;
        basedOff = R::FindPropertyOffset(c, L"BasedMovement");
        void* layout = R::PropertyInnerStruct(c, L"BasedMovement");
        if (layout) {
            for (const auto& field : R::EnumerateStructFields(layout))
                if (::_wcsicmp(field.name.c_str(), L"MovementBase") == 0 && field.size == sizeof(void*) &&
                    field.offset >= 0 && field.offset + field.size <= R::StructSize(layout))
                    componentOff = field.offset;
        }
        if (basedOff < 0 || componentOff < 0)
            UE_LOGW("[PROP-SUPPORT] Character.BasedMovement.MovementBase did not resolve");
    }
    if (basedOff < 0 || componentOff < 0) return nullptr;
    void* component = nullptr;
    std::memcpy(&component, static_cast<uint8_t*>(player)+basedOff+componentOff, sizeof(component));
    if (!component || !R::IsLive(component)) return nullptr;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(component), L"GetOwner");
    if (!fn) return nullptr;
    ParamFrame f(fn);
    if (!f.valid() || !Call(component, f)) return nullptr;
    void* owner = f.Get<void*>(L"ReturnValue");
    return owner && R::IsLive(owner) ? owner : nullptr;
}
}
