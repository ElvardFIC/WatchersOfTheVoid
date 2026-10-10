// coop/interactables/meadow_db_writers.cpp -- see coop/interactables/meadow_db_writers.h.

#include "coop/interactables/meadow_db_writers.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/facts/function_row.h"

#include <iterator>

namespace coop::meadow_db_writers {
namespace {

namespace sg = ue_wrap::script_gate;

UE_FACT_FN(kRowAddSignal, "meadow_db_writers", L"ui_laptop_C", L"addSignal", Declared);
UE_FACT_FN(kRowRemoveSignal, "meadow_db_writers", L"ui_laptop_C", L"removeSignal", Declared);
UE_FACT_FN(kRowSortSignal, "meadow_db_writers", L"ui_laptop_C", L"sortSignal", Declared);
UE_FACT_FN(kRowUbergraphSignalName, "meadow_db_writers", L"ui_signalName_C", L"ExecuteUbergraph_ui_signalName",
           Declared);

struct Writer { const ue_wrap::facts::Function* row; };
const Writer kWriters[] = {
    {&kRowAddSignal},
    {&kRowRemoveSignal},
    {&kRowSortSignal},
    {&kRowUbergraphSignalName},
};
constexpr int kTagWriter = 0x4D445742;  // 'MDWB'
bool g_watched = false;  // registered, once a process

}  // namespace

bool Watch(sg::PreFn pre, sg::PostFn post) {
    if (g_watched) return true;
    bool ok = true;
    for (const Writer& w : kWriters)
        ok = sg::WatchName(*w.row, kTagWriter, pre, post) && ok;
    g_watched = ok;
    if (!ok) UE_LOGW("meadow_db: a writer's watch was refused -- a change it makes is not sent");
    return ok;
}

bool Settle() {
    sg::ResolvePendingNames();
    size_t live = 0;
    for (const Writer& w : kWriters)
        if (sg::NameWatchLive(*w.row, kTagWriter)) ++live;
    if (live < std::size(kWriters) && sg::PendingNameCount() > 0) return false;
    if (live == std::size(kWriters)) {
        UE_LOGI("meadow_db: the database's %zu writers are watched at the script-body gate", std::size(kWriters));
        return true;
    }
    for (const Writer& w : kWriters)
        if (!sg::NameWatchLive(*w.row, kTagWriter))
            UE_LOGW("meadow_db: the writer %ls::%ls is NOT watched -- a change it makes goes out only with the "
                    "next watched writer's", w.row->Class(), w.row->Name());
    return true;
}

}  // namespace coop::meadow_db_writers
