#include "ue_wrap/actors/material_saved_state.h"
#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::material_saved_state {
namespace {
namespace R = reflection;
enum class Kind { None, Concrete, Sponge };
Kind Type(void* actor) {
    if (!actor || !R::IsLive(actor)) return Kind::None;
    const auto cls = R::ClassNameOf(actor);
    if (cls == L"prop_concreteBucket_C") return Kind::Concrete;
    if (cls == L"prop_sponge_C" || cls == L"prop_sponge_p_C") return Kind::Sponge;
    return Kind::None;
}
}
bool Supported(void* actor) { return Type(actor) != Kind::None; }
bool SaveNative(void* actor, void* record) {
    const auto kind = Type(actor);
    if (kind == Kind::None) return false;
    CachedObjRef ref; ref.Set(actor); const auto world = world_identity::Generation();
    building_material::Stock stock{};
    if (!building_material::ReadStock(actor,stock) || !ref.Get() || world != world_identity::Generation()) return false;
    if (kind == Kind::Concrete && stock.kind == building_material::StockKind::ConcreteBucket)
        return save_record::ReplaceFloatGroup(record,1,{stock.dryTimer});
    if (kind == Kind::Sponge && stock.kind == building_material::StockKind::Sponge) {
        std::vector<float> native;
        if (!save_record::ReadFloatGroup(record,1,native) || (native.size()!=1 && native.size()!=2)) return false;
        return save_record::ReplaceFloatGroup(record,1,{stock.wetness,stock.soapAmount});
    }
    return false;
}
bool LoadNative(void* actor, const void* record) {
    const auto kind = Type(actor);
    if (kind == Kind::None || !record) return false;
    std::vector<float> fields;
    if (!save_record::ReadFloatGroup(record,1,fields)) return kind == Kind::Concrete; // old native record
    if (kind == Kind::Sponge && fields.size()==1) return true;
    if (kind == Kind::Sponge && (fields.empty() || !std::isfinite(fields[0]) || fields[0]<0 || fields[0]>1'000'000.f)) return false;
    const size_t index = kind == Kind::Concrete ? 0 : 1;
    if (fields.size() != index+1 || !std::isfinite(fields[index]) ||
        fields[index] < (kind == Kind::Concrete ? 0.f : -5.f) || fields[index] > 1'000'000.f) return false;
    CachedObjRef ref; ref.Set(actor); const auto world = world_identity::Generation();
    R::InstanceOffset field(kind == Kind::Concrete ? L"dryTimer" : L"soapAmount");
    const auto off = field.Of(actor);
    if (off < 0 || !ref.Get() || world != world_identity::Generation()) return false;
    std::memcpy(static_cast<uint8_t*>(actor)+off,&fields[index],sizeof(float));
    return true; // no drying, washing, sounds or object births during restoration
}
}
