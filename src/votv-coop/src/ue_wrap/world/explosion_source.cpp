// ue_wrap/world/explosion_source.cpp -- see ue_wrap/world/explosion_source.h.

#include "ue_wrap/world/explosion_source.h"

#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"

namespace ue_wrap::explosion_source {
namespace {

// The seven kinds that name a class, as indices 0..6 (Kind::None is 0 in the enum, so a kind's slot is its value
// less one).
constexpr int kCount = static_cast<int>(Kind::SoapGrenade);

void*    g_cls[kCount] = {};
uint64_t g_unlistVersion = ~0ull;

Kind At(int i) { return static_cast<Kind>(i + 1); }

// The index unlisted a class since the last call, so a pointer here may name a freed UClass: the pointers are
// cleared and looked up again.
void Sync() {
    const uint64_t version = object_index::ClassUnlistVersion();
    if (version == g_unlistVersion) return;
    g_unlistVersion = version;
    for (int i = 0; i < kCount; ++i) g_cls[i] = nullptr;
}

bool AllResolved() {
    for (int i = 0; i < kCount; ++i)
        if (!g_cls[i]) return false;
    return true;
}

}  // namespace

const wchar_t* ClassName(Kind k) {
    switch (k) {
        case Kind::Rocket:      return L"prop_rocket_C";
        case Kind::Sticky:      return L"prop_stickybomb_C";
        case Kind::Grenade:     return L"prop_grenade_C";
        case Kind::Pipebomb:    return L"prop_pipebomb_C";
        case Kind::Player:      return L"mainPlayer_C";
        case Kind::PoopGrenade: return L"prop_grenade_poop_C";
        case Kind::SoapGrenade: return L"prop_grenade_soap_C";
        case Kind::None:        break;
    }
    return nullptr;
}

void Resolve() {
    UE_ASSERT_GAME_THREAD("explosion_source::Resolve");
    Sync();
    if (AllResolved()) return;
    if (object_index::Backlog() != 0) return;
    for (int i = 0; i < kCount; ++i) {
        if (g_cls[i]) continue;
        if (void* cls = object_index::ClassByName(ClassName(At(i)))) {
            g_cls[i] = cls;
            UE_LOGI("[EXPLOSION] source class %ls resolved", ClassName(At(i)));
        }
    }
}

void Reset() {
    UE_ASSERT_GAME_THREAD("explosion_source::Reset");
    for (int i = 0; i < kCount; ++i) g_cls[i] = nullptr;
}

}  // namespace ue_wrap::explosion_source
