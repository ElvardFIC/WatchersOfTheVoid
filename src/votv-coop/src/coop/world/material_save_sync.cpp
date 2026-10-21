#include "coop/world/material_save_sync.h"
#include "ue_wrap/actors/wall_drying.h"
#include "ue_wrap/actors/material_saved_state.h"
#include "ue_wrap/devices/grill_saved_state.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/log.h"
namespace coop::material_save_sync {
namespace {
namespace SG = ue_wrap::script_gate;
namespace R = ue_wrap::reflection;
bool g_saveWatch[4]{}, g_loadWatch[4]{};
void* Record(const SG::Call& c) {
    if (!c.function) return nullptr;
    const int off = R::FindParamOffset(c.function,L"data"), size = R::FunctionFrameSize(c.function);
    if (off < 0 || off > size || size-off < ue_wrap::save_record::kSaveRecordBytes) return nullptr;
    if (void* out = SG::OutParamPtr(c,off)) return out;
    return c.locals ? c.locals + off : nullptr;
}
void Saved(const SG::Call& c) {
    const bool wall = ue_wrap::wall_drying::IsWetWall(c.object);
    const bool grill=ue_wrap::grill_saved_state::Supported(c.object);
    if (!wall && !grill && !ue_wrap::material_saved_state::Supported(c.object)) return;
    static bool warned = false;
    const bool saved = wall ? ue_wrap::wall_drying::SaveNative(c.object,Record(c)) :
        (grill?ue_wrap::grill_saved_state::SaveNative(c.object,Record(c)):ue_wrap::material_saved_state::SaveNative(c.object,Record(c)));
    if (!saved && !warned) {
        warned = true;
        UE_LOGW("material_save: native save extension failed for %ls",R::ClassNameOf(c.object).c_str());
    }
}
void Loaded(const SG::Call& c) {
    const bool wall = ue_wrap::wall_drying::IsWetWall(c.object);
    const bool grill=ue_wrap::grill_saved_state::Supported(c.object);
    if (!wall && !grill && !ue_wrap::material_saved_state::Supported(c.object)) return;
    static bool warned = false;
    const bool loaded = wall ? ue_wrap::wall_drying::LoadNative(c.object,Record(c)) :
        (grill?ue_wrap::grill_saved_state::LoadNative(c.object,Record(c)):ue_wrap::material_saved_state::LoadNative(c.object,Record(c)));
    if (!loaded && !warned) {
        warned = true;
        UE_LOGW("material_save: unreadable saved extension for %ls; native defaults retained",R::ClassNameOf(c.object).c_str());
    }
}
}
void Install() {
    constexpr int tag = 0x57445259;
    const wchar_t* classes[]{L"customWall_C",L"prop_concreteBucket_C",L"prop_sponge_C",L"prop_bbq_C"};
    for (int i=0;i<4;++i) {
        if (!g_saveWatch[i]) g_saveWatch[i] = SG::WatchClassName(classes[i],L"getData",tag,nullptr,&Saved);
        if (!g_loadWatch[i]) g_loadWatch[i] = SG::WatchClassName(classes[i],L"loadData",tag,nullptr,&Loaded);
    }
}
}
