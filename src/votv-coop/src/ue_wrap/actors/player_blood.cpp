#include "ue_wrap/actors/player_blood.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/world/world_singleton.h"
#include <cmath>
#include <cstring>

namespace ue_wrap::player_blood {
namespace R = reflection;
bool Valid(const Drop& drop) {
    for (float v : {drop.location.X, drop.location.Y, drop.location.Z})
        if (!std::isfinite(v) || std::fabs(v) > 1.e7f) return false;
    for (float v : {drop.velocity.X, drop.velocity.Y, drop.velocity.Z})
        if (!std::isfinite(v) || std::fabs(v) > 25.01f) return false;
    return std::isfinite(drop.size) && drop.size >= .25f && drop.size <= 1.5f;
}
bool ReadDrop(const script_gate::Call& call, Drop& drop) {
    if (call.fromOurCode || !call.locals || !call.object || !call.callerObject ||
        !R::IsLive(call.object) || !R::IsLive(call.callerObject) ||
        R::ClassNameOf(call.object) != L"bloodDecalMaster_C" ||
        R::ClassNameOf(call.callerObject) != L"effect_bloodLoss_C" ||
        !world_identity::CurrentWorld() ||
        world_identity::WorldOf(call.object) != world_identity::CurrentWorld() ||
        world_identity::WorldOf(call.callerObject) != world_identity::CurrentWorld()) return false;
    const int bytes = R::FunctionFrameSize(call.function);
    const auto read = [&](const wchar_t* name, auto& out) {
        const int off = R::FindParamOffset(call.function, name);
        if (off < 0 || bytes < int(sizeof(out)) || off > bytes - int(sizeof(out)) ||
            R::FindParamSize(call.function, name) != int(sizeof(out))) return false;
        std::memcpy(&out, call.locals + off, sizeof(out)); return true;
    };
    Drop value{}; int32_t type = -1;
    if (!read(L"location", value.location) || !read(L"velocity", value.velocity) ||
        !read(L"size", value.size) || !read(L"type", type) || type != 0 || !Valid(value)) return false;
    drop = value; return true;
}
bool Replay(const Drop& drop, const std::function<bool()>& current) {
    if (!Valid(drop) || !current || !current()) return false;
    void* manager = world_singleton::Find(L"bloodDecalMaster_C");
    if (!manager || !current()) return false;
    CachedObjRef ref; ref.Set(manager);
    CachedObjRef fn; fn.Set(R::FindFunction(R::ClassOf(manager), L"addParticle"));
    ParamFrame frame(fn.Get());
    if (!frame.valid() || !frame.Set(L"location", drop.location) ||
        !frame.Set(L"velocity", drop.velocity) || !frame.Set(L"size", drop.size) ||
        !frame.Set<int32_t>(L"type", 0) || !ref.Get() || !fn.Get() || !current()) return false;
    // addParticle creates native physical drops and their splat/sound/stain; it
    // never runs the local blood-loss actor's damage or viewport-widget path.
    Call(manager, frame);
    return true;
}
}
