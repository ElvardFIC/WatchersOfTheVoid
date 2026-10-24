// coop/dev/atv_spawn_near.cpp -- see coop/dev/atv_spawn_near.h.

#include "atv_spawn_near.h"  // co-located private header (src tree, not include/)

#include "coop/interactables/atv_sync.h"

#include "ue_wrap/core/object_index.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_nav.h"

#include <cstdio>

namespace coop::dev::atv_spawn_near {

namespace E = ue_wrap::engine;

bool SpawnAtNav(void* world, const ue_wrap::FVector& point, Spawned& out) {
    void* cls = ue_wrap::object_index::ClassByName(L"ATV_C");
    if (!cls) {
        out.what = "the ATV's class is not loaded";
        out.want = "ATV_C";
        std::snprintf(out.got, sizeof(out.got), "no class");
        return false;
    }
    if (!E::ProjectToNav(world, point, {kProjectCm, kProjectCm, kProjectCm}, &out.onMesh)) {
        out.what = "the spawn point did not project to the navmesh";
        out.want = "a NavMesh point";
        out.noNav = true;
        std::snprintf(out.got, sizeof(out.got), "no NavMesh point within %.0f cm of (%.0f,%.0f,%.0f)", kProjectCm,
                      point.X, point.Y, point.Z);
        return false;
    }
    const ue_wrap::FVector loc{out.onMesh.X, out.onMesh.Y, out.onMesh.Z + kLiftCm};
    out.actor = E::SpawnActor(cls, loc);
    if (!out.actor) {
        out.what = "the ATV did not spawn";
        out.want = "an ATV";
        std::snprintf(out.got, sizeof(out.got), "no actor");
        return false;
    }
    return true;
}

Key KeyOf(void* atv, std::wstring* key, const char** why) {
    const std::wstring k = ue_wrap::atv::GetKeyString(atv);
    coop::atv_sync::TableView t;
    if (!coop::atv_sync::AnnounceableAtvKey(k) || !coop::atv_sync::ReadTable(k, t)) return Key::Wait;
    for (const wchar_t c : k) {
        if (c >= 128 || c == L' ') {
            *why = "a space or a non-ASCII char";
            return Key::Bad;
        }
    }
    *key = k;
    return Key::Ok;
}

}  // namespace coop::dev::atv_spawn_near
