// ue_wrap/actors/container_inventory.cpp -- see ue_wrap/actors/container_inventory.h.

#include "ue_wrap/actors/container_inventory.h"

#include "ue_wrap/actors/inventory.h"      // ResolveSaveSlot
#include "ue_wrap/actors/prop.h"           // WalksToBase
#include "ue_wrap/core/component_calls.h"  // CallParamless
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"

#include <cstring>
#include <array>

namespace ue_wrap::container_inventory {
namespace {

namespace R  = ue_wrap::reflection;
namespace SR = ue_wrap::save_record;

R::InstanceOffset g_offInvIndex{L"Index"},g_offInvOwner{L"Owner"},g_offGObjStack{L"GObjStack"},
    g_offPropInv{L"propInventory"},g_offCurrVol{L"currVol"};
CachedObjRef g_playerClass;
int32_t g_playerOffset=-1;
uint8_t g_playerMask=0;
R::InstanceOffset g_offInvObj{L"obj"}, g_offAllContainers{L"allContainers"};

template <class T> T ReadAt(const void* base, int32_t off) {
    T v{};
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + off, sizeof(T));
    return v;
}

// Looked up per use, one index lookup each, so a class not loaded yet is asked for again.
void* ContainerClass() { return ue_wrap::object_index::ClassByName(L"prop_container_C"); }
void* InventoryClass() { return ue_wrap::object_index::ClassByName(L"propInventory_C"); }

}  // namespace

bool IsContainer(void* actor) {
    void* base = ContainerClass();
    return base && actor && ue_wrap::prop::WalksToBase(R::ClassOf(actor), base);
}

bool IsInventory(void* obj) {
    void* base = InventoryClass();
    return base && obj && ue_wrap::prop::WalksToBase(R::ClassOf(obj), base);
}

void* InventoryOf(void* container) {
    if (!container) return nullptr;
    const auto off=g_offPropInv.Of(container);if(off<0)return nullptr;
    void* inv = ReadAt<void*>(container, off);
    return (inv && R::IsLive(inv)) ? inv : nullptr;
}

void* OwnerOf(void* inventory) {
    if (!inventory) return nullptr;
    const auto off=g_offInvOwner.Of(inventory);if(off<0)return nullptr;
    void* owner = ReadAt<void*>(inventory, off);
    return (owner && R::IsLive(owner)) ? owner : nullptr;
}

bool IsWorldInventory(void* inventory) {
    bool personal=false;return ReadPersonalFlag(inventory,personal) && !personal;
}

bool ReadPersonalFlag(void* inventory,bool& personal) {
    personal=false;
    if(!inventory || !R::IsLive(inventory) || !IsInventory(inventory))return false;
    void* cls=R::ClassOf(inventory);
    if(!g_playerClass.Is(cls)) {
        int32_t off=-1;uint8_t mask=0;
        if(!R::FindBoolProperty(cls,L"Player",off,mask) || off<0 || !mask)return false;
        g_playerOffset=off;g_playerMask=mask;g_playerClass.Set(cls);
    }
    personal=(ReadAt<uint8_t>(inventory,g_playerOffset)&g_playerMask)!=0;
    return true;
}

uint8_t* ContentsSlot(void* inventory) {
    if (!inventory) return nullptr;
    void* save = ue_wrap::inventory::ResolveSaveSlot();
    if (!save) return nullptr;
    const auto stackOffset=g_offGObjStack.Of(save),indexOffset=g_offInvIndex.Of(inventory);
    if(stackOffset<0 || indexOffset<0)return nullptr;
    const int32_t idx = ReadAt<int32_t>(inventory, indexOffset);
    if (idx < 0) return nullptr;
    SR::Arr stack{};if(!SR::TryReadArr(save,stackOffset,stack))return nullptr;
    if (idx >= stack.num) return nullptr;
    return const_cast<uint8_t*>(stack.data) + static_cast<size_t>(idx) * SR::kMxStride;
}

bool ReadContents(void* inventory, std::vector<SR::SaveRecord>& out, size_t maxRecords,
                  int32_t* countOut) {
    out.clear();
    if (countOut) *countOut = -1;
    uint8_t* slot = ContentsSlot(inventory);
    if (!slot) return false;
    SR::Arr objs{};if(!SR::TryReadArr(slot,0,objs))return false;
    if (static_cast<size_t>(objs.num) > maxRecords){if(countOut)*countOut=objs.num;return false;}
    if (countOut) *countOut = objs.num;
    out.clear();
    out.reserve(static_cast<size_t>(objs.num));
    for (int32_t i = 0; i < objs.num; ++i) {
        SR::SaveRecord r;
        SR::ReadSaveRecord(objs.data + static_cast<size_t>(i) * SR::kSaveStride, r);
        out.push_back(std::move(r));
    }
    return true;
}

uint8_t* WorldContentsSlot(void* inventory) {
    if(!IsWorldInventory(inventory))return nullptr;
    const auto off=g_offInvIndex.Of(inventory);
    if(off<0 || ReadAt<int32_t>(inventory,off)<=0)return nullptr;
    return ContentsSlot(inventory);
}

bool RefundRecordWhile(void* inventory,const SR::SaveRecord& value,const std::function<bool()>& callerCurrent) {
    if(!callerCurrent || !callerCurrent())return false;
    const SR::SaveRecord record=value;
    if(record.key.empty() || record.key==L"None" || record.className.empty() || !IsWorldInventory(inventory))return false;
    void* owner=OwnerOf(inventory);void* save=ue_wrap::inventory::ResolveSaveSlot();
    uint8_t* slot=WorldContentsSlot(inventory);
    if(!owner || !save || !slot || InventoryOf(owner)!=inventory)return false;
    CachedObjRef inv,actor,store,invType,actorType,saveType;
    inv.Set(inventory);actor.Set(owner);store.Set(save);
    invType.Set(R::ClassOf(inventory));actorType.Set(R::ClassOf(owner));saveType.Set(R::ClassOf(save));
    const auto world=world_identity::Generation();
    const auto stackOffset=g_offGObjStack.Of(save),indexOffset=g_offInvIndex.Of(inventory);
    if(stackOffset<0 || indexOffset<0)return false;
    const auto index=ReadAt<int32_t>(inventory,indexOffset);
    std::array<uint8_t,16> stackHeader{},rowHeader{};
    auto* stackAt=static_cast<uint8_t*>(save)+stackOffset;
    std::memcpy(stackHeader.data(),stackAt,stackHeader.size());
    std::memcpy(rowHeader.data(),slot,rowHeader.size());
    SR::Arr rows{};if(!SR::TryReadArr(slot,0,rows))return false;
    const auto current=[&] {
        return callerCurrent() && world==world_identity::Generation() && inv.Get()==inventory && actor.Get()==owner && store.Get()==save &&
            invType.Is(R::ClassOf(inventory)) && actorType.Is(R::ClassOf(owner)) && saveType.Is(R::ClassOf(save)) &&
            ue_wrap::inventory::ResolveSaveSlot()==save && InventoryOf(owner)==inventory && OwnerOf(inventory)==owner &&
            IsWorldInventory(inventory) && g_offInvIndex.Of(inventory)==indexOffset && ReadAt<int32_t>(inventory,indexOffset)==index &&
            g_offGObjStack.Of(save)==stackOffset && !std::memcmp(stackHeader.data(),stackAt,stackHeader.size()) &&
            WorldContentsSlot(inventory)==slot && !std::memcmp(rowHeader.data(),slot,rowHeader.size());
    };
    // Inspect identity independently of payload: an already returned row can
    // have newer charge/contents, which a retry must never overwrite.
    const auto present=[&] {
        for(int32_t i=0;i<rows.num;++i) {
            SR::SaveRecord existing;
            SR::ReadSaveRecord(rows.data+size_t(i)*SR::kSaveStride,existing);
            if(!current())return -1;
            if(existing.key==record.key)return existing.className==record.className?1:-1;
        }
        return 0;
    };
    if(!current())return false;
    const int already=present();if(already)return already>0;
    if(rows.num>=SR::kMaxArr)return false;
    std::array<uint8_t,SR::kSaveRecordBytes> added{};
    if(!SR::WriteSaveRecordChecked(added.data(),record,ue_wrap::object_index::ClassByName) || !current())return false;
    void* fresh=SR::AllocZeroed(size_t(rows.num)+1,SR::kSaveStride);
    if(!fresh)return false;
    // No live row is copied until every engine allocation/codec callback ends.
    // Concurrent in-place edits to other rows therefore survive this append.
    if(!current()){R::EngineFree(fresh);return false;}
    const int nowPresent=present();
    if(nowPresent){R::EngineFree(fresh);return nowPresent>0;}
    if(!current()){R::EngineFree(fresh);return false;}
    if(rows.num)std::memcpy(fresh,rows.data,size_t(rows.num)*SR::kSaveStride);
    std::memcpy(static_cast<uint8_t*>(fresh)+size_t(rows.num)*SR::kSaveStride,added.data(),added.size());
    SR::WriteArrHeader(slot,0,fresh,rows.num+1);
    // Existing child buffers transferred into fresh. Cleanup may reenter after
    // this commit; its caller must not mistake that for an uncommitted refund.
    if(rows.data)R::EngineFree(const_cast<uint8_t*>(rows.data));
    return true;
}

bool IsContainerRecord(const SR::SaveRecord& r) {
    if (r.className.empty()) return false;
    void* const base = ContainerClass();
    if (!base) return false;
    void* const cls = ue_wrap::object_index::ClassByName(r.className.c_str());
    return cls && ue_wrap::prop::WalksToBase(cls, base);
}

bool CarriesInventoryIndex(const SR::SaveRecord& r) {
    return !r.ints.empty() && !r.ints[0].empty() && r.ints[0][0] != -1;
}

void ClearInventoryIndex(SR::SaveRecord& r) {
    if (r.ints.empty()) r.ints.resize(1);
    if (r.ints[0].empty()) r.ints[0].resize(1);
    r.ints[0][0] = -1;
}

// updateVolumesAndMass calls only Get Volume; the ejector checkObjectsVolume (which calls takeObj) is
// not called. Each verb is looked up on the instance's own class through the memoised dispatch
// lookup, which climbs to the class that declares it -- updateVolumesAndMass is declared only on
// Aprop_container_C, and every real container is a subclass -- and holds its answer by the class's
// slot and serial, so no function of a class that is gone is called.
void RederiveShownState(void* container, void* inventory) {
    RederiveShownStateWhile(container,inventory,[]{return true;});
}

void RederiveShownStateWhile(void* container, void* inventory,const std::function<bool()>& callerCurrent) {
    if(!callerCurrent || !callerCurrent())return;
    CachedObjRef owner,inv,ownerType,invType;
    if(container){if(!R::IsLive(container))return;owner.Set(container);ownerType.Set(R::ClassOf(container));}
    if(inventory){if(!R::IsLive(inventory))return;inv.Set(inventory);invType.Set(R::ClassOf(inventory));}
    const auto world=world_identity::Generation();
    const auto current=[&]{return callerCurrent() && world==world_identity::Generation() &&
        (!container || (owner.Get()==container && ownerType.Get() && R::ClassOf(container)==ownerType.Get())) &&
        (!inventory || (inv.Get()==inventory && invType.Get() && R::ClassOf(inventory)==invType.Get())) &&
        (!container || !inventory || (InventoryOf(container)==inventory && OwnerOf(inventory)==container));};
    if(!current())return;
    void* const updateVol =
        container ? R::FindDispatchFunctionCached(R::ClassOf(container), L"updateVolumesAndMass") : nullptr;
    void* const recalcNames =
        inventory ? R::FindDispatchFunctionCached(R::ClassOf(inventory), L"recalculateNames") : nullptr;
    if ((container && !updateVol) || (inventory && !recalcNames)) {
        static bool s_said = false;
        if (!s_said) {
            s_said = true;
            UE_LOGW("container_inventory: re-derive verb MISSING (updateVolumesAndMass=%p recalculateNames=%p) "
                    "-- applied contents will show a STALE currVol / names", updateVol, recalcNames);
        }
    }
    CachedObjRef update,recalc;update.Set(updateVol);recalc.Set(recalcNames);
    if (current() && recalc.Get())ue_wrap::component_calls::CallParamless(inventory,recalc.Get());
    if (current() && update.Get())ue_wrap::component_calls::CallParamless(container,update.Get());
}

bool CurrentVolume(void* inventory, float& out) {
    out = 0.f;
    const auto off=g_offCurrVol.Of(inventory);if(off<0)return false;
    out = ReadAt<float>(inventory, off);
    return true;
}

// The host's world-settle edge: propInventory.init sets `obj` on its first line and nothing clears it,
// so a live `obj` is the mark that a container's inventory has run its init.
bool InventoryInitialized(void* container) {
    void* const inv = InventoryOf(container);
    const int offset=inv?g_offInvObj.Of(inv):-1;
    if(offset<0)return false;
    void* const obj=ReadAt<void*>(inv,offset);
    return obj && R::IsLive(obj);
}

bool AllContainersResolved(void* gamemode) {
    return gamemode && g_offAllContainers.Of(gamemode) >= 0;
}

// The gamemode's allContainers (each container adds itself at BeginPlay and leaves at its end): the
// registry the host's world-settle edge counts against. Null and non-live entries are skipped.
size_t AllContainers(void* gamemode, void** out, size_t cap) {
    if (!gamemode || !out || cap == 0) return 0;
    if (g_offAllContainers.Of(gamemode) < 0) return 0;
    SR::Arr arr{};
    if(!SR::TryReadArr(gamemode,g_offAllContainers.Of(gamemode),arr))return 0;
    size_t n = 0;
    for (int32_t i = 0; i < arr.num && n < cap; ++i) {
        void* const c = ReadAt<void*>(arr.data, i * static_cast<int32_t>(sizeof(void*)));
        if (c && R::IsLive(c)) out[n++] = c;
    }
    return n;
}

}  // namespace ue_wrap::container_inventory
