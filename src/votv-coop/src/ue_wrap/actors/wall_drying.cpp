#include "ue_wrap/actors/wall_drying.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::wall_drying {
namespace {
namespace R = reflection;
bool Offsets(void* actor, int& elapsed, int& duration) {
    static R::InstanceOffset dryTime(L"dryTime"), timeToDry(L"timeToDry");
    elapsed = dryTime.Of(actor); duration = timeToDry.Of(actor);
    return elapsed >= 0 && duration >= 0 && elapsed != duration;
}
bool Read(void* actor, float& elapsed, float& duration) {
    int e = -1, d = -1;
    if (!Offsets(actor,e,d)) return false;
    const auto* data = static_cast<const uint8_t*>(actor);
    std::memcpy(&elapsed,data+e,sizeof(float)); std::memcpy(&duration,data+d,sizeof(float));
    return std::isfinite(elapsed) && elapsed >= 0 && elapsed <= 1'000'000.f &&
        std::isfinite(duration) && duration > 0 && duration <= 1'000'000.f;
}
}
bool IsWetWall(void* actor) {
    return actor && R::IsLive(actor) && R::ClassNameOf(actor) == L"customWall_wetConcrete_C";
}
bool Capture(void* actor, save_record::SaveRecord& record) {
    if (!IsWetWall(actor) || record.className != L"customWall_wetConcrete_C" ||
        record.floats.size() != 1 || !Valid(record.floats[0])) return false;
    CachedObjRef ref; ref.Set(actor); const auto world = world_identity::Generation();
    float elapsed = 0, duration = 0;
    if (!Read(actor,elapsed,duration) || !ref.Get() || world != world_identity::Generation()) return false;
    auto fields = record.floats[0]; fields.resize(4); fields[2] = elapsed; fields[3] = duration;
    record.floats[0] = std::move(fields);
    return true;
}
bool Restore(void* actor, const save_record::SaveRecord& record) {
    if (!IsWetWall(actor) || record.className != L"customWall_wetConcrete_C" ||
        record.floats.size() != 1 || !Valid(record.floats[0])) return false;
    if (record.floats[0].size() == 2) return true; // old native save: keep BeginPlay's defaults
    CachedObjRef ref; ref.Set(actor); const auto world = world_identity::Generation();
    int elapsed = -1, duration = -1;
    if (!Offsets(actor,elapsed,duration) || !ref.Get() || world != world_identity::Generation()) return false;
    auto* data = static_cast<uint8_t*>(actor);
    std::memcpy(data+elapsed,&record.floats[0][2],sizeof(float));
    std::memcpy(data+duration,&record.floats[0][3],sizeof(float));
    return true; // no tick/replacement on a replica or during save loading
}
bool SaveNative(void* actor, void* nativeRecord) {
    save_record::SaveRecord record; record.className = L"customWall_wetConcrete_C"; record.floats.resize(1);
    return save_record::ReadFloatGroup(nativeRecord,0,record.floats[0]) && Capture(actor,record) &&
        save_record::ReplaceFloatGroup(nativeRecord,0,record.floats[0]);
}
bool LoadNative(void* actor, const void* nativeRecord) {
    save_record::SaveRecord record; record.className = L"customWall_wetConcrete_C"; record.floats.resize(1);
    return save_record::ReadFloatGroup(nativeRecord,0,record.floats[0]) && Restore(actor,record);
}
}
