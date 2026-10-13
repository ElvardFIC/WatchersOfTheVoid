#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>

namespace ue_wrap::wall_build {
std::wstring Key(void* wall) {
    if (!placed_save::IsWall(placed_save::Type(wall))) return {};
    static reflection::InstanceOffset field(L"key");
    const int off = field.Of(wall);
    return off < 0 ? std::wstring{} : save_record::ReadFNameAt(wall, off);
}
bool Damage(void* wall, float amount) {
    if (placed_save::Type(wall) != placed_save::Kind::Wall || !std::isfinite(amount) || amount <= 0) return false;
    CachedObjRef live; live.Set(wall);
    const auto world = world_identity::Generation();
    FVector position{};
    ParamFrame f(reflection::FindDispatchFunctionCached(reflection::ClassOf(wall), L"damage"));
    if (!engine::TryGetActorLocation(wall, position) || !f.Set<float>(L"damage", amount) ||
        !f.Set<FVector>(L"Location", position) || !f.Set<FRotator>(L"Rotation", {}) ||
        !live.Get() || world != world_identity::Generation()) return false;
    return Call(wall, f); // destruction is a successful native outcome
}
bool RestoreMaterial(void* wall, const wchar_t* name) {
    if (placed_save::Type(wall) != placed_save::Kind::WallOutline) return false;
    if (!name || !*name) return true; // no material selected yet
    CachedObjRef ref; ref.Set(wall);
    const auto row = fname_utils::StringToFName(name);
    if (!ref.Get() || !row.ComparisonIndex) return false;
    ParamFrame f(reflection::FindDispatchFunctionCached(reflection::ClassOf(wall), L"findMat"));
    bool ok = false;
    return f.Set<reflection::FName>(L"B", row) && Call(wall, f) && ref.Get() &&
        f.GetRaw(L"return", &ok, sizeof(ok)) && ok;
}
bool Feed(void* wall, void* material) {
    namespace R = reflection;
    if (placed_save::Type(wall) != placed_save::Kind::WallOutline || !material || !R::IsLive(material)) return true;
    CachedObjRef wallRef, materialRef; wallRef.Set(wall); materialRef.Set(material);
    static R::InstanceOffset overlapField(L"overlap");
    const int32_t off = overlapField.Of(wall);
    if (off < 0) return false;
    void* overlap = *reinterpret_cast<void**>(static_cast<uint8_t*>(wall) + off);
    if (!overlap || !R::IsLive(overlap)) return false;
    ParamFrame check(R::FindDispatchFunctionCached(R::ClassOf(overlap), L"IsOverlappingActor"));
    if (!check.Set<void*>(L"Other", material) || !Call(overlap, check)) return false;
    bool touches = false;
    if (!check.GetRaw(L"ReturnValue", &touches, sizeof(touches))) return false;
    if (!wallRef.Get() || !materialRef.Get() || !touches) return true;
    ParamFrame feed(R::FindDispatchFunctionCached(R::ClassOf(wall), kOverlap));
    // Native graph reads only OtherActor. Remaining event arguments are zeroed;
    // no invented hit result or client-supplied material value is used.
    if (!feed.Set<void*>(L"OtherActor", material)) return false;
    return Call(wall, feed);
}
}
