#pragma once

#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/engine/world_identity.h"
#include <utility>

namespace coop::items {
// Native getData reads the live charge/portions, while saveSlot.hold can still contain
// the values from equip. Commit only a matching, still-live item after dispatch.
inline bool CaptureHeldRecord(void* actor, ue_wrap::save_record::SaveRecord& record) {
    if (record.className.empty()) return true;
    if (!actor || record.key.empty() || record.key == L"None") return false;
    ue_wrap::CachedObjRef identity;
    identity.Set(actor);
    const auto world = ue_wrap::world_identity::Generation();
    ue_wrap::save_record::SaveRecord current;
    if (!ue_wrap::save_record::CaptureRecord(actor, current) || identity.Get() != actor ||
        world != ue_wrap::world_identity::Generation() || current.key != record.key ||
        current.className != record.className) return false;
    // The hand's display transform is not the inventory item's stored transform.
    current.xform = record.xform;
    record = std::move(current);
    return true;
}
}
