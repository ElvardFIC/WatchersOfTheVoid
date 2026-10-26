#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/actors/llama_marker.h"
#include "ue_wrap/actors/wall_drying.h"
#include <cmath>

namespace ue_wrap::placed_save {
namespace SR = save_record;
namespace {
const wchar_t* const plants[] = {L"growingPlant_C", L"growingPlant_bush_C",
    L"growingPlant_deadtree_C", L"growingPlant_grassPatch_C", L"growingPlant_lemontree_C",
    L"growingPlant_orangetree_C", L"growingPlant_pine_C", L"growingPlant_shroom_0_C",
    L"growingPlant_shroom_2_C", L"growingPlant_shroom_3_C", L"growingPlant_shroom_4_C",
    L"growingPlant_shroom_5_C", L"growingPlant_shroom_6_C", L"growingPlant_shroom_7_C",
    L"growingPlant_shroom_funny_C", L"growingPlant_stolasHerb_C"};
bool Name(const std::wstring& s) {
    if (s.size() > 128) return false;
    for (wchar_t c : s) if (c < 32 || c > 126) return false;
    return true;
}
bool Empty(const std::wstring& s) { return s.empty() || s == L"None"; }
}
Kind ClassKind(const std::wstring& name) {
    if (name == L"llamaSoul_C") return Kind::Marker;
    if (name == L"dirthole_C") return Kind::DirtHole;
    if (name == L"faucet_C") return Kind::Faucet;
    if (name == L"groundHose_C") return Kind::Hose;
    if (name == L"customWall_unfinished_C") return Kind::WallOutline;
    if (name == L"customWall_C" || name == L"customWall_rad_C" || name == L"customWall_wetConcrete_C") return Kind::Wall;
    for (auto p : plants) if (name == p) return Kind::Plant;
    return Kind::None;
}
bool Normalize(const SR::SaveRecord& in, SR::SaveRecord& out) {
    const Kind kind = ClassKind(in.className);
    if (kind == Kind::Marker) return llama_marker::Normalize(in,out);
    if (kind == Kind::None || Empty(in.key) || !Name(in.key)) return false;
    for (float v : in.xform) if (!std::isfinite(v)) return false;
    float q = 0; for (int i = 0; i < 4; ++i) q += in.xform[i] * in.xform[i];
    if (q < .9f || q > 1.1f) return false;
    for (int i = 4; i < 7; ++i) if (std::fabs(in.xform[i]) > 2'000'000.f) return false;
    for (int i = 7; i < 10; ++i) if (in.xform[i] < .01f || in.xform[i] > 100.f) return false;
    SR::SaveRecord r; r.className = in.className; r.key = in.key; r.xform = in.xform;
    if (kind == Kind::DirtHole) {
        if (in.ints.size() != 1 || in.ints[0].size() != 1 || in.ints[0][0] < 0 || in.ints[0][0] > 2) return false;
        r.ints = in.ints;
        out = std::move(r); return true;
    }
    if (IsWall(kind)) {
        const bool outline = kind == Kind::WallOutline;
        const size_t count = outline ? 4 : 2;
        if (in.ints.empty() || in.ints[0].size() != count || in.ints[0].back() != -1 ||
            in.strings.empty() || in.strings[0].size() != 1 || !Name(in.strings[0][0])) return false;
        const auto shape = in.ints[0][outline ? 2 : 0];
        if (shape < 0 || shape > 3) return false;
        if (outline) {
            if (in.ints[0][0] < 1 || in.ints[0][0] > 1000000 ||
                in.ints[0][1] < 0 || in.ints[0][1] > 1000000) return false;
        } else {
            if (in.strings[0][0].empty() || in.floats.empty()) return false;
            if (in.className == L"customWall_wetConcrete_C") {
                if (!wall_drying::Valid(in.floats[0])) return false;
            } else if (in.floats[0].size() != 2) return false;
            for (float f : in.floats[0]) if (!std::isfinite(f) || f < 0 || f > 1000000.f) return false;
            if (in.floats[0][1] <= 0) return false;
            r.floats = {in.floats[0]};
        }
        r.ints = {in.ints[0]}; r.strings = {in.strings[0]};
        out = std::move(r); return true;
    }
    const size_t ns = kind == Kind::Plant ? 1 : 0;
    const size_t nc = kind == Kind::Faucet ? 1 : 2;
    if (in.names.size() <= ns || in.names[ns].size() != nc) return false;
    for (const auto& s : in.names[ns]) if (!Name(s)) return false;
    r.names.resize(ns + 1); r.names[ns] = in.names[ns];
    if (kind == Kind::Faucet) {
        if (in.bools.empty() || in.bools[0].size() != 1 || in.bools[0][0] > 1) return false;
        r.bools = {in.bools[0]};
    } else if (kind == Kind::Hose) {
        if (in.vectors.empty() || in.vectors[0].size() < 2 || in.vectors[0].size() > 4096 ||
            Empty(in.names[0][0])) return false;
        for (const auto& v : in.vectors[0]) for (float f : v)
            if (!std::isfinite(f) || std::fabs(f) > 2'000'000.f) return false;
        r.vectors = {in.vectors[0]};
    } else {
        if (in.floats.size() < 2 || in.floats[1].size() != 6 || Empty(in.names[1][0])) return false;
        for (size_t i = 0; i < 6; ++i) {
            const float f = in.floats[1][i];
            // Native Tick subtracts a whole delta from positive water/fertilizer,
            // so either can cross zero. turnDead writes the dryness sentinel -1.
            const float minimum = (i == 1 || i == 2) ? -1'000'000.f : i == 5 ? -1.f : 0.f;
            if (!std::isfinite(f) || f < minimum || f > 1'000'000.f) return false;
        }
        r.floats.resize(2); r.floats[1] = in.floats[1];
    }
    out = std::move(r); return true;
}
}
