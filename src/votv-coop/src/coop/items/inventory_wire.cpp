// coop/items/inventory_wire.cpp -- see coop/items/inventory_wire.h.
//
// The per-record grammar and the byte primitives live in coop/items/save_record_wire, shared
// with coop/props/container_contents_sync; what stays here is the
// player-scoped envelope: the version byte, the three arrays and Fstruct_equipment, then the
// vitals and the pose.

#include "coop/items/inventory_wire.h"

#include "coop/items/save_record_wire.h"

#include <cstddef>

namespace coop::inventory_wire {
namespace {

namespace W = coop::save_record_wire;

using coop::player_profile::Profile;
using ue_wrap::inventory::EquipRecord;
using ue_wrap::vitals::Snapshot;

// The vitals' scalar order on the wire. Appending is a layout change (bump kVersion).
constexpr float Snapshot::* kVitalScalars[] = {
    &Snapshot::health, &Snapshot::maxHealth, &Snapshot::food, &Snapshot::sleep, &Snapshot::battery,
    &Snapshot::coffeePower, &Snapshot::gasolinepilled, &Snapshot::strength, &Snapshot::agility,
};
constexpr uint32_t kMaxFoods = 4096;  // distinct foods a tolerance list may name

void SerEquip(std::vector<uint8_t>& b, const EquipRecord& r) {
    W::AppWStr(b, r.propName);
    W::AppWStr(b, r.propKey);
    W::SerSave(b, r.data);
    W::AppWStr(b, r.tag);
}
bool DeEquip(const std::vector<uint8_t>& b, size_t& o, EquipRecord& r) {
    if (!W::RdWStr(b, o, r.propName)) return false;
    if (!W::RdWStr(b, o, r.propKey)) return false;
    if (!W::DeSave(b, o, r.data)) return false;
    return W::RdWStr(b, o, r.tag);
}

}  // namespace

std::vector<uint8_t> SerializeItems(const ue_wrap::inventory::PlayerInventory& inv) {
    std::vector<uint8_t> b;
    b.push_back(kVersion);
    W::AppU32(b, static_cast<uint32_t>(inv.inventory.size()));
    for (const auto& r : inv.inventory) W::SerSave(b, r);
    W::AppU32(b, static_cast<uint32_t>(inv.equipment.size()));
    for (const auto& r : inv.equipment) SerEquip(b, r);
    W::AppU32(b, static_cast<uint32_t>(inv.hold.size()));
    for (const auto& r : inv.hold) SerEquip(b, r);
    return b;
}

void AppendState(std::vector<uint8_t>& b, const Snapshot* v, const coop::player_profile::Pose& pose) {
    W::AppU8(b, v ? 1 : 0);
    if (v) {
        const Snapshot& vitals = *v;
        for (auto m : kVitalScalars) W::AppF32(b, vitals.*m);
        W::AppWStr(b, vitals.flashlightBattery);
        const size_t foods = vitals.foodConsumed.size() < vitals.foodTolerance.size()
                                 ? vitals.foodConsumed.size() : vitals.foodTolerance.size();
        W::AppU32(b, static_cast<uint32_t>(foods));
        for (size_t i = 0; i < foods; ++i) {
            W::AppWStr(b, vitals.foodConsumed[i]);
            W::AppF32(b, vitals.foodTolerance[i]);
        }
    }
    W::AppU8(b, pose.valid ? 1 : 0);
    W::AppF32(b, pose.x); W::AppF32(b, pose.y); W::AppF32(b, pose.z); W::AppF32(b, pose.yaw);
}

std::vector<uint8_t> Serialize(const Profile& p) {
    std::vector<uint8_t> b = SerializeItems(p.items);
    AppendState(b, p.hasVitals ? &p.vitals : nullptr, p.pose);
    return b;
}

bool Deserialize(const std::vector<uint8_t>& b, Profile& p, uint8_t* outVersion) {
    p = Profile{};
    auto& out = p.items;
    size_t o = 0;
    uint8_t ver; if (!W::RdU8(b, o, ver)) return false;
    if (ver != kVersion && !(outVersion && ver == kVersionProjection)) return false;
    if (outVersion) *outVersion = ver;
    uint32_t n;
    if (!W::RdU32(b, o, n) || n > W::kMaxRecords || !W::Feasible(n, b, o)) return false;
    out.inventory.resize(n);
    for (auto& r : out.inventory) if (!W::DeSave(b, o, r)) return false;
    if (!W::RdU32(b, o, n) || n > W::kMaxRecords || !W::Feasible(n, b, o)) return false;
    out.equipment.resize(n);
    for (auto& r : out.equipment) if (!DeEquip(b, o, r)) return false;
    if (!W::RdU32(b, o, n) || n > W::kMaxRecords || !W::Feasible(n, b, o)) return false;
    out.hold.resize(n);
    for (auto& r : out.hold) if (!DeEquip(b, o, r)) return false;
    if (ver == kVersionProjection) return o == b.size();  // three arrays and nothing after them

    uint8_t hasVitals = 0;
    if (!W::RdU8(b, o, hasVitals) || hasVitals > 1) return false;
    p.hasVitals = hasVitals != 0;
    if (p.hasVitals) {
        for (auto m : kVitalScalars) if (!W::RdF32(b, o, p.vitals.*m)) return false;
        if (!W::RdWStr(b, o, p.vitals.flashlightBattery)) return false;
        if (!W::RdU32(b, o, n) || n > kMaxFoods || !W::Feasible(n, b, o)) return false;
        p.vitals.foodConsumed.resize(n);
        p.vitals.foodTolerance.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            if (!W::RdWStr(b, o, p.vitals.foodConsumed[i])) return false;
            if (!W::RdF32(b, o, p.vitals.foodTolerance[i])) return false;
        }
    }
    uint8_t valid = 0;
    if (!W::RdU8(b, o, valid) || valid > 1) return false;
    p.pose.valid = valid != 0;
    return W::RdF32(b, o, p.pose.x) && W::RdF32(b, o, p.pose.y) && W::RdF32(b, o, p.pose.z) &&
           W::RdF32(b, o, p.pose.yaw) && o == b.size();  // nothing may follow the pose
}

}  // namespace coop::inventory_wire
