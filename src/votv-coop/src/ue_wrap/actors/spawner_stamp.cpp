// ue_wrap/actors/spawner_stamp.cpp -- see ue_wrap/actors/spawner_stamp.h.

#include "ue_wrap/actors/spawner_stamp.h"

#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/facts/function_row.h"

#include <cstdint>
#include <cwchar>

namespace ue_wrap::spawner_stamp {
namespace {

namespace R  = reflection;
namespace OI = object_index;

// The two functions a row stands on, for the version census: the class's BeginPlay stub (its fingerprint is the
// guard's path) and the spawner's whole graph (its stub is not singled out, so it moves on any edit of the graph).
UE_FACT_FN(kRowOrborbBeginPlay, "spawner_stamp", L"npc_orborb_C", L"ReceiveBeginPlay", Declared);
UE_FACT_FN(kRowBlackballSpawner, "spawner_stamp", L"blackballSpawner_C", L"ExecuteUbergraph_blackballSpawner",
           Declared);

// One class's native spawner: the FString property it writes, the constant, and the functions that constant
// depends on. blackballSpawner_C's ubergraph writes NewVar_0 between the spawn's halves and npc_orborb_C's
// BeginPlay compares it, else ends the game.
struct Row {
    const wchar_t*                  cls;
    const wchar_t*                  prop;
    const wchar_t*                  value;
    const facts::Function* const    rows[2];
};
const Row kRows[] = {
    {L"npc_orborb_C", L"NewVar_0", L"O0ttQI_-jnVwHSztT-bb_w", {&kRowOrborbBeginPlay, &kRowBlackballSpawner}},
};
constexpr int kRowCount = static_cast<int>(sizeof kRows / sizeof kRows[0]);

// What a row learned about one listing of its class.
struct Entry {
    void*    cls = nullptr;
    uint64_t listing = 0;
    int32_t  off = -1;  // -1: the property is missing or is not an FString
    bool     saidStamped = false;
    bool     saidFailed = false;
};
Entry g_entries[kRowCount];
int   g_failures = 0;

int RowOf(void* cls) {
    const R::FName& name = R::NameOf(cls);
    for (int i = 0; i < kRowCount; ++i)
        if (R::NameEquals(name, kRows[i].cls)) return i;
    return -1;
}

// The property's offset when it is an FString, else -1. Names compare without case, as the engine's do.
int32_t StringOffset(const Row& row, void* cls) {
    const int32_t off = R::FindPropertyOffset(cls, row.prop);
    if (off < 0) return -1;
    return ::_wcsicmp(R::PropertyClassName(cls, row.prop).c_str(), L"StrProperty") == 0 ? off : -1;
}

}  // namespace

bool Stamp(void* actorClass, void* deferredActor) {
    UE_ASSERT_GAME_THREAD("spawner_stamp::Stamp");
    const int index = actorClass ? RowOf(actorClass) : -1;
    if (index < 0) return true;
    const Row& row = kRows[index];
    Entry& e = g_entries[index];
    // A class the index does not list yet (id 0) cannot be told from a reloaded one: it is resolved afresh.
    const uint64_t listing = OI::ClassListingId(actorClass);
    if (listing == 0 || e.cls != actorClass || e.listing != listing) {
        // Each row on its class through the object index (no array walk); a class not loaded here, as the spawner's
        // often is on a client, is left for the census to find.
        for (const facts::Function* fact : row.rows)
            if (void* cls = OI::ClassByName(fact->Class())) facts::ResolveOn(*fact, cls);
        e = Entry{actorClass, listing, StringOffset(row, actorClass), false, false};
    }
    if (deferredActor && e.off >= 0 && field_io::WriteFStringField(deferredActor, e.off, row.value)) {
        if (!e.saidStamped) {
            e.saidStamped = true;
            UE_LOGI("[NQ] stamped %ls %ls", row.cls, row.prop);
        }
        return true;
    }
    ++g_failures;
    if (!e.saidFailed) {
        e.saidFailed = true;
        UE_LOGE("[NQ] stamp failed %ls %ls", row.cls, row.prop);
    }
    return false;
}

int FailureCount() { return g_failures; }

}  // namespace ue_wrap::spawner_stamp
