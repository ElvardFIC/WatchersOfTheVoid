#pragma once

#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include <cstring>
#include <cwchar>

namespace coop::inventory_pickup_sync::detail {
enum class ExtractRestore { Unrelated, Restored, Failed };

// Native prop_container::extract removes the saved row, creates a temporary actor, then calls
// putObjectInventory2 BEFORE loadData. Restore that exact carrier before getData captures it.
// No parked global record: caller locals belong to this synchronous extraction only.
inline ExtractRestore RestoreContainerCarrier(const ue_wrap::script_gate::Call& call, void* actor) {
    namespace R = ue_wrap::reflection;
    namespace SR = ue_wrap::save_record;
    if (!call.callerFunction || !call.callerLocals || !call.callerObject || !actor)
        return ExtractRestore::Unrelated;
    if (::_wcsicmp(R::ToString(R::NameOf(call.callerFunction)).c_str(), L"extract") != 0)
        return ExtractRestore::Unrelated;
    void* owner = R::OuterOf(call.callerFunction);
    if (!owner || ::_wcsicmp(R::ToString(R::NameOf(owner)).c_str(), L"prop_container_C") != 0)
        return ExtractRestore::Unrelated;

    static ue_wrap::CachedObjRef fn;
    static int32_t recordOff = -1, actorOff = -1;
    if (!fn.Is(call.callerFunction)) {
        fn.Set(call.callerFunction);
        recordOff = actorOff = -1;
        const int32_t frame = R::FunctionFrameSize(call.callerFunction);
        for (const auto& field : R::EnumerateStructFields(call.callerFunction)) {
            if (field.offset < 0 || field.size < 0 || field.offset > frame || field.size > frame - field.offset)
                continue;
            if (::_wcsicmp(field.name.c_str(), L"CallFunc_takeObj_Output") == 0 && field.size >= SR::kSaveRecordBytes)
                recordOff = field.offset;
            if (::_wcsicmp(field.name.c_str(), L"CallFunc_FinishSpawningActor_ReturnValue") == 0 && field.size == sizeof(void*))
                actorOff = field.offset;
        }
    }
    if (recordOff < 0 || actorOff < 0) return ExtractRestore::Failed;
    void* carrier = nullptr;
    std::memcpy(&carrier, call.callerLocals + actorOff, sizeof(carrier));
    if (carrier != actor) return ExtractRestore::Unrelated;

    SR::SaveRecord record;
    SR::ReadSaveRecord(call.callerLocals + recordOff, record);
    if (record.className.empty() || record.className != R::ClassNameOf(actor) || record.key.empty())
        return ExtractRestore::Failed;
    void* load = R::FindDispatchFunctionCached(R::ClassOf(actor), L"loadData");
    // Use the full native record: the temporary carrier needs the ORIGINAL key/name as well as
    // its charge/uses. ApplyRecord's network splice intentionally preserves the receiver's key.
    return load && SR::ApplyRecordVia(actor, load, record) ? ExtractRestore::Restored : ExtractRestore::Failed;
}
} // namespace coop::inventory_pickup_sync::detail
