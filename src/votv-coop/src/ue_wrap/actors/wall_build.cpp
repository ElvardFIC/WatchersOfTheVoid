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
    return DamageWhile(wall,amount,[]{return true;});
}
bool DamageWhile(void* wall,float amount,const std::function<bool()>& callerCurrent) {
    if(!callerCurrent || !callerCurrent())return false;
    if (placed_save::Type(wall) != placed_save::Kind::Wall || !std::isfinite(amount) || amount <= 0) return false;
    namespace R=reflection;
    CachedObjRef live,type,function,owner;live.Set(wall);type.Set(R::ClassOf(wall));
    const auto world = world_identity::Generation();
    const auto current=[&]{return callerCurrent() && world==world_identity::Generation() && live.Get() && type.Get() &&
        R::ClassOf(wall)==type.Raw() && placed_save::Type(wall)==placed_save::Kind::Wall;};
    if(!current())return false;
    void* fn=R::FindDispatchFunctionCached(type.Raw(),L"damage");function.Set(fn);owner.Set(fn?R::OuterOf(fn):nullptr);
    const auto prepared=[&]{return current() && function.Get() && owner.Get() && R::OuterOf(fn)==owner.Raw() &&
        R::FindDispatchFunctionCached(type.Raw(),L"damage")==fn;};
    if(!prepared())return false;
    FVector position{};
    ParamFrame f(fn);
    if (!prepared() || !f.valid() || !engine::TryGetActorLocation(wall, position) || !prepared() ||
        !std::isfinite(position.X) || !std::isfinite(position.Y) || !std::isfinite(position.Z) ||
        !f.Set<float>(L"damage", amount) || !prepared() || !f.Set<FVector>(L"Location", position) || !prepared() ||
        !f.Set<FRotator>(L"Rotation", {}) || !prepared())return false;
    return Call(wall, f); // destruction is a successful native outcome
}
bool RestoreMaterial(void* wall, const wchar_t* name) {
    if (placed_save::Type(wall) != placed_save::Kind::WallOutline) return false;
    if (!name || !*name) return true; // no material selected yet
    CachedObjRef ref; ref.Set(wall);
    const auto world = world_identity::Generation();
    const auto row = fname_utils::StringToFName(name);
    if (!ref.Get() || world != world_identity::Generation() || !row.ComparisonIndex) return false;
    ParamFrame f(reflection::FindDispatchFunctionCached(reflection::ClassOf(wall), L"findMat"));
    bool ok = false;
    return f.Set<reflection::FName>(L"B", row) && Call(wall, f) && ref.Get() && world == world_identity::Generation() &&
        f.GetRaw(L"return", &ok, sizeof(ok)) && ok;
}
bool Feed(void* wall, void* material) {
    return FeedWhile(wall,material,[]{return true;});
}
bool FeedWhile(void* wall, void* material,const std::function<bool()>& callerCurrent) {
    namespace R = reflection;
    if(!callerCurrent || !callerCurrent())return false;
    if (placed_save::Type(wall) != placed_save::Kind::WallOutline || !material || !R::IsLive(material)) return true;
    CachedObjRef wallRef, materialRef; wallRef.Set(wall); materialRef.Set(material);
    const auto world = world_identity::Generation();
    CachedObjRef wallType,materialType;wallType.Set(R::ClassOf(wall));materialType.Set(R::ClassOf(material));
    const auto current=[&]{return callerCurrent() && world==world_identity::Generation();};
    const auto types=[&]{return wallType.Get() && materialType.Get() && R::ClassOf(wall)==wallType.Raw() && R::ClassOf(material)==materialType.Raw();};
    static R::InstanceOffset overlapField(L"overlap");
    const int32_t off = overlapField.Of(wall);
    if (off < 0 || !current() || !wallRef.Get() || !materialRef.Get() || !types()) return false;
    void* overlap = *reinterpret_cast<void**>(static_cast<uint8_t*>(wall) + off);
    if (!overlap || !R::IsLive(overlap)) return false;
    CachedObjRef overlapRef,overlapType,queryFn;overlapRef.Set(overlap);overlapType.Set(R::ClassOf(overlap));
    queryFn.Set(R::FindDispatchFunctionCached(R::ClassOf(overlap),L"IsOverlappingActor"));
    const auto componentCurrent=[&]{return overlapRef.Get() && overlapType.Get() && queryFn.Get() &&
        R::ClassOf(overlap)==overlapType.Raw() && overlapField.Of(wall)==off &&
        *reinterpret_cast<void**>(static_cast<uint8_t*>(wall)+off)==overlap;};
    ParamFrame check(queryFn.Get());
    if (!current() || !wallRef.Get() || !materialRef.Get() || !types() || !componentCurrent() ||
        !check.Set<void*>(L"Other", material) || !current() || !wallRef.Get() || !materialRef.Get() || !types() || !componentCurrent() || !Call(overlap, check)) return false;
    if(!current())return false;
    bool touches = false;
    if (!check.GetRaw(L"ReturnValue", &touches, sizeof(touches))) return false;
    if (!wallRef.Get() || !materialRef.Get() || world != world_identity::Generation() || !touches ||
        placed_save::Type(wall) != placed_save::Kind::WallOutline) return true;
    // A component replaced during the query must be tested again on a later
    // attempt; its predecessor's collision result does not authorize spending.
    if (!types() || !componentCurrent()) return false;
    CachedObjRef feedFn;feedFn.Set(R::FindDispatchFunctionCached(R::ClassOf(wall),kOverlap));
    ParamFrame feed(feedFn.Get());
    // Native graph reads only OtherActor. Remaining event arguments are zeroed;
    // no invented hit result or client-supplied material value is used.
    if (!current() || !wallRef.Get() || !materialRef.Get() || !types() || !feedFn.Get() ||
        !componentCurrent() || !feed.Set<void*>(L"OtherActor", material) || !current() ||
        !wallRef.Get() || !materialRef.Get() || !types() || !componentCurrent() || !feedFn.Get()) return false;
    return Call(wall, feed);
}
}
