// coop/world/spawn_authority.cpp -- see the header. The rows rest on these bytecode facts: the
// mushroom master arms one looping spawn timer at begin-play, and its spawn mints spawner children
// with a lifespan, so refused children are reaped by the engine independently of the refused event;
// the mushroom spawner's own looping timer materialises the food cap and self-destroys, and with
// spawn refused the lifespan reaps it; the yellow-wisp ticker spawns at a navmesh random-walk point,
// not around the player, and its product is host-mirrored, so the client must not run its own
// spawner; the sky-wisp ticker spawns the sky wisps at absolute map coordinates, so the host rolls
// and clients mirror through the source-gated catch and the variant allowlist; the roach master's
// summon and its three looping timer entries fire independently of its tick. The tick rows: the
// fossilhound ticker rolls inside its tick with no delay chains or reap duties, so
// refusing the tick stops the roll and the product, and the product is host-mirrored; the roach
// master's tick drives roach movement, the food-eat mutation and crush traces, which a client running
// it would diverge, so it and its summoner are refused while the roach sync drives the client
// population. The jellyfish path's spawn makes seven fish inside its graph, and the host's are
// mirrored through the source-gated catch, so a client's own, its 18:00 roll's, is refused.

#include "coop/world/spawn_authority.h"

#include "coop/net/session.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cstdint>
#include <iterator>

namespace coop::spawn_authority {
namespace {

namespace SG = ue_wrap::script_gate;

std::atomic<coop::net::Session*> g_session{nullptr};

// Refuse only while an active client session exists: the running flag flips true in the session
// start and false in the stop, which every disconnect path reaches; a bare role gate would bleed the
// refusal into single player after the session.
bool IsActiveClientSession() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() && s->role() == coop::net::Role::Client;
}

// The facts rows: the body a client must not run, named by its class and its function. The function
// is spelled as the live header dump spells it; names compare without case.
UE_FACT_FN(kRowSpawnMushroomMaster, "spawn_authority", L"mushroomMaster_C", L"Spawn", Declared);
UE_FACT_FN(kRowSpawnMushroomSpawner, "spawn_authority", L"mushroomSpawner_C", L"Spawn", Declared);
UE_FACT_FN(kRowReceiveTickTickerYellowWispSpawner, "spawn_authority", L"ticker_yellowWispSpawner_C",
           L"ReceiveTick", Declared);
UE_FACT_FN(kRowReceiveTickTickerWispSpawner, "spawn_authority", L"ticker_wispSpawner_C", L"ReceiveTick",
           Declared);
UE_FACT_FN(kRowReceiveTickTickerEgSpawner, "spawn_authority", L"ticker_egSpawner_C", L"ReceiveTick", Declared);
UE_FACT_FN(kRowSpawnJellyfishPath, "spawn_authority", L"jellyfishPath_C", L"spawn", Declared);
UE_FACT_FN(kRowSummonRoachCockroachMaster, "spawn_authority", L"cockroachMaster_C", L"summonRoach", Declared);
UE_FACT_FN(kRowAddRoachTimerCockroachMaster, "spawn_authority", L"cockroachMaster_C", L"addRoachTimer",
           Declared);
UE_FACT_FN(kRowSpawnNestTimerCockroachMaster, "spawn_authority", L"cockroachMaster_C", L"spawnNestTimer",
           Declared);
UE_FACT_FN(kRowCustomEventCockroachMaster, "spawn_authority", L"cockroachMaster_C", L"CustomEvent", Declared);
UE_FACT_FN(kRowReceiveTickTickerFossilhoundSpawner, "spawn_authority", L"ticker_fossilhoundSpawner_C",
           L"ReceiveTick", Declared);
UE_FACT_FN(kRowReceiveTickCockroachMaster, "spawn_authority", L"cockroachMaster_C", L"ReceiveTick", Declared);
UE_FACT_FN(kRowReceiveTickTickerRoachSummoner, "spawn_authority", L"ticker_roachSummoner_C", L"ReceiveTick",
           Declared);
UE_FACT_FN(kRowReceiveBeginPlaySuperEgger, "spawn_authority", L"superEgger_C", L"ReceiveBeginPlay", Declared);
UE_FACT_FN(kRowReceiveBeginPlayKavotiaPatrolController, "spawn_authority", L"kavotiaPatrolController_C",
           L"ReceiveBeginPlay", Declared);

// One row: the facts row a client must not run, and the tag its refusal log carries.
struct Row {
    const ue_wrap::facts::Function* row;
    const wchar_t* className;
    const wchar_t* functionName;
    const char* tag;
};
constexpr Row kRows[] = {
    {&kRowSpawnMushroomMaster, nullptr, nullptr, "mushroomMaster.Spawn"},
    {&kRowSpawnMushroomSpawner, nullptr, nullptr, "mushroomSpawner.Spawn"},
    // A late-game class: keyed by name, the watch holds from the class's first body, whenever it loads.
    {&kRowReceiveTickTickerYellowWispSpawner, nullptr, nullptr, "yellowWispSpawner.ReceiveTick"},
    // Sky wisps: world-anchored, so the host rolls; the source-gated catch and the variant
    // allowlist mirror the products.
    {&kRowReceiveTickTickerWispSpawner, nullptr, nullptr, "wispSpawner.ReceiveTick"},
    // The night-egg ticker: the same world-anchored shape -- its tick re-arms a 60-300 s
    // interval and rolls a weighted night spawn of one eg_C at random absolute map coordinates,
    // writing gamemode->eg (ticker_egSpawner_C UG:6-44). The host rolls, the catch mirrors the
    // egg; a client's own tick would mint a second, unsynced egg.
    {&kRowReceiveTickTickerEgSpawner, nullptr, nullptr, "egSpawner.ReceiveTick"},
    // The space jellyfish: the host's run is mirrored through the source-gated catch.
    {&kRowSpawnJellyfishPath, nullptr, nullptr, "jellyfishPath.spawn"},
    // The roach sim's entries: the ticker's cross-object call and the three looping timer
    // delegates. The roach sync drives the client population instead.
    {&kRowSummonRoachCockroachMaster, nullptr, nullptr, "cockroachMaster.summonRoach"},
    {&kRowAddRoachTimerCockroachMaster, nullptr, nullptr, "cockroachMaster.addRoachTimer"},
    {&kRowSpawnNestTimerCockroachMaster, nullptr, nullptr, "cockroachMaster.spawnNestTimer"},
    {&kRowCustomEventCockroachMaster, nullptr, nullptr, "cockroachMaster.CustomEvent"},
    // The ticks.
    // Insomniac is personal to the sleep-deprived player. Its spawn location,
    // pursuit, camera grab and dream entry all use the local player, so its
    // ticker must run on clients too (and its products stay off the NPC lane).
    {&kRowReceiveTickTickerFossilhoundSpawner, nullptr, nullptr, "fossilhoundSpawner.ReceiveTick"},
    {&kRowReceiveTickCockroachMaster, nullptr, nullptr, "cockroachMaster.ReceiveTick"},
    {&kRowReceiveTickTickerRoachSummoner, nullptr, nullptr, "roachSummoner.ReceiveTick"},
    // The event actors the world-actor lane mirrors whose own begin-play spawns the event's content
    // (the follower's balls, the egger's eggs, the cleaner's sponge, a dropper's cargo, the deployed
    // tank's gun): on a client each is a pose-driven mirror, and its begin-play would spawn a local
    // copy of what the host's creature and world-actor lanes deliver. The droppers' base body serves
    // the body and car drops; the pig and tank drops override it with bodies of their own.
    {nullptr, L"tentacleBallsFollower_C", L"ReceiveBeginPlay", "tentacleBallsFollower.ReceiveBeginPlay"},
    {&kRowReceiveBeginPlaySuperEgger, nullptr, nullptr, "superEgger.ReceiveBeginPlay"},
    {nullptr, L"soltomiaCleaning_C", L"ReceiveBeginPlay", "soltomiaCleaning.ReceiveBeginPlay"},
    {nullptr, L"ufoDropper_C", L"ReceiveBeginPlay", "ufoDropper.ReceiveBeginPlay"},
    {nullptr, L"ufoDropper_pig_C", L"ReceiveBeginPlay", "ufoDropper_pig.ReceiveBeginPlay"},
    {nullptr, L"ufoDropper_tank_C", L"ReceiveBeginPlay", "ufoDropper_tank.ReceiveBeginPlay"},
    {nullptr, L"firetankDeploy_C", L"ReceiveBeginPlay", "firetankDeploy.ReceiveBeginPlay"},
    // This placed controller creates both kavotia_C and igetis_C in its BeginPlay graph. The
    // host's EX-spawn catch above owns both products; a client run would mint an uncaught pair.
    {&kRowReceiveBeginPlayKavotiaPatrolController, nullptr, nullptr,
     "kavotiaPatrolController.ReceiveBeginPlay"},
    // Mimics are prop_impostor, with a props-table disguise chosen BEFORE finish.
    // The ordinary keyed prop lane preserves both the real class and that row.
    {nullptr, L"impostorPropsMaster_C", L"gen", "impostorPropsMaster.gen"},
    // Its tick only culls the actor when unseen and far from the LOCAL camera.
    // A mirror must survive the client's view; host PropDestroy ends its lifetime.
    // Keep BeginPlay/init intact: they install the disguise and flesh material.
    {nullptr, L"prop_impostor_C", L"ReceiveTick", "prop_impostor.ReceiveTick"},
    // Replicas paint lever/door state but never consume or ignite contents.
    {nullptr, L"cremator_C", L"collectObjects", "cremator.collectObjects"},
    {nullptr, L"cremator_C", L"igniteInside", "cremator.igniteInside"},
    {nullptr, L"cremator_C", L"removeTrash", "cremator.removeTrash"},
    {nullptr, L"cremator_C", L"spitFireballs_timer", "cremator.spitFireballs_timer"},
};
constexpr int kRowCount = static_cast<int>(std::size(kRows));
constexpr int kTagBase = 0x53410000;   // 'SA', then the row

// Refusals per row, for the log: the first and then each power of two, so a refused tick proves itself
// without a line a second. Game thread, as the gate's callbacks are.
std::uint64_t g_refused[kRowCount] = {};

SG::Verdict OnRowPre(const SG::Call& call) {
    if (!IsActiveClientSession()) return SG::Verdict::Run;
    const int row = call.tag - kTagBase;
    if (row < 0 || row >= kRowCount) return SG::Verdict::Run;
    const std::uint64_t n = ++g_refused[row];
    if ((n & (n - 1)) == 0)
        UE_LOGI("spawn_authority[%s]: client-refuse %p (call #%llu)", kRows[row].tag, call.object,
                static_cast<unsigned long long>(n));
    return SG::Verdict::Cancel;
}

std::atomic<bool> g_watched{false};

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_watched.exchange(true, std::memory_order_acq_rel)) return;
    int watched = 0;
    for (int i = 0; i < kRowCount; ++i) {
        const bool registered = kRows[i].row
            ? SG::WatchName(*kRows[i].row, kTagBase + i, &OnRowPre, nullptr)
            : SG::WatchClassName(kRows[i].className, kRows[i].functionName, kTagBase + i, &OnRowPre, nullptr);
        if (registered) {
            ++watched;
        } else {
            UE_LOGE("spawn_authority: the watch on %ls::%ls did not register -- a client runs it",
                    kRows[i].row ? kRows[i].row->Class() : kRows[i].className,
                    kRows[i].row ? kRows[i].row->Name() : kRows[i].functionName);
        }
    }
    UE_LOGI("spawn_authority: %d/%d spawner watches registered by class and name, each live once the game "
            "thread resolves its names; refused only on an active client session", watched, kRowCount);
}

}  // namespace coop::spawn_authority
