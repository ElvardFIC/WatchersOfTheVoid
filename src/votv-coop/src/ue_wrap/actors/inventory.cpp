// ue_wrap/actors/inventory.cpp -- see ue_wrap/actors/inventory.h.
//
// The Fstruct_save record codec + the TArray primitives live in ue_wrap/actors/save_record, which
// coop/props/container_contents_sync needs for the same walk; what stays here is the
// player-scoped concern: the saveSlot resolve, the three player arrays, and Fstruct_equipment.

#include "ue_wrap/actors/inventory.h"

#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/actors/container_inventory.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/world/world_singleton.h"

#include <cstdint>
#include <cstring>

namespace ue_wrap::inventory {
namespace {

namespace R = ue_wrap::reflection;
namespace SR = ue_wrap::save_record;

// saveSlot.hpp player-scoped arrays. The carried items are not among them: they live in
// GObjStack[kPersonalSlot], resolved by reflection below.
constexpr int32_t kOff_equipment     = 0x0440;  // TArray<Fstruct_equipment>
constexpr int32_t kOff_hold          = 0x0450;  // TArray<Fstruct_equipment>

// Fstruct_equipment FIELD layout (struct_equipment.hpp; raw size 0x118, ARRAY STRIDE 0x120 -- see
// kEquipStride below).
constexpr int32_t kEquip_propName  = 0x00;   // Fstruct_propDynamic.name FName
constexpr int32_t kEquip_propKey   = 0x08;   // Fstruct_propDynamic.key  FName
constexpr int32_t kEquip_data      = 0x10;   // embedded Fstruct_save (in a 0x100 slot)
constexpr int32_t kEquip_tag       = 0x110;  // FName
// 16-ALIGNED stride (raw `Size: 0x118` -> Align(0x118,16) = 0x120). Fstruct_equipment embeds the
// 16-aligned Fstruct_save, so it too is 16-aligned, and the struct that embeds it reports the
// member as 0x120 wide. Same crash class as save_record::kSaveStride if the raw 0x118 is used for
// the equipment/hold arrays with N>=2.
constexpr int32_t kEquipStride     = 0x120;

// The player's GObjStack slot. Baked, not restored: prop_inventoryContainer_player_C's component
// template serializes index=0, and the saveSlot CDO pre-seeds the slot, so it is the one index
// that is known before any container actor exists -- which is when the join apply runs. The live
// reader checks the running component against it.
constexpr int32_t kPersonalSlot    = 0;

R::InstanceOffset g_offSave{L"saveSlot"};

// Class/slot/serial-scoped offsets, including failures against an old class.
R::InstanceOffset g_offPlayerContainer{L"playerContainer"},g_offContainerPropInv{L"propInventory"},
    g_offInvIndex{L"Index"},g_offGObjStack{L"GObjStack"};
uint64_t g_applySerial=0;
uint64_t g_handWriteSerial=0;

// The contents TArray header of GObjStack[kPersonalSlot] on `save` (the struct_mObject element's
// single field, at +0), or null when the save object has no such slot.
uint8_t* PersonalSlotOf(void* save) {
    const auto off=g_offGObjStack.Of(save);if(off<0)return nullptr;
    SR::Arr stack{};if(!SR::TryReadArr(save,off,stack))return nullptr;
    if (kPersonalSlot >= stack.num) return nullptr;
    return const_cast<uint8_t*>(stack.data) + static_cast<size_t>(kPersonalSlot) * SR::kMxStride;
}

template <class T> T ReadAt(const void* base, int32_t off) {
    T v{};
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + off, sizeof(T));
    return v;
}

void ReadEquipRecord(const void* base, EquipRecord& rec) {
    rec.propName = SR::ReadFNameAt(base, kEquip_propName);
    rec.propKey  = SR::ReadFNameAt(base, kEquip_propKey);
    SR::ReadSaveRecord(reinterpret_cast<const uint8_t*>(base) + kEquip_data, rec.data);
    rec.tag      = SR::ReadFNameAt(base, kEquip_tag);
}

bool WriteEquipRecord(uint8_t* base, const EquipRecord& r) {
    SR::WriteFNameField(base + kEquip_propName, r.propName);
    SR::WriteFNameField(base + kEquip_propKey, r.propKey);
    // Repair empty slots in profiles saved by older builds, too. Occupied records (and their
    // transforms) must survive unchanged; only the old all-zero empty-slot transform is wrong.
    auto data = r.data;
    if (r.propName.empty() && r.propKey.empty() && r.tag.empty() && data.className.empty() &&
        data.key.empty() && data.xform == std::array<float, 10>{})
        data.xform = SR::SaveRecord{}.xform;
    const bool encoded=SR::WriteSaveRecordChecked(base + kEquip_data, data);
    SR::WriteFNameField(base + kEquip_tag, r.tag);
    const auto nameReady=[&](int32_t off,const std::wstring& name) {
        R::FName value{};std::memcpy(&value,base+off,sizeof(value));
        return name.empty() || ::_wcsicmp(name.c_str(),L"None")==0 || value.ComparisonIndex!=0;
    };
    return encoded && nameReady(kEquip_propName,r.propName) && nameReady(kEquip_propKey,r.propKey) && nameReady(kEquip_tag,r.tag);
}

// Build one engine TArray<RecordT> from `recs` and overwrite the header at base+off. The
// old buffer is intentionally orphaned (see ApplyToSaveObject's contract).
struct StagedArray {
    void* data=nullptr;int32_t num=0;bool writing=false;
    ~StagedArray(){if(data && !writing)R::EngineFree(data);}
    bool Allocate(size_t count,size_t stride) {
        if(count>SR::kMaxArr)return false;
        num=static_cast<int32_t>(count);data=SR::AllocZeroed(count,stride);
        return !num || data;
    }
    template<class T,class F> bool Encode(const std::vector<T>& records,size_t stride,F write,const std::function<bool()>& current) {
        writing=true;
        for(size_t i=0;i<records.size();++i) {
            if(!current() || !write(static_cast<uint8_t*>(data)+i*stride,records[i]) || !current())return false;
        }
        return current();
    }
    void Publish(void* base,int32_t off) {SR::WriteArrHeader(base,off,data,num);data=nullptr;}
};

// The player's contents array, addressed ONLY through the assertions that prove it is the
// personal store: the container component, its `Player` flag, and its running Index against the
// baked slot. Returns the slot index and fills `contents`, or -1 having read nothing.
//
// Both readers go through it. The count reader used to address GObjStack[0] directly, which is
// the same slot on every build measured -- but "the same slot in practice" is what an address
// assertion exists to stop a reader from relying on, and the count is what the quick-slot edge
// makes its decision on.
int32_t ValidatedPersonalSlot(SR::Arr& contents) {
    void* save = ResolveSaveSlot();
    void* gm = world_singleton::Gamemode();
    if (!save || !gm) return -1;

    const auto containerOff=g_offPlayerContainer.Of(gm);if(containerOff<0)return -1;
    void* pc = ReadAt<void*>(gm, containerOff);
    if (!pc || !SR::PlausibleObjPtr(pc) || !R::IsLive(pc)) return -1;

    const auto inventoryOff=g_offContainerPropInv.Of(pc);if(inventoryOff<0)return -1;
    void* inv = ReadAt<void*>(pc, inventoryOff);
    if (!inv || !SR::PlausibleObjPtr(inv) || !R::IsLive(inv)) return -1;

    // ADDRESS ASSERTION (fail-closed): this must be the PERSONAL store and nothing else. Same flag
    // that container_contents_sync's BOUNDARY 1 refuses on -- opposite sides of one boundary.
    bool personal=false;
    if (!container_inventory::ReadPersonalFlag(inv,personal) || !personal) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            UE_LOGW("inventory: playerContainer.propInventory is NOT flagged personal (Player==0) "
                    "-- refusing to read it as the live personal store. Expected player=True from "
                    "the class's component template; something resolved to the wrong container.");
        }
        return -1;
    }

    const auto indexOff=g_offInvIndex.Of(inv);if(indexOff<0)return -1;
    const int32_t idx = ReadAt<int32_t>(inv, indexOff);
    if (idx < 0) return -1;  // -1 = never initialised
    if (idx != kPersonalSlot) {
        // The join apply wrote kPersonalSlot before this component existed. A different running
        // index means the apply and the game disagree about where the player's items live.
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            UE_LOGE("inventory: the player container runs on GObjStack[%d], not the baked slot %d "
                    "-- refusing foreign-slot contents", idx, kPersonalSlot);
        }
        return -1;
    }

    const auto stackOff=g_offGObjStack.Of(save);if(stackOff<0)return -1;
    SR::Arr stack{};if(!SR::TryReadArr(save,stackOff,stack))return -1;
    if (idx >= stack.num) return -1;

    // The struct_mObject element's single field is the contents TArray<Fstruct_save> at +0.
    if(!SR::TryReadArr(stack.data+static_cast<size_t>(idx)*SR::kMxStride,0,contents))return -1;
    return idx;
}

}  // namespace

void* ResolveSaveSlot() {
    void* gm = world_singleton::Gamemode();
    if (!gm) return nullptr;
    const auto off=g_offSave.Of(gm);if(off<0)return nullptr;
    void* save = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gm) + off);
    return (save && R::IsLive(save)) ? save : nullptr;
}

std::wstring ReadHeldItemName() {
    void* save = ResolveSaveSlot();
    if (!save) return {};
    const SR::Arr hd = SR::ReadArr(save, kOff_hold);
    if (!hd.data || hd.num < 1) return {};
    std::wstring name = SR::ReadFNameAt(hd.data, kEquip_propName);
    if (name == L"None") name.clear();
    return name;
}

bool WriteHeldItem(const std::wstring& name, const wchar_t* classLeaf) {
    void* save = ResolveSaveSlot();
    if (!save) return false;
    SR::Arr hd{};if(!SR::TryReadArr(save,kOff_hold,hd))return false;
    if (!hd.data || hd.num < 1) return false;
    const auto serial=++g_handWriteSerial;
    const auto world=world_identity::Generation();
    CachedObjRef target;target.Set(save);
    std::array<uint8_t,16> header{};std::memcpy(header.data(),static_cast<uint8_t*>(save)+kOff_hold,header.size());
    std::array<uint8_t,kEquipStride> image{};std::memcpy(image.data(),hd.data,image.size());
    const auto current=[&]{return serial==g_handWriteSerial && world==world_identity::Generation() && target.Get()==save &&
        ResolveSaveSlot()==save && !std::memcmp(header.data(),static_cast<uint8_t*>(save)+kOff_hold,header.size()) &&
        !std::memcmp(image.data(),hd.data,image.size());};
    const std::wstring cls=classLeaf?classLeaf:L"";
    const std::wstring itemName=name;
    if(!cls.empty() && !R::FindClass(cls.c_str()))return false;
    if(!current())return false;
    // The record as the game's own pickup leaves it, as far as the hand reads it: the class updateHold
    // spawns, and the name the spawned actor's loadData takes, its first names entry, which
    // propToDynamic also made the slot's own name from; an emptied hand's has neither. The rest of the
    // record stays, and its old buffers are orphaned, as every rewrite of one is.
    EquipRecord record;ReadEquipRecord(hd.data,record);
    if(!current())return false;
    record.propName=itemName;record.data.className=cls;
    if(classLeaf)record.data.names.assign(1,std::vector<std::wstring>{itemName});
    else record.data.names.clear();
    std::array<uint8_t,kEquipStride> staged{};
    if(!WriteEquipRecord(staged.data(),record) || !current())return false;
    std::memcpy(const_cast<uint8_t*>(hd.data),staged.data(),staged.size());
    return true;
}

bool ReadAll(PlayerInventory& out) {
    out={};PlayerInventory snapshot;
    LivePersonalStore live;
    if (!ReadLivePersonalStore(live)) return false;  // pre-world: no container, nothing carried yet
    void* save = ResolveSaveSlot();
    if (!save) return false;
    SR::Arr eq{},hd{};
    if(!SR::TryReadArr(save,kOff_equipment,eq) || !SR::TryReadArr(save,kOff_hold,hd))return false;
    snapshot.inventory=std::move(live.records);
    snapshot.equipment.reserve(static_cast<size_t>(eq.num));
    for (int32_t i = 0; i < eq.num; ++i) {
        EquipRecord r;
        ReadEquipRecord(eq.data + static_cast<size_t>(i) * kEquipStride, r);
        snapshot.equipment.push_back(std::move(r));
    }
    snapshot.hold.reserve(static_cast<size_t>(hd.num));
    for (int32_t i = 0; i < hd.num; ++i) {
        EquipRecord r;
        ReadEquipRecord(hd.data + static_cast<size_t>(i) * kEquipStride, r);
        snapshot.hold.push_back(std::move(r));
    }
    out=std::move(snapshot);
    return true;
}

bool PersistHeldMaterialStock(void* actor, const std::function<bool()>& current) {
    if (!actor || (current && !current())) return false;
    CachedObjRef hand; hand.Set(actor);
    void* save = ResolveSaveSlot();
    if (!save) return false;
    CachedObjRef slot; slot.Set(save);
    const auto world = world_identity::Generation();
    const auto valid = [&] {
        return hand.Get() == actor && slot.Get() == save && world == world_identity::Generation() &&
            (!current || current()) && ResolveSaveSlot() == save;
    };
    const SR::Arr held = SR::ReadArr(save, kOff_hold);
    if (!held.data || held.num < 1) return false;
    std::array<uint8_t, kEquipStride> slotImage{};
    std::memcpy(slotImage.data(), held.data, slotImage.size());
    EquipRecord original; ReadEquipRecord(held.data, original);
    const auto& cls = original.data.className;
    if (cls != L"prop_wallfixer_C" && cls != L"prop_bucket_C" && cls != L"prop_concreteBucket_C" &&
        cls != L"prop_cementBag_C" && cls != L"prop_sponge_C") return false;
    auto refreshed = original.data;
    if (original.propKey.empty() || original.propKey != refreshed.key ||
        !building_material::RefreshHeldMaterialRecord(actor, refreshed) || !valid()) return false;
    const auto stillHeld = SR::ReadArr(save, kOff_hold);
    if (stillHeld.data != held.data || stillHeld.num != held.num) return false;
    EquipRecord latest; ReadEquipRecord(stillHeld.data, latest);
    if (latest.propKey != original.propKey || latest.propName != original.propName || latest.tag != original.tag ||
        latest.data.key != original.data.key || latest.data.className != original.data.className || !valid()) return false;
    auto confirmed = original.data;
    if (!building_material::RefreshHeldMaterialRecord(actor, confirmed) ||
        confirmed.ints != refreshed.ints || confirmed.floats != refreshed.floats ||
        confirmed.bools != refreshed.bools || !valid()) return false;
    const auto finalHeld = SR::ReadArr(save, kOff_hold);
    if (finalHeld.data != held.data || finalHeld.num != held.num ||
        std::memcmp(held.data, slotImage.data(), slotImage.size()) != 0) return false;
    return SR::UpdateScalarGroups(const_cast<uint8_t*>(held.data) + kEquip_data, original.data, refreshed);
}

int32_t LivePersonalStoreCount() {
    SR::Arr contents{};
    return ValidatedPersonalSlot(contents) < 0 ? -1 : contents.num;
}

bool ReadLivePersonalStore(LivePersonalStore& out) {
    out.slotIndex = -1;
    out.records.clear();
    SR::Arr contents{};
    const int32_t idx = ValidatedPersonalSlot(contents);
    if (idx < 0) return false;

    out.slotIndex = idx;
    out.records.reserve(static_cast<size_t>(contents.num));
    for (int32_t i = 0; i < contents.num; ++i) {
        SR::SaveRecord r;
        SR::ReadSaveRecord(contents.data + static_cast<size_t>(i) * SR::kSaveStride, r);
        out.records.push_back(std::move(r));
    }
    return true;
}

bool ApplyToSaveObjectWhile(void* saveSlot, const PlayerInventory& inv,const std::function<bool()>& callerCurrent) {
    if(callerCurrent && !callerCurrent())return false;
    if (!saveSlot || !R::IsLive(saveSlot)) return false;
    if(inv.inventory.size()>SR::kMaxArr || inv.equipment.size()>SR::kMaxArr || inv.hold.size()>SR::kMaxArr)return false;
    // Native string/name construction may reenter the caller and replace its
    // profile. Own every input before the first engine dispatch.
    const PlayerInventory snapshot=inv;
    const auto serial=++g_applySerial;
    const auto world=world_identity::Generation();
    void* gm=world_singleton::Gamemode();void* bound=ResolveSaveSlot();
    CachedObjRef target,type,mode,liveSave;
    target.Set(saveSlot);type.Set(R::ClassOf(saveSlot));mode.Set(gm);liveSave.Set(bound);
    uint8_t* carried = PersonalSlotOf(saveSlot);
    if (!carried) {
        UE_LOGE("inventory: ApplyToSaveObject -- save object %p has no GObjStack[%d]; refusing to "
                "write a partial inventory", saveSlot, kPersonalSlot);
        return false;
    }
    const auto stackOff=g_offGObjStack.Of(saveSlot);
    struct Header {
        const uint8_t* at;std::array<uint8_t,16> bytes{};
        explicit Header(const void* p):at(static_cast<const uint8_t*>(p)){std::memcpy(bytes.data(),at,bytes.size());}
        bool Same()const{return std::memcmp(at,bytes.data(),bytes.size())==0;}
    } stackHeader(static_cast<uint8_t*>(saveSlot)+stackOff),carriedHeader(carried),
      equipmentHeader(static_cast<uint8_t*>(saveSlot)+kOff_equipment),heldHeader(static_cast<uint8_t*>(saveSlot)+kOff_hold);
    const auto current=[&] {
        return (!callerCurrent || callerCurrent()) && serial==g_applySerial && world==world_identity::Generation() && target.Get()==saveSlot && type.Get() &&
            R::ClassOf(saveSlot)==type.Get() && (!gm || mode.Get()==gm) && (!bound || liveSave.Get()==bound) &&
            world_singleton::Gamemode()==gm && ResolveSaveSlot()==bound &&
            stackHeader.Same() && carriedHeader.Same() && equipmentHeader.Same() && heldHeader.Same();
    };
    // equipment and hold are FIXED-SHAPE slot arrays: the game finds an empty slot to equip into
    // (AddEquipment: Array_Find, "Not enough space" on -1) and writes hold[0] in place, and
    // nothing in it ever grows them. A profile with fewer slots than the save object -- an empty
    // one above all -- is padded with blank slots to the shape the save object came with.
    SR::Arr oldCarried{},oldEquipment{},oldHeld{};
    if(!SR::TryReadArr(carried,0,oldCarried) || !SR::TryReadArr(saveSlot,kOff_equipment,oldEquipment) ||
       !SR::TryReadArr(saveSlot,kOff_hold,oldHeld))return false;
    auto slotsOf = [](size_t shape, const std::vector<EquipRecord>& recs) {
        std::vector<EquipRecord> out = recs;
        if (out.size() < shape) out.resize(shape);
        return out;
    };
    const auto equipment=slotsOf(static_cast<size_t>(oldEquipment.num),snapshot.equipment),held=slotsOf(static_cast<size_t>(oldHeld.num),snapshot.hold);
    StagedArray items,worn,hand;
    if(!current() || !items.Allocate(snapshot.inventory.size(),SR::kSaveStride) || !current() ||
       !worn.Allocate(equipment.size(),kEquipStride) || !current() ||
       !hand.Allocate(held.size(),kEquipStride) || !current())return false;
    const auto writeItem=[](uint8_t* at,const SR::SaveRecord& record){return SR::WriteSaveRecordChecked(at,record);};
    const auto writeEquip=[](uint8_t* at,const EquipRecord& record){return WriteEquipRecord(at,record);};
    if(!items.Encode(snapshot.inventory,SR::kSaveStride,writeItem,current) ||
       !worn.Encode(equipment,kEquipStride,writeEquip,current) ||
       !hand.Encode(held,kEquipStride,writeEquip,current) || !current())return false;
    // No dispatch between these raw header swaps. Until here the live save has
    // not changed. Old/dead scratch follows the existing nested-buffer pin policy.
    items.Publish(carried,0);worn.Publish(saveSlot,kOff_equipment);hand.Publish(saveSlot,kOff_hold);
    UE_LOGI("inventory: ApplyToSaveObject(%p) wrote carried=%zu equipment=%zu hold=%zu",
            saveSlot, snapshot.inventory.size(), snapshot.equipment.size(), snapshot.hold.size());
    return true;
}

bool ApplyToSaveObject(void* saveSlot,const PlayerInventory& inv) {return ApplyToSaveObjectWhile(saveSlot,inv,{});}

}  // namespace ue_wrap::inventory
