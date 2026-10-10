// coop/world/power_puzzle.cpp -- see coop/world/power_puzzle.h.

#include "coop/world/power_puzzle.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/interactables/device_occupancy.h"  // who is inside a panel
#include "coop/net/session.h"
#include "coop/session/net_pump.h"  // IsInAnnouncedWorld: a client's own world load runs natively
#include "coop/world/power_grid.h"  // the shared sequence and the host's send of the rows

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/device_screen.h"  // the panel's claim key
#include "ue_wrap/devices/generator.h"
#include "ue_wrap/devices/generator_panel.h"
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>

namespace coop::power_puzzle {
namespace {

namespace GEN = ue_wrap::generator;
namespace GP  = ue_wrap::generator_panel;
namespace sg  = ue_wrap::script_gate;
using coop::net::PowerGridPayload;
using coop::net::PowerGridPuzzle;
using coop::net::kPowerGridGenerators;
using coop::net::kPowerPuzzleFields;
using coop::net::kPowerPuzzleFieldSwitches;
using coop::net::kPowerPuzzleFieldRotator0;

// A drag writes its value every frame; a field goes to the host at most this often, and its last value always.
constexpr uint64_t kInputEveryMs = 100;

std::atomic<coop::net::Session*> g_session{nullptr};

coop::net::Session* Connected() {
    auto* s = g_session.load(std::memory_order_acquire);
    return (s && s->connected()) ? s : nullptr;
}

bool IsClient(coop::net::Session* s) { return s && s->role() == coop::net::Role::Client; }

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// `a` is later than `b` in a 16-bit sequence that wraps.
bool SeqAfter(uint16_t a, uint16_t b) { return static_cast<int16_t>(static_cast<uint16_t>(a - b)) > 0; }

// ---- a puzzle's fields and its wire form ------------------------------------------------------------------------

uint8_t FieldOf(const GP::Puzzle& p, int f) {
    if (f < kPowerPuzzleFieldSwitches) return p.sine[f];
    if (f == kPowerPuzzleFieldSwitches) return p.switches;
    return p.rotators[f - kPowerPuzzleFieldRotator0];
}

void SetField(GP::Puzzle& p, int f, uint8_t v) {
    if (f < kPowerPuzzleFieldSwitches) p.sine[f] = v;
    else if (f == kPowerPuzzleFieldSwitches) p.switches = v;
    else p.rotators[f - kPowerPuzzleFieldRotator0] = v;
}

// The ranges the panel's own setters keep: the sines 0..15, a rotator's turn 0..3.
bool InRange(int f, uint8_t v) {
    if (f < kPowerPuzzleFieldSwitches) return v <= 15;
    if (f == kPowerPuzzleFieldSwitches) return true;
    return f < kPowerPuzzleFields && v <= 3;
}

void Pack(const GP::Puzzle& in, PowerGridPuzzle& out) {
    out = {};
    out.valid = 1;
    for (int k = 0; k < 3; ++k) {
        out.targetSine[k] = in.targetSine[k];
        out.sine[k] = in.sine[k];
    }
    out.switchesTarget = in.switchesTarget;
    out.switches = in.switches;
    for (int e = 0; e < GP::kRotators * 4; ++e) {
        const uint8_t c = static_cast<uint8_t>(in.colors[e / 4][e % 4] & 0x0F);
        out.colors[e / 2] = static_cast<uint8_t>(out.colors[e / 2] | ((e & 1) ? (c << 4) : c));
    }
    uint32_t turns = 0;
    for (int i = 0; i < GP::kRotators; ++i) turns |= static_cast<uint32_t>(in.rotators[i] & 3u) << (2 * i);
    for (int b = 0; b < 3; ++b) out.rotators[b] = static_cast<uint8_t>(turns >> (8 * b));
}

bool Unpack(const PowerGridPuzzle& in, GP::Puzzle& out) {
    if (!in.valid) return false;
    for (int k = 0; k < 3; ++k) {
        out.targetSine[k] = in.targetSine[k];
        out.sine[k] = in.sine[k];
    }
    out.switchesTarget = in.switchesTarget;
    out.switches = in.switches;
    for (int e = 0; e < GP::kRotators * 4; ++e)
        out.colors[e / 4][e % 4] = static_cast<uint8_t>((in.colors[e / 2] >> ((e & 1) ? 4 : 0)) & 0x0F);
    const uint32_t turns = in.rotators[0] | (in.rotators[1] << 8) | (in.rotators[2] << 16);
    for (int i = 0; i < GP::kRotators; ++i) out.rotators[i] = static_cast<uint8_t>((turns >> (2 * i)) & 3u);
    return true;
}

// ---- a client ------------------------------------------------------------------------------------------------

// My inputs the host has not taken, oldest first; the values a drag holds back; the host's last puzzles.
struct Input { uint16_t seq; uint8_t index; uint8_t field; uint8_t value; };
std::vector<Input> g_untaken;
struct Held { bool dirty = false; uint8_t value = 0; uint64_t sentMs = 0; };
Held g_held[kPowerGridGenerators][kPowerPuzzleFields];
int g_heldCount = 0;
PowerGridPuzzle g_canon[kPowerGridGenerators] = {};
// Whether each panel holds the canonical, written at least once: until then its values are its own init's roll,
// not a player's input, and a panel its own initiate has not sized yet is written on a later tick.
bool g_written[kPowerGridGenerators] = {};
uint64_t g_inputsSent = 0, g_rollsRefused = 0;
// Set while this lane writes a panel: what the event graph runs then is ours, not a player's input. A player's
// input made through our own call (the drill's) is still one, so the test is this scope, not fromOurCode.
bool g_writing = false;
// The panel the event graph last ran on and what it read then: while a player is inside, the graph runs every
// frame, and a frame that changed nothing costs one read and one compare.
void* g_lastPanel = nullptr;
GP::Puzzle g_lastRead{};

// The puzzle the host will hold once it takes my inputs: its canonical, my untaken inputs on top, then the
// values held back.
bool Model(int32_t index, GP::Puzzle& out) {
    if (!Unpack(g_canon[index], out)) return false;
    for (const Input& u : g_untaken)
        if (u.index == index) SetField(out, u.field, u.value);
    for (int f = 0; f < kPowerPuzzleFields; ++f)
        if (g_held[index][f].dirty) SetField(out, f, g_held[index][f].value);
    return true;
}

void Flush(bool all) {
    auto* s = Connected();
    if (!IsClient(s) || g_heldCount == 0) return;
    const uint64_t now = NowMs();
    for (int32_t idx = 0; idx < kPowerGridGenerators; ++idx) {
        for (int f = 0; f < kPowerPuzzleFields; ++f) {
            Held& h = g_held[idx][f];
            if (!h.dirty || (!all && now - h.sentMs < kInputEveryMs)) continue;
            PowerGridPayload p{};
            p.op = coop::net::kPowerGridOpPuzzle;
            p.index = static_cast<uint8_t>(idx);
            p.field = static_cast<uint8_t>(f);
            p.value = h.value;
            p.seq = coop::power_grid::NextSeq();
            s->SendReliableToSlot(0, coop::net::ReliableKind::PowerGridState, &p, sizeof(p));
            g_untaken.push_back({p.seq, p.index, p.field, p.value});
            h.dirty = false;
            h.sentMs = now;
            --g_heldCount;
            if (++g_inputsSent <= 3 || g_inputsSent % 100 == 0)
                UE_LOGI("power_puzzle: local input on generator %d, field %d = %u (seq %u, %llu sent) -- to the host",
                        idx, f, p.value, p.seq, static_cast<unsigned long long>(g_inputsSent));
        }
    }
}

bool SameValues(const GP::Puzzle& a, const GP::Puzzle& b) {
    for (int f = 0; f < kPowerPuzzleFields; ++f)
        if (FieldOf(a, f) != FieldOf(b, f)) return false;
    return true;
}

// A client's input: whatever its player's hand left on the panel that the model does not already say.
void CaptureInputs(void* panel) {
    GP::Puzzle cur;
    if (!GP::Read(panel, cur)) return;
    if (panel == g_lastPanel && SameValues(cur, g_lastRead)) return;
    g_lastPanel = panel;
    g_lastRead = cur;
    const int32_t idx = GEN::IndexOf(GP::GeneratorOf(panel));
    GP::Puzzle model;
    if (idx < 0 || idx >= kPowerGridGenerators || !g_written[idx] || !Model(idx, model)) return;
    for (int f = 0; f < kPowerPuzzleFields; ++f) {
        const uint8_t v = FieldOf(cur, f);
        if (v == FieldOf(model, f)) continue;
        Held& h = g_held[idx][f];
        if (!h.dirty) ++g_heldCount;
        h.dirty = true;
        h.value = v;
    }
    Flush(false);
}

// ---- the watches ---------------------------------------------------------------------------------------------

// The panel's event graph ran: every input a player makes runs inside it (the clicks, the drags, the scroll's
// addVal), and so does the end of each move. A client's own is an input; on the host any change sends the rows.
void OnPanelGraphPost(const sg::Call& call) {
    auto* s = Connected();
    if (!s || !call.object) return;
    if (s->role() == coop::net::Role::Host) {
        coop::power_grid::HostPuzzleChanged();
        return;
    }
    if (g_writing || !coop::net_pump::IsInAnnouncedWorld(call.object)) return;
    CaptureInputs(call.object);
}

// A roll: a client never makes one, since the targets are the host's world and its canonical carries them.
sg::Verdict OnRollPre(const sg::Call& call) {
    auto* s = Connected();
    if (!IsClient(s) || !call.object || !coop::net_pump::IsInAnnouncedWorld(call.object)) return sg::Verdict::Run;
    if (++g_rollsRefused == 1)
        UE_LOGI("power_puzzle: this client refuses its own puzzle rolls -- the host's rows carry every puzzle");
    return sg::Verdict::Cancel;
}

// On the host a roll outside the generators' verbs (the generator's own BeginPlay) sends the rows too.
void OnRollPost(const sg::Call& call) {
    auto* s = Connected();
    if (s && s->role() == coop::net::Role::Host && call.object) coop::power_grid::HostPuzzleChanged();
}

UE_FACT_FN(kRowUbergraphTransformerMgPanel, "power_puzzle", L"transformerMGPanel_C",
           L"ExecuteUbergraph_transformerMGPanel", Declared);
UE_FACT_FN(kRowRandomizeTargets, "power_puzzle", L"transformerMGPanel_C", L"randomizeTargets", Declared);
UE_FACT_FN(kRowRandomizeValues, "power_puzzle", L"transformerMGPanel_C", L"randomizeValues", Declared);
UE_FACT_FN(kRowRandomizeSines, "power_puzzle", L"transformerMGPanel_C", L"randomizeSines", Declared);

struct WatchDef {
    const ue_wrap::facts::Function* row;
    int tag;
    sg::PreFn pre;
    sg::PostFn post;
};
constexpr WatchDef kWatches[] = {
    { &kRowUbergraphTransformerMgPanel, 0x50505547 /*'PPUG'*/, nullptr,     &OnPanelGraphPost },
    { &kRowRandomizeTargets,            0x50505254 /*'PPRT'*/, &OnRollPre, &OnRollPost },
    { &kRowRandomizeValues,             0x50505256 /*'PPRV'*/, &OnRollPre, &OnRollPost },
    { &kRowRandomizeSines,              0x50505253 /*'PPRS'*/, &OnRollPre, &OnRollPost },
};
constexpr int kWatchCount = static_cast<int>(sizeof(kWatches) / sizeof(kWatches[0]));
bool g_registered = false;
bool g_settled = false;

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_registered) return;
    g_registered = true;
    for (const WatchDef& w : kWatches)
        if (!sg::WatchName(*w.row, w.tag, w.pre, w.post))
            UE_LOGE("power_puzzle: the script-body gate refused the watch on %ls::%ls -- that edge is each peer's own",
                    w.row->Class(), w.row->Name());
}

void Fill(void* gen, PowerGridPuzzle& out) {
    out = {};
    GP::Puzzle p;
    if (GP::Read(GP::PanelOf(gen), p)) Pack(p, out);
}

bool SamePuzzle(const PowerGridPuzzle& a, const PowerGridPuzzle& b) {
    return std::equal(reinterpret_cast<const uint8_t*>(&a), reinterpret_cast<const uint8_t*>(&a) + sizeof(a),
                      reinterpret_cast<const uint8_t*>(&b));
}

bool Solved(void* gen) { return GP::Solved(GP::PanelOf(gen)); }

bool Settling(void* gen) { return GP::IsMoving(GP::PanelOf(gen)); }

const char* HostTake(uint8_t sender, const PowerGridPayload& p, void* gen, bool broken, bool claimWaited,
                     bool& wait) {
    wait = false;
    void* panel = GP::PanelOf(gen);
    if (!panel) return "that generator has no panel";
    if (!InRange(p.field, p.value)) return "the value is outside its field's range";
    const std::wstring key = ue_wrap::device_screen::ClassifyDeviceActorClaimKey(panel);
    const uint8_t holder = key.empty() ? 0xFF : coop::device_occupancy::HolderOf(key.c_str());
    if (holder == 0xFF && !claimWaited) {
        wait = true;
        return nullptr;
    }
    if (holder == 0xFF) return "the sender is not inside the panel";
    if (holder != sender) return "another player is inside the panel";
    // The clicks' own gate (the panel's event graph @3757 and @4090): the knobs turn on a whole generator too.
    if (p.field >= kPowerPuzzleFieldSwitches && !broken)
        return "the generator is whole, and its switches and rotators turn only while it is broken";
    GP::Puzzle cur;
    if (!GP::Read(panel, cur)) return "the panel is not ready";
    SetField(cur, p.field, p.value);
    g_writing = true;
    GP::Write(panel, cur);
    g_writing = false;
    return nullptr;
}

void OnRows(const PowerGridPayload& p, uint16_t ack) {
    size_t kept = 0;
    for (const Input& u : g_untaken)
        if (SeqAfter(u.seq, ack)) g_untaken[kept++] = u;
    g_untaken.resize(kept);
    const int n = std::min<int>(p.count, kPowerGridGenerators);
    for (int i = 0; i < n; ++i)
        if (p.rows[i].present && p.rows[i].puzzle.valid) g_canon[i] = p.rows[i].puzzle;
}

// [dev] grid_drill=puzzlered: a client never writes the host's puzzle, as each peer kept its own before.
bool RedSkip() {
    static const bool red = coop::config::ResolveString(::coop::config_registry::rows::grid_drill) == "puzzlered";
    return red;
}

void Reconcile(const std::vector<void*>& gens) {
    if (RedSkip()) return;
    const size_t n = std::min(gens.size(), static_cast<size_t>(kPowerGridGenerators));
    g_writing = true;
    for (size_t i = 0; i < n; ++i) {
        void* panel = gens[i] ? GP::PanelOf(gens[i]) : nullptr;
        GP::Puzzle want;
        if (panel && Model(static_cast<int32_t>(i), want) && GP::Write(panel, want)) g_written[i] = true;
    }
    g_writing = false;
}

void FlushInputs() { Flush(true); }

void Tick() {
    Flush(false);
    // A canonical that came before its panel could be written (a joiner's rows ahead of the panel's initiate),
    // tried again each second: a panel that never sizes costs one try a second, not one a frame.
    static uint64_t s_nextTryMs = 0;
    for (int32_t i = 0; i < kPowerGridGenerators; ++i) {
        if (!g_canon[i].valid || g_written[i]) continue;
        const uint64_t now = NowMs();
        if (now < s_nextTryMs) break;
        s_nextTryMs = now + 1000;
        std::vector<void*> gens;
        if (GEN::ReadGenerators(gens) > 0) Reconcile(gens);
        break;
    }
    if (g_settled || !g_registered) return;
    sg::ResolvePendingNames();
    int live = 0, settled = 0;
    for (const WatchDef& w : kWatches) {
        if (sg::NameWatchLive(*w.row, w.tag)) ++live;
        if (sg::NameWatchSettled(*w.row, w.tag)) ++settled;
    }
    if (live == kWatchCount) {
        g_settled = true;
        UE_LOGI("power_puzzle: the puzzle's gates are live (the panel's inputs, its three rolls)");
    } else if (settled == kWatchCount) {
        g_settled = true;
        UE_LOGE("power_puzzle: %d of %d puzzle gates are dead -- those edges stay each peer's own",
                kWatchCount - live, kWatchCount);
    }
}

void OnDisconnect() {
    if (g_inputsSent || g_rollsRefused)
        UE_LOGI("power_puzzle: session end -- inputs sent %llu, own rolls refused %llu",
                static_cast<unsigned long long>(g_inputsSent), static_cast<unsigned long long>(g_rollsRefused));
    g_untaken.clear();
    for (auto& row : g_held)
        for (Held& h : row) h = Held{};
    g_heldCount = 0;
    for (PowerGridPuzzle& c : g_canon) c = {};
    for (bool& w : g_written) w = false;
    g_inputsSent = g_rollsRefused = 0;
    g_lastPanel = nullptr;
}

bool Canonical(int32_t index, PowerGridPuzzle& out) {
    if (index < 0 || index >= kPowerGridGenerators || !g_canon[index].valid) return false;
    out = g_canon[index];
    return true;
}

uint64_t InputsSent() { return g_inputsSent; }

size_t UntakenInputs() {
    size_t n = g_untaken.size();
    for (const auto& row : g_held)
        for (const Held& h : row) n += h.dirty ? 1 : 0;
    return n;
}

void DevFlood(int32_t index, int n) {
    auto* s = Connected();
    GP::Puzzle model;
    if (!IsClient(s) || index < 0 || index >= kPowerGridGenerators || !Model(index, model)) return;
    const uint8_t from = FieldOf(model, 0);
    for (int i = 0; i < n; ++i) {
        PowerGridPayload p{};
        p.op = coop::net::kPowerGridOpPuzzle;
        p.index = static_cast<uint8_t>(index);
        p.field = 0;
        p.value = static_cast<uint8_t>(i % 2 == 0 ? (from + 1) % 16 : from);
        p.seq = coop::power_grid::NextSeq();
        s->SendReliableToSlot(0, coop::net::ReliableKind::PowerGridState, &p, sizeof(p));
        g_untaken.push_back({p.seq, p.index, p.field, p.value});
        ++g_inputsSent;
    }
    UE_LOGI("power_puzzle: [dev] sent %d inputs on generator %d at once", n, index);
}

}  // namespace coop::power_puzzle
