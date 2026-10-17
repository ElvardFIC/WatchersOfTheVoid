// coop/player/puppet_identity.cpp -- see coop/player/puppet_identity.h.

#include "coop/player/puppet_identity.h"

#include "puppet_identity_internal.h"  // the rows and seams shared with the scan file (src-local)

#include "coop/player/players_registry.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"  // FindParamOffset
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <cstdint>

namespace coop::puppet_identity {
namespace {

namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace OI = ue_wrap::object_index;
namespace PL = coop::players;

// Every stub below is a generated delegate handler that carries its toucher as a parameter named OtherActor,
// read by name. wisp_o and wisp_p, dream_climb and dream_jump, and the three Box stubs share a name across
// classes, which a Declared row tells apart.
UE_FACT_FN(kRowPuppetWispW, "puppet_identity", L"wisp_w_C",
           L"BndEvt__wisp_red_Sphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetWispO, "puppet_identity", L"wisp_o_C",
           L"BndEvt__wisp_p_Sphere1_K2Node_ComponentBoundEvent_3_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetWispP, "puppet_identity", L"wisp_p_C",
           L"BndEvt__wisp_p_Sphere1_K2Node_ComponentBoundEvent_3_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetKelura, "puppet_identity", L"kelura_C",
           L"BndEvt__kelura_Sphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetOrborb, "puppet_identity", L"npc_orborb_C",
           L"BndEvt__blackBall_Sphere1_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetWellTpBegin, "puppet_identity", L"wellTp_C",
           L"BndEvt__wellTp_Sphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetWellTpEnd, "puppet_identity", L"wellTp_C",
           L"BndEvt__wellTp_Sphere_K2Node_ComponentBoundEvent_1_ComponentEndOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetDreamBoulder, "puppet_identity", L"dreamBoulder_C",
           L"BndEvt__dreamBoulder_StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetMapTransition, "puppet_identity", L"trigger_box_openMapTransition_C",
           L"BndEvt__Box_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature", Declared);
UE_FACT_FN(kRowPuppetBoxMark, "puppet_identity", L"trigger_box_C",
           L"BndEvt__Box_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature", Declared);
UE_FACT_FN(kRowPuppetRunTriggerOpenLevel, "puppet_identity", L"trigger_openLevel_C", L"runTrigger", Declared);
UE_FACT_FN(kRowPuppetRunTriggerSpikes, "puppet_identity", L"tutorWall_spikes_C", L"runTrigger", Declared);
UE_FACT_FN(kRowPuppetDreamClimb, "puppet_identity", L"dream_climb_C",
           L"BndEvt__dream_climb_Box_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetDreamJump, "puppet_identity", L"dream_jump_C",
           L"BndEvt__dream_climb_Box_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetDreamRunFin, "puppet_identity", L"dream_run_C",
           L"BndEvt__fin_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature", Declared);
UE_FACT_FN(kRowPuppetDreamRunWok, "puppet_identity", L"dream_run_C",
           L"BndEvt__wok_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature", Declared);
UE_FACT_FN(kRowPuppetDreamfillDoor, "puppet_identity", L"dreamfillDoor_C",
           L"BndEvt__Box_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature", Declared);
UE_FACT_FN(kRowPuppetGruntMark, "puppet_identity", L"prop_grunt_C",
           L"BndEvt__prop_grunt_damageSphere_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPuppetSkullzone, "puppet_identity", L"skullzone_C",
           L"BndEvt__dest_K2Node_ComponentBoundEvent_2_ComponentBeginOverlapSignature__DelegateSignature", Declared);

sg::Verdict OnWholePre(const sg::Call& call);
sg::Verdict OnBoxMarkPre(const sg::Call& call);
sg::Verdict OnRunTriggerPre(const sg::Call& call);
sg::Verdict OnGruntPre(const sg::Call& call);
void OnGruntPost(const sg::Call& call);

// One name watch. Whole: the stub is cancelled for a puppet toucher. BoxMark and GruntMark: the stub marks its
// scope for a puppet toucher (the grunt's also opens the SetControlRotation window). RunTrigger: the body is
// cancelled while the box stub's mark is on the chain.
enum class Kind : uint8_t { Whole, BoxMark, RunTrigger, GruntMark };

struct Watch {
    const ue_wrap::facts::Function* row;
    Kind                            kind;
    Row                             counter;  // kRowCount: the marking stub, which refuses nothing itself
    sg::PreFn                       pre;
    sg::PostFn                      post;
};

const Watch kWatches[] = {
    {&kRowPuppetWispW, Kind::Whole, kWispW, &OnWholePre, nullptr},
    {&kRowPuppetWispO, Kind::Whole, kWispO, &OnWholePre, nullptr},
    {&kRowPuppetWispP, Kind::Whole, kWispP, &OnWholePre, nullptr},
    {&kRowPuppetKelura, Kind::Whole, kKelura, &OnWholePre, nullptr},
    {&kRowPuppetOrborb, Kind::Whole, kOrborb, &OnWholePre, nullptr},
    {&kRowPuppetWellTpBegin, Kind::Whole, kWellTpBegin, &OnWholePre, nullptr},
    {&kRowPuppetWellTpEnd, Kind::Whole, kWellTpEnd, &OnWholePre, nullptr},
    {&kRowPuppetDreamBoulder, Kind::Whole, kDreamBoulder, &OnWholePre, nullptr},
    {&kRowPuppetMapTransition, Kind::Whole, kMapTransition, &OnWholePre, nullptr},
    {&kRowPuppetBoxMark, Kind::BoxMark, kRowCount, &OnBoxMarkPre, nullptr},
    {&kRowPuppetRunTriggerOpenLevel, Kind::RunTrigger, kTriggerOpenLevel, &OnRunTriggerPre, nullptr},
    {&kRowPuppetRunTriggerSpikes, Kind::RunTrigger, kTriggerSpikes, &OnRunTriggerPre, nullptr},
    {&kRowPuppetDreamClimb, Kind::Whole, kDreamClimb, &OnWholePre, nullptr},
    {&kRowPuppetDreamJump, Kind::Whole, kDreamJump, &OnWholePre, nullptr},
    {&kRowPuppetDreamRunFin, Kind::Whole, kDreamRunFin, &OnWholePre, nullptr},
    {&kRowPuppetDreamRunWok, Kind::Whole, kDreamRunWok, &OnWholePre, nullptr},
    {&kRowPuppetDreamfillDoor, Kind::Whole, kDreamfillDoor, &OnWholePre, nullptr},
    {&kRowPuppetGruntMark, Kind::GruntMark, kRowCount, &OnGruntPre, &OnGruntPost},
    {&kRowPuppetSkullzone, Kind::Whole, kSkullzone, &OnWholePre, nullptr},
};
constexpr int kWatchCount = static_cast<int>(sizeof(kWatches) / sizeof(kWatches[0]));
static_assert(kWatchCount == 19, "nineteen name watches: seventeen stubs and two runTrigger bodies");

constexpr int kPiTagBase = 0x50490000;  // 'PI' and the watch's index

enum class Reg : uint8_t { Pending, Registered, Refused };
Reg  g_reg[kWatchCount] = {};
bool g_watched = false;
bool g_settled = false;
int  g_live = 0;

RowCounter g_counters[kRowCount] = {
    {"wisp_w", 0}, {"wisp_o", 0}, {"wisp_p", 0}, {"kelura", 0}, {"orborb", 0}, {"wellTp.begin", 0},
    {"wellTp.end", 0}, {"dreamBoulder", 0}, {"mapTransition", 0}, {"trigger.openLevel", 0}, {"trigger.spikes", 0},
    {"dream_climb", 0}, {"dream_jump", 0}, {"dream_run.fin", 0}, {"dream_run.wok", 0}, {"dreamfillDoor", 0},
    {"grunt", 0}, {"shower", 0}, {"birch", 0}, {"erie", 0}, {"skullzone", 0},
};
uint32_t g_logged = 0;  // a bit per row: said since the last resolve

// Each stub's OtherActor parameter, found by name in the stub's own frame at its first call: kUnread until then,
// -1 when the stub has none, said once.
constexpr int32_t kUnread = INT32_MIN;
int32_t g_actorOff[kWatchCount] = {};

// The scope mark rows 9 and 11 put on a puppet's touch and read downstream. Its address is the owner.
int       g_markOwner = 0;
uintptr_t g_bit = 0;

// The registered puppets, from the registry's listener.
int g_count = 0;

// What the last resolve described: the two marking stubs' classes and functions, and the class-set number. A
// callback that reads a stub pointer first checks the number still stands (a class unloaded since frees them).
constexpr uint64_t kNoVersion = UINT64_MAX;
uint64_t g_resolvedVer = kNoVersion;
void*    g_boxCls = nullptr;
void*    g_gruntCls = nullptr;
void*    g_boxStub = nullptr;
void*    g_gruntStub = nullptr;
int      g_gen = 0;  // moves with each resolve that looked at anything

bool WorldCurrent() { return OI::ClassUnlistVersion() == g_resolvedVer; }

// The stub's toucher, or null.
void* Toucher(int index, const sg::Call& call) {
    int32_t& off = g_actorOff[index];
    if (off == kUnread) {
        off = R::FindParamOffset(call.function, L"OtherActor");
        if (off < 0)
            UE_LOGE("[PI] %ls::%ls has no OtherActor parameter -- this stub refuses nothing",
                    kWatches[index].row->Class(), kWatches[index].row->Name());
    }
    if (off < 0 || !call.locals) return nullptr;
    return *reinterpret_cast<void* const*>(call.locals + off);
}

// Is the marked scope of the box stub on the chain: the world still described, the stub resolved and a puppet
// toucher marked it.
bool InScope(void* stub) {
    return g_bit && stub && WorldCurrent() && (sg::ScopeBits(stub) & g_bit) != 0;
}

sg::Verdict OnWholePre(const sg::Call& call) {
    const int index = call.tag - kPiTagBase;
    if (index < 0 || index >= kWatchCount) return sg::Verdict::Run;
    void* actor = Toucher(index, call);
    if (!actor || !PL::Registry::Get().IsPuppetBody(actor)) return sg::Verdict::Run;
    NoteRefused(kWatches[index].counter, actor);
    return sg::Verdict::Cancel;
}

sg::Verdict OnBoxMarkPre(const sg::Call& call) {
    const int index = call.tag - kPiTagBase;
    if (!g_bit || index < 0 || index >= kWatchCount) return sg::Verdict::Run;
    void* actor = Toucher(index, call);
    if (actor && PL::Registry::Get().IsPuppetBody(actor)) sg::MarkScope(call, g_bit);
    return sg::Verdict::Run;
}

// The two targets of the box that leave the level or the game. The box's other targets run on.
sg::Verdict OnRunTriggerPre(const sg::Call& call) {
    if (!InScope(g_boxStub)) return sg::Verdict::Run;
    const int index = call.tag - kPiTagBase;
    if (index < 0 || index >= kWatchCount) return sg::Verdict::Run;
    NoteRefused(kWatches[index].counter, nullptr);
    return sg::Verdict::Cancel;
}

sg::Verdict OnGruntPre(const sg::Call& call) {
    if (!g_bit || !g_gruntStub || !WorldCurrent()) return sg::Verdict::Run;
    void* actor = Toucher(call.tag - kPiTagBase, call);
    if (!actor || !PL::Registry::Get().IsPuppetBody(actor)) return sg::Verdict::Run;
    sg::MarkScope(call, g_bit);
    scan::ArmGruntWindow(true);
    return sg::Verdict::Run;
}

// The window closes when no other grunt body is still open on the chain.
void OnGruntPost(const sg::Call&) {
    if (!WorldCurrent()) return;
    if (!sg::IsBodyActive(g_gruntStub)) scan::ArmGruntWindow(false);
}

void OnPuppetCount(int count) {
    g_count = count;
    scan::Rearm(count);
}

void Settle() {
    sg::ResolvePendingNames();
    int live = 0, settled = 0;
    for (int i = 0; i < kWatchCount; ++i) {
        if (g_reg[i] == Reg::Refused) { ++settled; continue; }
        if (sg::NameWatchLive(*kWatches[i].row, kPiTagBase + i)) { ++live; ++settled; continue; }
        if (sg::NameWatchSettled(*kWatches[i].row, kPiTagBase + i)) ++settled;
    }
    g_live = live;
    if (settled < kWatchCount) return;
    g_settled = true;
    UE_LOGI("[PI] watches live=%d unresolved=%d", WatchesLive(), kWatchCount - WatchesLive());
    for (int i = 0; i < kWatchCount; ++i)
        if (g_reg[i] == Reg::Refused || !sg::NameWatchLive(*kWatches[i].row, kPiTagBase + i))
            UE_LOGE("[PI] %ls::%ls is unwatched -- a puppet's touch there acts on this machine's player",
                    kWatches[i].row->Class(), kWatches[i].row->Name());
}

// A marking stub's function on its class, null when the class lacks it (said once per resolve).
void* StubOn(const ue_wrap::facts::Function& row, void* cls) {
    if (!cls) return nullptr;
    void* fn = ue_wrap::facts::ResolveOn(row, cls).Fn();
    if (!fn)
        UE_LOGE("[PI] %ls has no %ls -- the rows that read its mark refuse nothing in this world", row.Class(),
                row.Name());
    return fn;
}

// The per-world resolve, when a class pointer or the class-set number moved since the last one.
void ResolveWorld() {
    const uint64_t ver = OI::ClassUnlistVersion();
    void* const box = OI::ClassByName(L"trigger_box_C");
    void* const grunt = OI::ClassByName(L"prop_grunt_C");
    const bool stubsMoved = ver != g_resolvedVer || box != g_boxCls || grunt != g_gruntCls;
    const bool scanMoved = scan::Resolve(ver);
    if (!stubsMoved && !scanMoved) return;
    if (stubsMoved) {
        g_boxCls = box;
        g_gruntCls = grunt;
        g_boxStub = StubOn(kRowPuppetBoxMark, box);
        g_gruntStub = StubOn(kRowPuppetGruntMark, grunt);
    }
    g_resolvedVer = ver;
    ++g_gen;
    g_logged = 0;
}

}  // namespace

bool IsPuppetBodyFn(void*, void* actor) { return PL::Registry::Get().IsPuppetBody(actor); }

void NoteRefused(Row row, void* actor) {
    if (row >= kRowCount) return;
    ++g_counters[row].refused;
    const uint32_t bit = uint32_t{1} << row;
    if (g_logged & bit) return;
    g_logged |= bit;
    const uint8_t slot = actor ? PL::Registry::Get().PeerIdOfBody(actor) : PL::kPeerIdUnknown;
    if (slot == PL::kPeerIdUnknown)
        UE_LOGI("[PI] refused %s toucher=puppet slot=unknown", g_counters[row].row);
    else
        UE_LOGI("[PI] refused %s toucher=puppet slot=%u", g_counters[row].row, static_cast<unsigned>(slot));
}

bool InGruntScope() { return InScope(g_gruntStub); }

void Install() {
    if (!g_watched) {
        g_watched = true;
        for (int32_t& off : g_actorOff) off = kUnread;
        g_bit = sg::ScopeBitFor(&g_markOwner);
        if (!g_bit)
            UE_LOGE("[PI] no scope-mark bit is free -- trigger.openLevel, trigger.spikes and grunt refuse nothing");
        for (int i = 0; i < kWatchCount; ++i)
            g_reg[i] = sg::WatchName(*kWatches[i].row, kPiTagBase + i, kWatches[i].pre, kWatches[i].post)
                           ? Reg::Registered : Reg::Refused;
        PL::Registry::Get().SetPuppetCountListener(&OnPuppetCount);
    }
    scan::InstallPump(g_gen, g_count);
}

void Tick() {
    if (!g_watched) return;
    if (!g_settled) Settle();
    ResolveWorld();
    // A window a Cancel or a fault left open: the pump runs outside every body, so any armed window is one.
    if (scan::GruntWindowArmed() && !sg::IsBodyActive(g_gruntStub)) scan::ArmGruntWindow(false);
}

int WatchesLive() { return g_live; }

const RowCounter* Counters(int& n) {
    n = kRowCount;
    return g_counters;
}

void OnDisconnect() {
    int rows = 0;
    const RowCounter* counters = Counters(rows);
    for (int i = 0; i < rows; ++i)
        if (counters[i].refused)
            UE_LOGI("[PI] session end -- %s refused %u puppet touch(es) since the process started", counters[i].row,
                    static_cast<unsigned>(counters[i].refused));
    g_boxCls = g_gruntCls = nullptr;
    g_boxStub = g_gruntStub = nullptr;
    g_resolvedVer = kNoVersion;
    g_logged = 0;
    scan::Clear();
}

}  // namespace coop::puppet_identity
