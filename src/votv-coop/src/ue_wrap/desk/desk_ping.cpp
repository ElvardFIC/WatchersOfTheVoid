// ue_wrap/desk/desk_ping.cpp -- see ue_wrap/desk/desk_ping.h.

#include "ue_wrap/desk/desk_ping.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/desk/coords_panel.h"
#include "ue_wrap/engine/engine_audio.h"
#include "ue_wrap/engine/engine_component.h"

#include <cstdint>
#include <cstring>

namespace ue_wrap::desk_ping {
namespace {

namespace R = reflection;
namespace sg = script_gate;

R::InstanceOffset g_stage{L"coord_pingStage"};
R::InstanceOffset g_outer{L"coord_ping_outer"};
R::InstanceOffset g_inner{L"coord_ping_inner"};
R::InstanceOffset g_ping{L"coord_ping_ping"};

// coord_isPing's byte and bit, found on the class of the desk it is asked with and kept per class object, as
// InstanceOffset keeps an offset.
struct BoolField {
    CachedObjRef cls;
    int32_t off = -1;
    uint8_t mask = 0;
};
BoolField g_isPing;

uint8_t* PingingByte(void* desk, uint8_t& mask) {
    void* cls = desk ? R::ClassOf(desk) : nullptr;
    if (!cls) return nullptr;
    if (!g_isPing.cls.Alive() || g_isPing.cls.Raw() != cls) {
        g_isPing = BoolField{};
        g_isPing.cls.Set(cls);
        if (!R::FindBoolProperty(cls, L"coord_isPing", g_isPing.off, g_isPing.mask)) {
            g_isPing.off = -1;
            UE_LOGW("desk_ping: the desk's coord_isPing does not read -- its ping machine is out of reach");
        }
    }
    if (g_isPing.off < 0) return nullptr;
    mask = g_isPing.mask;
    return static_cast<uint8_t*>(desk) + g_isPing.off;
}

template <typename T>
T* Field(void* desk, R::InstanceOffset& member) {
    const int32_t off = desk ? member.Of(desk) : -1;
    return off < 0 ? nullptr : reinterpret_cast<T*>(static_cast<uint8_t*>(desk) + off);
}

R::InstanceOffset g_linesEdges{L"dynmat_linesEdges"};  // ui_coordinates_C: TArray<UMaterialInstanceDynamic*>
R::InstanceOffset g_linesInner{L"dynmat_linesInner"};
R::FName g_alphaName{0, 0};

// The first material of the panel's TArray<UMaterialInstanceDynamic*> at `off` on `panel`, and its alpha scalar.
bool ReadFirstLineAlpha(void* panel, int32_t off, float& out) {
    if (off < 0) return false;
    const auto* base = static_cast<const uint8_t*>(panel) + off;
    uint8_t* data = nullptr;
    int32_t num = 0;
    std::memcpy(&data, base, sizeof(data));
    std::memcpy(&num, base + sizeof(void*), sizeof(num));
    if (!data || num < 1) return false;
    void* mid = nullptr;
    std::memcpy(&mid, data, sizeof(mid));
    if (!mid || !R::IsLive(mid)) return false;
    if (g_alphaName.ComparisonIndex == 0 && g_alphaName.Number == 0)
        g_alphaName = fname_utils::StringToFName(L"alpha");
    return engine::GetScalarParameterValue(mid, g_alphaName, out);
}

R::ParamOffset g_return{L"return"};
R::ParamOffset g_caughtAny{L"caughtAtLeastOne"};
R::ParamOffset g_data{L"data"};
R::ParamOffset g_newSound{L"NewSound"};

bool WriteMachine(void* desk, bool pinging, int32_t stage, float accumulators) {
    uint8_t mask = 0;
    uint8_t* flag = PingingByte(desk, mask);
    int32_t* stageField = Field<int32_t>(desk, g_stage);
    float* outer = Field<float>(desk, g_outer);
    float* inner = Field<float>(desk, g_inner);
    float* ping = Field<float>(desk, g_ping);
    if (!flag || !stageField || !outer || !inner || !ping) return false;
    *flag = static_cast<uint8_t>(pinging ? (*flag | mask) : (*flag & ~mask));
    *stageField = stage;
    *outer = accumulators;
    *inner = accumulators;
    *ping = accumulators;
    return true;
}

}  // namespace

bool ReadPinging(void* desk, bool& out) {
    uint8_t mask = 0;
    const uint8_t* b = PingingByte(desk, mask);
    if (!b) return false;
    out = (*b & mask) != 0;
    return true;
}

bool ReadProgress(void* desk, Progress& out) {
    int32_t* stage = Field<int32_t>(desk, g_stage);
    float* outer = Field<float>(desk, g_outer);
    float* inner = Field<float>(desk, g_inner);
    float* ping = Field<float>(desk, g_ping);
    if (!stage || !outer || !inner || !ping) return false;
    out = {*stage, *outer, *inner, *ping};
    return true;
}

bool WriteProgress(void* desk, const Progress& value) {
    int32_t* stage = Field<int32_t>(desk, g_stage);
    float* outer = Field<float>(desk, g_outer);
    float* inner = Field<float>(desk, g_inner);
    float* ping = Field<float>(desk, g_ping);
    if (!stage || !outer || !inner || !ping) return false;
    *stage = value.stage;
    *outer = value.outer;
    *inner = value.inner;
    *ping = value.ping;
    return true;
}

bool ReadObserverView(void* desk, ObserverView& out) {
    Progress p;
    ObserverView v;
    void* panel = coords_panel::Instance();
    if (!ReadPinging(desk, v.isPing) || !ReadProgress(desk, p) || !panel) return false;
    if (!ReadFirstLineAlpha(panel, g_linesEdges.Of(panel), v.alphaEdges) ||
        !ReadFirstLineAlpha(panel, g_linesInner.Of(panel), v.alphaInner))
        return false;
    v.stage = p.stage;
    v.outer = p.outer;
    v.inner = p.inner;
    v.ping = p.ping;
    out = v;
    return true;
}

bool PrimeVerdict(void* desk) { return WriteMachine(desk, true, 2, 1.f); }

bool ResetMachine(void* desk) { return WriteMachine(desk, false, 0, 0.f); }

bool WriteNoVerdict(const sg::Call& c) {
    const int32_t retOff = g_return.Of(c.function);
    const int32_t anyOff = g_caughtAny.Of(c.function);
    uint8_t* ret = retOff >= 0 ? sg::OutParamPtr(c, retOff) : nullptr;
    uint8_t* any = anyOff >= 0 ? sg::OutParamPtr(c, anyOff) : nullptr;
    if (!ret || !any) return false;
    *ret = 0;
    *any = 0;
    return true;
}

bool ReadCaught(const sg::Call& c, bool& out) {
    const int32_t retOff = g_return.Of(c.function);
    const uint8_t* ret = retOff >= 0 ? sg::OutParamPtr(c, retOff) : nullptr;
    if (!ret) return false;
    out = *ret != 0;
    return true;
}

bool ReadVerdictSignal(const sg::Call& c, console_desk::CoordSignal& out) {
    const int32_t off = g_data.Of(c.function);
    const uint8_t* data = off >= 0 ? sg::OutParamPtr(c, off) : nullptr;
    return data && console_desk::ReadSignalAt(data, out);
}

bool IsStageChangeCue(const sg::Call& c) {
    const int32_t off = g_newSound.Of(c.function);
    if (off < 0 || !c.locals) return false;
    void* sound = nullptr;
    std::memcpy(&sound, c.locals + off, sizeof(sound));
    return sound && R::IsLive(sound) && R::NameEquals(R::NameOf(sound), L"newdesk_panelCoord_stageChange");
}

bool CallInstaCatch(void* desk) {
    void* cls = desk ? R::ClassOf(desk) : nullptr;
    void* fn = cls ? R::FindDispatchFunctionCached(cls, kInstaCatch) : nullptr;
    if (!fn) return false;
    ParamFrame f(fn);
    return f.valid() && Call(desk, f);
}

bool PlayPingSound(void* desk, const wchar_t* soundName) {
    void* cls = desk ? R::ClassOf(desk) : nullptr;
    void* fn = cls ? R::FindDispatchFunctionCached(cls, kPingSound) : nullptr;
    void* sound = engine::FindSound(soundName);
    if (!fn || !sound) return false;
    ParamFrame f(fn);
    return f.valid() && f.Set<void*>(L"NewSound", sound) && Call(desk, f);
}

}  // namespace ue_wrap::desk_ping
