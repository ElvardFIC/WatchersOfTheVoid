// coop/world/fuse_tower_kind.cpp -- see coop/world/fuse_tower_kind.h.
//
// The radio tower's facts (bp_cpp radiotower_C, line numbers are its pseudo-C++): `scramble` (:1496) blows the first
// good fuse and broadcasts radioTowerBroken(tower, true), or else breaks the puzzle and broadcasts (tower, false);
// the reset lever's timeline event re-rolls through it (:2339); the confirm lever's forward end judges the puzzle
// through tryToFix (:2165); completeMinigame lights every cell and judges (:2185); actionOptionIndex and
// playerUsedOn are ignored while `animation` (:510, :655).

#include "coop/world/fuse_tower_kind.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/devices/coord_tower.h"
#include "ue_wrap/devices/radio_tower.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/game_rules.h"  // the difficulty an insert reads

#include <algorithm>
#include <cstdint>

namespace coop::fuse_tower_kind {
namespace {

namespace CT = ue_wrap::coord_tower;
namespace RT = ue_wrap::radio_tower;
using coop::net::FuseTowerRow;
using Edges = coop::fuse_tower_rows::Edges;

static_assert(CT::kMaxFuses <= coop::net::kFuseTowerMaxFuses && RT::kMaxFuses <= coop::net::kFuseTowerMaxFuses,
              "a row holds eight fuses");
static_assert(CT::kMaxLights <= coop::net::kFuseTowerMaxLights && RT::kMaxCells <= coop::net::kFuseTowerMaxLights,
              "a row holds the lights of the larger puzzle");
static_assert(coop::net::kFuseTowerMaxLights < 32, "a row's lights are one 32-bit mask");

// The four bodies each kind's gate watches. The coordinate dish's roll is one function, and its lever's judgement
// the timeline's end; the radio tower's roll is `scramble`, which its reset lever, its decay timer and a blast all
// reach, and its judgement the same timeline end.
UE_FACT_FN(kRowCoordRoll, "fuse_tower_kind", L"coordRadarDish_C", L"Scramble Radar Dish", Declared);
UE_FACT_FN(kRowCoordJudge, "fuse_tower_kind", L"coordRadarDish_C", L"leverTL__FinishedFunc", Declared);
UE_FACT_FN(kRowCoordUse, "fuse_tower_kind", L"coordRadarDish_C", L"actionOptionIndex", Declared);
UE_FACT_FN(kRowCoordInsert, "fuse_tower_kind", L"coordRadarDish_C", L"playerUsedOn", Declared);
UE_FACT_FN(kRowRadioRoll, "fuse_tower_kind", L"radiotower_C", L"scramble", Declared);
UE_FACT_FN(kRowRadioJudge, "fuse_tower_kind", L"radiotower_C", L"TL_lever__FinishedFunc", Declared);
UE_FACT_FN(kRowRadioUse, "fuse_tower_kind", L"radiotower_C", L"actionOptionIndex", Declared);
UE_FACT_FN(kRowRadioInsert, "fuse_tower_kind", L"radiotower_C", L"playerUsedOn", Declared);
// The radio tower generates its puzzle and its fuses at construction and at BeginPlay (CXXHeaderDump/radiotower.cpp:1061,
// :488-493); a client follows each with the host's kept row.
UE_FACT_FN(kRowRadioGenPuzzle, "fuse_tower_kind", L"radiotower_C", L"genPuzzle", Declared);
UE_FACT_FN(kRowRadioGenFuses, "fuse_tower_kind", L"radiotower_C", L"genFuses", Declared);

const WatchRows kWatches[kKinds] = {
    { &kRowCoordRoll, &kRowCoordJudge, &kRowCoordUse, &kRowCoordInsert, nullptr, nullptr },
    { &kRowRadioRoll, &kRowRadioJudge, &kRowRadioUse, &kRowRadioInsert, &kRowRadioGenPuzzle, &kRowRadioGenFuses },
};

bool LightBit(uint32_t lights, int i) { return ((lights >> i) & 1u) != 0; }

// ---- the coordinate towers -----------------------------------------------------------------------------------

CT::LookAt CoordPart(uint8_t op, int32_t index) {
    switch (op) {
    case coop::net::kFuseTowerOpButton:  return {CT::Part::Button, index};
    case coop::net::kFuseTowerOpLever:   return {CT::Part::Lever, -1};
    case coop::net::kFuseTowerOpRetract: return {CT::Part::Retract, -1};
    case coop::net::kFuseTowerOpPull:
    case coop::net::kFuseTowerOpInsert:  return {CT::Part::Fuse, index};
    default:                              return {CT::Part::None, -1};
    }
}

bool CoordReadRow(void* tower, FuseTowerRow& w) {
    CT::State st;
    if (!CT::Read(tower, st) || st.fuseCount>CT::kMaxFuses || st.lightCount>CT::kMaxLights) return false;
    w = FuseTowerRow{};
    w.id = st.id;
    w.kind = coop::net::kFuseTowerKindCoordinate;
    w.flags = static_cast<uint8_t>((st.isBroken ? coop::net::kFuseTowerBroken : 0) |
                                   (st.opened ? coop::net::kFuseTowerOpened : 0) |
                                   (st.isAnim ? coop::net::kFuseTowerAnim : 0) |
                                   (st.leverMoving ? coop::net::kFuseTowerLeverMoving : 0) |
                                   (st.leverUp ? coop::net::kFuseTowerLeverUp : 0));
    w.fuseCount = st.fuseCount;
    for (int i = 0; i < st.fuseCount; ++i) w.fuses[i] = st.fuses[i];
    w.lightCount = st.lightCount;
    for (int i = 0; i < st.lightCount; ++i)
        if (st.lights[i]) w.lights |= 1u << i;
    return true;
}

// A row keeps paint debt and presentation receipts separately. Callbacks own
// their storage and are checked before/after every engine operation.
enum Effect : unsigned { Repair,Click,Pull,Insert,Fail,Confirm,ResetSound,Reset,Damage,BreakAlert,ConfirmSound,SetState,
                         RepairCount=16,BreakCount,PressCount,PullCount,InsertCount,FailCount,LeverCount,ResetCount };
constexpr uint64_t Bit(unsigned n){return uint64_t{1}<<n;}
constexpr uint64_t kPuzzle=1,kBroken=2,kFuses=4;
struct ApplyRun {
    ue_wrap::CachedObjRef actor;
    const ApplyCtx& ctx;
    uint64_t localWanted=0,localDirty=0,localTried=0;
    ApplyRun(void* a,const ApplyCtx& c):ctx(c){actor.Set(a);}
    bool Current() const {return actor.Get()&&ctx.edges&&ctx.sizeWarned&&(!ctx.current||ctx.current())&&actor.Get();}
    void Request(uint64_t bits){if(Current()){if(ctx.effects)ctx.effects(bits);else localWanted|=bits;}}
    uint64_t Wanted(){return Current()?(ctx.effects?ctx.effects(0):localWanted):0;}
    void Mark(uint64_t bits){if(Current()){if(ctx.writes)ctx.writes(bits,0);else localDirty|=bits;}}
    template<class F> bool Write(F&& f){if(!Current())return false;const bool ok=f();return Current()&&ok;}
    template<class F> bool Paint(uint64_t bit,F&& f){
        if(!Current())return false;
        if(!((ctx.writes?ctx.writes(0,0):localDirty)&bit))return true;
        if(!Write(f))return false;
        if(ctx.writes)ctx.writes(0,bit);else localDirty&=~bit;
        return Current();
    }
    bool First(uint64_t bit){
        if(!Current())return false;
        if(ctx.firstAttempt)return ctx.firstAttempt(bit);
        if(localTried&bit)return false;localTried|=bit;return true;
    }
    template<class F> bool EffectOnce(unsigned n,F&& f){
        if(!Current())return false;const auto bit=Bit(n);
        if(!(Wanted()&bit))return true;
        const bool first=First(bit);if(!Current())return false;
        if(first)f(); // A false/faulted native attempt is uncertain, never an invitation to replay it.
        return Current();
    }
    bool Count(unsigned n,uint32_t& value){return EffectOnce(n,[&]{++value;});}
};
bool CoordApply(void* tower,const FuseTowerRow& c,const ApplyCtx& x){
    ApplyRun run(tower,x);CT::State l;
    if(!run.Current()||!CT::Read(tower,l)||!run.Current()||c.lightCount>CT::kMaxLights||l.lightCount>CT::kMaxLights||
        c.fuseCount>CT::kMaxFuses||l.fuseCount>CT::kMaxFuses)return false;
    const bool broken=(c.flags&net::kFuseTowerBroken)!=0,changed=l.isBroken!=broken;
    bool lights[CT::kMaxLights]{};bool lightDiff=false,fuseDiff=false,pulled=false,inserted=false;
    if(c.lightCount==l.lightCount){for(int i=0;i<c.lightCount;++i){lights[i]=LightBit(c.lights,i);lightDiff=lightDiff||lights[i]!=l.lights[i];}}
    else if(!*x.sizeWarned){*x.sizeWarned=true;UE_LOGW("fuse_tower: coordinate puzzle dimensions differ");}
    if(c.fuseCount==l.fuseCount)for(int i=0;i<c.fuseCount;++i)if(c.fuses[i]!=l.fuses[i]){
        fuseDiff=true;pulled=pulled||(l.fuses[i]==2&&c.fuses[i]==0);inserted=inserted||(l.fuses[i]==0&&c.fuses[i]==1);
    }
    const bool up=(c.flags&net::kFuseTowerLeverUp)!=0,leverDiff=up!=l.leverUp;
    uint64_t wanted=0;
    if(changed)wanted|=Bit(broken?BreakCount:RepairCount);
    if(changed&&!broken&&x.live)wanted|=Bit(Repair);
    if(lightDiff&&!changed){wanted|=Bit(PressCount);if(x.live)wanted|=Bit(Click);}
    if(pulled){wanted|=Bit(PullCount);if(x.live)wanted|=Bit(Pull);}
    if(inserted){wanted|=Bit(InsertCount);if(x.live)wanted|=Bit(Insert);}
    if(leverDiff){wanted|=Bit(Confirm)|Bit(LeverCount);if(!up&&l.isBroken&&broken){wanted|=Bit(FailCount);if(x.live)wanted|=Bit(Fail);}}
    run.Request(wanted);
    if(lightDiff||changed||!x.live)run.Mark(kPuzzle);
    if(changed||!x.live)run.Mark(kBroken);
    if(fuseDiff||!x.live)run.Mark(kFuses);
    if(lightDiff&&!run.Write([&]{return CT::WriteLights(tower,lights,c.lightCount);}))return false;
    if(c.fuseCount==l.fuseCount)for(int i=0;i<c.fuseCount;++i)if(c.fuses[i]!=l.fuses[i])
        if(!run.Write([&]{return CT::WriteFuse(tower,i,c.fuses[i]);}))return false;
    if(!run.EffectOnce(Repair,[&]{return CT::SolvePuzzle(tower);}))return false;
    if((run.Wanted()&Bit(Repair))&&c.lightCount==l.lightCount)
        if(!run.Write([&]{return CT::WriteLights(tower,lights,c.lightCount);}))return false;
    if(changed&&!run.Write([&]{return CT::WriteBroken(tower,broken);}))return false;
    if(!run.Paint(kPuzzle,[&]{return CT::UpdPuzzle(tower);})||!run.Paint(kBroken,[&]{return CT::UpdBroken(tower);}))return false;
    if(!run.EffectOnce(Click,[&]{return CT::Play(tower,CT::Sound::Click);}))return false;
    if(!run.Paint(kFuses,[&]{return CT::UpdFuses(tower);}))return false;
    if(!run.EffectOnce(Pull,[&]{return CT::Play(tower,CT::Sound::FusePulled);})||
       !run.EffectOnce(Insert,[&]{return CT::Play(tower,CT::Sound::FuseInserted);})||
       !run.EffectOnce(Fail,[&]{return CT::Play(tower,CT::Sound::Fail);})||
       !run.EffectOnce(Confirm,[&]{return CT::MoveLever(tower,up);}))return false;
    if(!run.Write([&]{return CT::WriteLeverMoving(tower,(c.flags&net::kFuseTowerLeverMoving)!=0);}))return false;
    auto& e=*x.edges;
    return run.Count(RepairCount,e.repairs)&&run.Count(BreakCount,e.breaks)&&run.Count(PressCount,e.presses)&&
        run.Count(PullCount,e.pulls)&&run.Count(InsertCount,e.inserts)&&run.Count(FailCount,e.fails)&&run.Count(LeverCount,e.levers);
}

Look CoordReadUse(void* tower, uint8_t& op, int32_t& index) {
    CT::LookAt at;
    if (!CT::ReadLookAt(tower, at)) return Look::None;
    switch (at.part) {
    case CT::Part::Button:
        op = coop::net::kFuseTowerOpButton;
        index = at.index;
        return Look::Send;
    case CT::Part::Lever:
        op = coop::net::kFuseTowerOpLever;
        index = -1;
        return Look::Send;
    case CT::Part::Retract:
        op = coop::net::kFuseTowerOpRetract;
        index = -1;
        return Look::Send;
    case CT::Part::Fuse: {
        CT::State st;
        if (!CT::Read(tower, st) || at.index < 0 || at.index >= st.fuseCount || st.fuses[at.index] != 2)
            return Look::None;
        op = coop::net::kFuseTowerOpPull;
        index = at.index;
        return Look::Pull;
    }
    default:
        return Look::None;
    }
}

// ---- the radio tower -----------------------------------------------------------------------------------------

RT::LookAt RadioPart(uint8_t op, int32_t index) {
    switch (op) {
    case coop::net::kFuseTowerOpButton: return {RT::Part::Button, index};
    case coop::net::kFuseTowerOpLever:  return {RT::Part::Confirm, -1};
    case coop::net::kFuseTowerOpReset:  return {RT::Part::Reset, -1};
    case coop::net::kFuseTowerOpPull:
    case coop::net::kFuseTowerOpInsert: return {RT::Part::Fuse, index};
    default:                             return {RT::Part::None, -1};  // the radio tower has no panel
    }
}

bool RadioReadRow(void* tower, FuseTowerRow& w) {
    RT::State st;
    if (!RT::Read(tower, st) || st.fuseCount>RT::kMaxFuses || st.cellCount>RT::kMaxCells) return false;
    w = FuseTowerRow{};
    w.id = 0;
    w.kind = coop::net::kFuseTowerKindRadio;
    // The confirm lever's bits are its timeline's: running, and running forward. The reset lever plays one way. The
    // tower's `animation` is its own flag: the confirm lever's forward end clears it before the judgement, so it is
    // false while a failed lever runs back.
    w.flags = static_cast<uint8_t>((st.isBroken ? coop::net::kFuseTowerBroken : 0) |
                                   (st.leverPlaying ? coop::net::kFuseTowerLeverMoving : 0) |
                                   (st.leverPlaying && st.leverForward ? coop::net::kFuseTowerLeverUp : 0) |
                                   (st.resetPlaying ? coop::net::kFuseTowerResetMoving : 0) |
                                   (st.animation ? coop::net::kFuseTowerAnimating : 0));
    w.fuseCount = st.fuseCount;
    for (int i = 0; i < st.fuseCount; ++i) w.fuses[i] = st.fuses[i];
    w.lightCount = st.cellCount;
    for (int i = 0; i < st.cellCount; ++i)
        if (st.cells[i]) w.lights |= 1u << i;
    return true;
}

// The tower's own graph is not run for the host's act: its writes, painters, sounds and lever timelines are made
// here, and the alert its scramble broadcasts is raised on the console by hand. The scramble itself, the lever's
// judgement and the reset event's roll are refused at the gate, so the host's rows are the only author.
bool RadioApply(void* tower,const FuseTowerRow& c,const ApplyCtx& x){
    ApplyRun run(tower,x);RT::State l;
    if(!run.Current()||!RT::Read(tower,l)||!run.Current()||c.lightCount>RT::kMaxCells||l.cellCount>RT::kMaxCells||
        c.fuseCount>RT::kMaxFuses||l.fuseCount>RT::kMaxFuses)return false;
    const bool broken=(c.flags&net::kFuseTowerBroken)!=0,changed=l.isBroken!=broken;
    bool cells[RT::kMaxCells]{};bool cellDiff=false,fuseDiff=false,pulled=false,inserted=false,blown=false;
    if(c.lightCount==l.cellCount){for(int i=0;i<c.lightCount;++i){cells[i]=LightBit(c.lights,i);cellDiff=cellDiff||cells[i]!=l.cells[i];}}
    else if(!*x.sizeWarned){*x.sizeWarned=true;UE_LOGW("fuse_tower: radio puzzle dimensions differ");}
    if(c.fuseCount==l.fuseCount)for(int i=0;i<c.fuseCount;++i)if(c.fuses[i]!=l.fuses[i]){
        fuseDiff=true;pulled=pulled||(l.fuses[i]==2&&c.fuses[i]==0);inserted=inserted||(l.fuses[i]==0&&c.fuses[i]==1);blown=blown||(l.fuses[i]==1&&c.fuses[i]==2);
    }
    const bool moving=(c.flags&net::kFuseTowerLeverMoving)!=0,up=(c.flags&net::kFuseTowerLeverUp)!=0;
    const bool confirm=moving&&(!(x.prevFlags&net::kFuseTowerLeverMoving)||bool(x.prevFlags&net::kFuseTowerLeverUp)!=up);
    const bool reset=(c.flags&net::kFuseTowerResetMoving)&&!(x.prevFlags&net::kFuseTowerResetMoving)&&x.live;
    uint64_t wanted=0;
    if(changed){wanted|=Bit(broken?BreakCount:RepairCount);if(x.live)wanted|=Bit(broken?BreakAlert:Repair);
        if(broken||!x.live)wanted|=Bit(SetState);}
    if(cellDiff&&!changed){wanted|=Bit(PressCount);if(x.live)wanted|=Bit(Click);}
    if(pulled){wanted|=Bit(PullCount);if(x.live)wanted|=Bit(Pull);}
    if(inserted){wanted|=Bit(InsertCount);if(x.live)wanted|=Bit(Insert);}
    if(blown&&x.live)wanted|=Bit(Damage);
    if(confirm){wanted|=Bit(LeverCount);if(!up)wanted|=Bit(FailCount);if(x.live)wanted|=Bit(Confirm)|Bit(up?ConfirmSound:Fail);}
    if(reset)wanted|=Bit(ResetSound)|Bit(Reset)|Bit(ResetCount);
    run.Request(wanted);
    if(cellDiff||changed||!x.live)run.Mark(kPuzzle);
    if(changed||!x.live)run.Mark(kBroken);
    if(fuseDiff||!x.live)run.Mark(kFuses);
    if(cellDiff&&!run.Write([&]{return RT::WriteCells(tower,cells,c.lightCount);}))return false;
    if(c.fuseCount==l.fuseCount)for(int i=0;i<c.fuseCount;++i)if(c.fuses[i]!=l.fuses[i])
        if(!run.Write([&]{return RT::WriteFuse(tower,i,c.fuses[i]);}))return false;
    if(!run.EffectOnce(Repair,[&]{return RT::CompleteMinigame(tower);}))return false;
    if((run.Wanted()&Bit(Repair))&&c.lightCount==l.cellCount)
        if(!run.Write([&]{return RT::WriteCells(tower,cells,c.lightCount);}))return false;
    if(!run.EffectOnce(SetState,[&]{return RT::SetBroken(tower,broken,false);}))return false;
    if(!run.Paint(kBroken,[&]{return RT::WriteBroken(tower,broken);})||!run.Paint(kPuzzle,[&]{return RT::UpdPuzzle(tower);}))return false;
    if(!run.EffectOnce(BreakAlert,[&]{return RT::Alert(tower,false);})||!run.EffectOnce(Click,[&]{return RT::Play(tower,RT::Sound::Click);}))return false;
    if(!run.Paint(kFuses,[&]{return RT::UpdFuses(tower);}))return false;
    if(!run.EffectOnce(Pull,[&]{return RT::Play(tower,RT::Sound::FusePulled);})||
       !run.EffectOnce(Insert,[&]{return RT::Play(tower,RT::Sound::FuseInserted);})||
       !run.EffectOnce(Damage,[&]{return RT::Alert(tower,true);})||
       !run.EffectOnce(ConfirmSound,[&]{return RT::Play(tower,RT::Sound::Lever);})||
       !run.EffectOnce(Fail,[&]{return RT::Play(tower,RT::Sound::Fail);})||
       !run.EffectOnce(Confirm,[&]{return RT::PlayLever(tower,RT::Lever::Confirm,up);})||
       !run.EffectOnce(ResetSound,[&]{return RT::Play(tower,RT::Sound::Lever);})||
       !run.EffectOnce(Reset,[&]{return RT::PlayLever(tower,RT::Lever::Reset,true);}))return false;
    if(!run.Write([&]{return RT::WriteAnimation(tower,(c.flags&net::kFuseTowerAnimating)!=0);}))return false;
    auto& e=*x.edges;
    return run.Count(RepairCount,e.repairs)&&run.Count(BreakCount,e.breaks)&&run.Count(PressCount,e.presses)&&
        run.Count(PullCount,e.pulls)&&run.Count(InsertCount,e.inserts)&&run.Count(FailCount,e.fails)&&
        run.Count(LeverCount,e.levers)&&run.Count(ResetCount,e.levers);
}

Look RadioReadUse(void* tower, uint8_t& op, int32_t& index) {
    RT::LookAt at;
    RT::State st;
    if (!RT::ReadLookAt(tower, at) || !RT::Read(tower, st) || st.animation) return Look::None;
    switch (at.part) {
    case RT::Part::Button:
    case RT::Part::Confirm:
    case RT::Part::Reset:
        // A whole tower answers a press with its own hint and does nothing else: the player's own game runs it.
        if (!st.isBroken) return Look::None;
        op = at.part == RT::Part::Button ? coop::net::kFuseTowerOpButton
           : at.part == RT::Part::Confirm ? coop::net::kFuseTowerOpLever : coop::net::kFuseTowerOpReset;
        index = at.part == RT::Part::Button ? at.index : -1;
        return Look::Send;
    case RT::Part::Fuse:
        if (at.index < 0 || at.index >= st.fuseCount || st.fuses[at.index] != 2) return Look::None;
        op = coop::net::kFuseTowerOpPull;
        index = at.index;
        return Look::Pull;
    default:
        return Look::None;
    }
}

}  // namespace

bool Resolved(Kind kind) { return kind == Kind::Radio ? RT::Instance() != nullptr : CT::EnsureResolved(); }

int32_t ReadAll(Tower* out, int32_t cap) {
    if(!out||cap<=0)return -1;
    int32_t n = 0;
    if (cap > 0)
        if (void* radio = RT::Instance()) out[n++] = {Kind::Radio, 0, radio};
    void* towers[coop::net::kFuseTowers];
    const int32_t room = std::min<int32_t>(cap - n, coop::net::kFuseTowers);
    const int32_t c = room > 0 ? CT::ReadAll(towers, room) : 0;
    if (c < 0) return n > 0 ? n : -1;
    for (int32_t i = 0; i < std::clamp(c,0,room); ++i) out[n++] = {Kind::Coordinate, CT::IdOf(towers[i]), towers[i]};
    return n;
}

void* Find(Kind kind, int32_t id) {
    if (kind == Kind::Radio) return id == 0 ? RT::Instance() : nullptr;
    void* towers[coop::net::kFuseTowers];
    const int32_t n = CT::ReadAll(towers, coop::net::kFuseTowers);
    for (int32_t i = 0; i < n; ++i)
        if (CT::IdOf(towers[i]) == id) return towers[i];
    return nullptr;
}

bool Identify(void* obj, Kind& kind, int32_t& id) {
    if (const int32_t i = CT::IdOf(obj); i >= 0) {
        kind = Kind::Coordinate;
        id = i;
        return true;
    }
    if (!RT::IsTower(obj)) return false;
    kind = Kind::Radio;
    id = 0;
    return true;
}

bool ReadView(Kind kind, void* tower, View& out) {
    out = View{};
    if (kind == Kind::Radio) {
        RT::State st;
        if (!RT::Read(tower, st) || st.fuseCount>RT::kMaxFuses || st.cellCount>RT::kMaxCells) return false;
        out.broken = st.isBroken;
        out.fuseCount = st.fuseCount;
        for (int i = 0; i < st.fuseCount; ++i) out.fuses[i] = st.fuses[i];
        out.lightCount = st.cellCount;
        return true;
    }
    CT::State st;
    if (!CT::Read(tower, st) || st.fuseCount>CT::kMaxFuses || st.lightCount>CT::kMaxLights) return false;
    out.broken = st.isBroken;
    out.fuseCount = st.fuseCount;
    for (int i = 0; i < st.fuseCount; ++i) out.fuses[i] = st.fuses[i];
    out.lightCount = st.lightCount;
    return true;
}

bool ReadRow(const Tower& tower, FuseTowerRow& row) {
    return tower.kind == Kind::Radio ? RadioReadRow(tower.obj, row) : CoordReadRow(tower.obj, row);
}

bool ApplyRow(Kind kind, void* tower, const FuseTowerRow& row, const ApplyCtx& ctx) {
    if(kind==Kind::Radio)return RadioApply(tower,row,ctx);
    if(kind==Kind::Coordinate)return CoordApply(tower,row,ctx);
    return false;
}

bool ConvergePanel(Kind kind,void* tower,void* player,const FuseTowerRow& row,bool& opens,bool& ran,
                   const std::function<bool()>& allowed) {
    if(kind!=Kind::Coordinate)return false;
    ue_wrap::CachedObjRef actor,body;actor.Set(tower);body.Set(player);
    const auto current=[&]{return actor.Get()&&body.Get()&&(!allowed||allowed())&&actor.Get()&&body.Get();};
    CT::State state;
    if(!current()||!CT::Read(tower,state)||!current()||state.isAnim)return false;
    const bool open=(row.flags&net::kFuseTowerOpened)!=0;
    const bool target=(row.flags&net::kFuseTowerAnim)?!open:open;
    if(state.opened==target)return false;
    const bool result=CT::Use(tower,player,CT::LookAt{CT::Part::Retract,-1});
    if(!current())return false;
    opens=target;ran=result;return true;
}

bool PartLocation(Kind kind, void* tower, uint8_t op, int32_t index, ue_wrap::FVector& out) {
    if (kind == Kind::Radio) return RT::PartLocation(tower, RadioPart(op, index), out);
    return CT::PartLocation(tower, CoordPart(op, index), out);
}

bool Use(Kind kind, void* tower, void* player, uint8_t op, int32_t index) {
    if (kind == Kind::Radio) return RT::Use(tower, player, RadioPart(op, index));
    return CT::Use(tower, player, CoordPart(op, index));
}

Look ReadUse(Kind kind, void* tower, uint8_t& op, int32_t& index) {
    return kind == Kind::Radio ? RadioReadUse(tower, op, index) : CoordReadUse(tower, op, index);
}

bool ReadFuseLook(Kind kind, void* tower, int32_t& slot) {
    return kind == Kind::Radio ? RT::ReadFuseLook(tower, slot) : CT::ReadFuseLook(tower, slot);
}

const char* CanUse(Kind kind, void* tower, uint8_t op) {
    if (kind == Kind::Radio) {
        // radiotower actionOptionIndex :510, :4027-:4259: ignored while a lever runs; a whole tower answers a press
        // with a hint and does nothing.
        RT::State st;
        if (!RT::Read(tower, st)) return "the tower does not read";
        if (st.animation) return "its lever is moving";
        if (!st.isBroken) return "the tower is not broken";
        return nullptr;
    }
    // coordRadarDish actionOptionIndex @2885..@3958: nothing while the panel moves; the retract is then free; the
    // rest needs the panel open, the lever at rest and the tower broken.
    CT::State st;
    if (!CT::Read(tower, st)) return "the tower does not read";
    if (st.isAnim) return "its panel is moving";
    if (op == coop::net::kFuseTowerOpRetract) return nullptr;
    if (!st.opened) return "its panel is closed";
    if (st.leverMoving) return "its lever is moving";
    if (!st.isBroken) return "the tower is not broken";
    return nullptr;
}

const char* CanPull(Kind kind, void* tower, const View& view, int32_t slot) {
    if (kind == Kind::Radio) {
        // The pull's own gates (radiotower actionOptionIndex :510, :524-540), on the host's copy: no lever runs.
        RT::State st;
        if (!RT::Read(tower, st)) return "the tower does not read";
        if (st.animation) return "its lever is moving";
    } else {
        // The pull's own gates (coordRadarDish actionOptionIndex @3586-@3782), on the host's copy.
        CT::State st;
        if (!CT::Read(tower, st)) return "the tower does not read";
        if (!st.opened) return "its panel is closed";
        if (st.isAnim) return "its panel is moving";
        if (st.leverMoving) return "its lever is moving";
    }
    if (slot < 0 || slot >= view.fuseCount || view.fuses[slot] != 2) return "that fuse is not blown on the host";
    return nullptr;
}

const char* CanInsert(Kind kind, void* tower, const View& view, int32_t slot) {
    // The radio tower's insert is ignored while a lever runs (radiotower playerUsedOn :655); the coordinate tower's
    // has no such test (coordRadarDish :616-665), only the held class and the slot.
    if (kind == Kind::Radio) {
        RT::State st;
        if (!RT::Read(tower, st)) return "the tower does not read";
        if (st.animation) return "its lever is moving";
    }
    if (slot < 0 || slot >= view.fuseCount || view.fuses[slot] != 0) return "that fuse slot is not empty on the host";
    return nullptr;
}

bool TakePull(Kind kind, void* tower, int32_t slot, const std::function<bool()>& current) {
    const auto active = [&] { return !current || current(); };
    if (!active()) return false;
    // The pull's writes on the host's copy; its fuse is already in its puller's hand.
    if (kind == Kind::Radio) {
        if (!RT::WriteFuse(tower, slot, 0)) return false;
        if (!active()) return true;
        RT::UpdFuses(tower);
        if (!active()) return true;
        RT::Play(tower, RT::Sound::FusePulled);
        return true;
    }
    if (!CT::WriteFuse(tower, slot, 0)) return false;
    if (!active()) return true;
    CT::UpdFuses(tower);
    if (!active()) return true;
    CT::Play(tower, CT::Sound::FusePulled);
    return true;
}

// The insert's own branch on the difficulty (coordRadarDish playerUsedOn @4823, radiotower playerUsedOn :696):
// difficulty 0 and 1 keep the fuse and solve, anything above spends it.
bool EasyInsert() {
    const int difficulty = ue_wrap::game_rules::ReadDifficulty();
    return difficulty >= 0 && difficulty <= 1;
}

bool TakeInsert(Kind kind, void* tower, int32_t slot, const std::function<bool()>& current) {
    const auto active = [&] { return !current || current(); };
    if (!active()) return false;
    // The insert's writes on the host's copy; its fuse is already spent (playerUsedOn @5129..@5317, :681-701).
    if (kind == Kind::Radio) {
        if (!RT::WriteFuse(tower, slot, 1)) return false;
        if (!active()) return true;
        RT::Play(tower, RT::Sound::FuseInserted);
        if (!active()) return true;
        RT::UpdFuses(tower);
        return true;
    }
    if (!CT::WriteFuse(tower, slot, 1)) return false;
    if (!active()) return true;
    CT::Play(tower, CT::Sound::FuseInserted);
    if (!active()) return true;
    CT::UpdFuses(tower);
    return true;
}

void TakeEasyInsert(Kind kind, void* tower, int32_t slot, const std::function<bool()>& current) {
    const auto active = [&] { return !current || current(); };
    if (!active()) return;
    if (kind == Kind::Radio) {
        // radiotower playerUsedOn :681-696: the slot good, its sound, updFuses, completeMinigame; the fuse stays.
        if (!RT::WriteFuse(tower, slot, 1) || !active()) return;
        RT::Play(tower, RT::Sound::FuseInserted);
        if (!active()) return;
        RT::UpdFuses(tower);
        if (!active()) return;
        RT::CompleteMinigame(tower);
        return;
    }
    CT::SolvePuzzle(tower);  // coordRadarDish playerUsedOn @5354: solvePuzzle and nothing else
}

bool IsFuse(void* actor) { return CT::IsFuse(actor); }
void* SpawnFuse(const ue_wrap::FVector& at) { return CT::SpawnFuse(at); }
void* SpawnFuseTracked(const ue_wrap::FVector& at,bool& attempted,const std::function<bool()>& current) { return CT::SpawnFuseTracked(at,attempted,current); }
bool IsPulledFuse(void* actor) { return CT::IsPulledFuse(actor); }

const WatchRows& WatchesOf(Kind kind) { return kWatches[static_cast<int>(kind)]; }

}  // namespace coop::fuse_tower_kind
