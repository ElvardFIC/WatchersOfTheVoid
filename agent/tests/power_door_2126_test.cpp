#include "../../src/votv-coop/src/coop/world/power_grid.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace test {
int actor=1,sets=0;bool body=true,reach=true,busy=false,readable=true;
ue_wrap::generator::Row state{false,false,100,0,1};
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void*) { return 1; }
int32_t SlotSerial(int32_t) { return 1; }
int32_t AllocateSlotSerial(int32_t) { return 1; }
bool IsLiveByIndex(void* a,int32_t) { return a!=nullptr; }
int32_t ParamOffset::Of(void*) { return 0; }
int32_t FindParamOffset(void*,const wchar_t*) { return 0; }
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn) { return true; }
bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int) { return true; }
bool ClassNameWatchSettled(const wchar_t*,const wchar_t*,int) { return true; }
void ResolvePendingNames() {}
bool IsBodyActive(void*,void*) { return false; }
}
namespace ue_wrap::world_identity { void* CurrentWorld() { return &test::actor; } void* WorldOf(void*) { return CurrentWorld(); } }
namespace coop::config_registry::detail { struct RegistryDef { static StringRow Stub() { return {nullptr,RegistryCtorKey{}}; } }; }
namespace coop::config_registry::rows { const StringRow grid_drill=detail::RegistryDef::Stub(); }
namespace ue_wrap::generator {
bool EnsureResolved() { return true; }
size_t ReadGenerators(std::vector<void*>& out) { out.push_back(&test::actor);return 1; }
int32_t IndexOf(void*) { return 0; }
bool ReadRow(void*,Row& out) { out=test::state;return test::readable; }
bool SetDoor(void*,bool open) { if (test::busy) return false;test::state.door=open ? 2 : 1;++test::sets;return true; }
bool WriteBroken(void*,bool v) { test::state.broken=v;return true; }
bool WriteCycle(void*,int32_t v) { test::state.cycle=v;return true; }
bool WriteCyc(void*,bool v) { test::state.cyc=v;return true; }
bool WriteUpgradeLevel(void*,int32_t v) { test::state.upgradeLevel=v;return true; }
bool CallBreak(void*) { return true; }
bool CallDamage(void*) { return true; }
bool CallUpdUpgrades(void*) { return true; }
bool Repair(void*) { return true; }
}
namespace ue_wrap::weapon_catalog {
bool Ready() { return false; }
bool Lookup(const std::wstring&,Swing&) { return false; }
}
namespace coop::config { std::string ResolveString(const config_registry::StringRow&) { return {}; } }
namespace coop::hand_item { std::wstring HeldItem(uint8_t) { return {}; } }
namespace coop::net_pump { bool IsInAnnouncedWorld(void*) { return true; } }
namespace coop::roster_ledger { void RegisterPerSlotClear(PerSlotClearFn,void*) {} }
namespace coop::power_panel { void ReassertCanonical() {} }
namespace coop::power_decay { void Install(net::Session*) {} void Tick() {} void OnDisconnect() {} }
namespace coop::power_upgrade {
void Install(net::Session*) {} void OnDisconnect() {}
bool TakeSpend(uint8_t,int32_t) { return false; } void Refund(uint8_t,void*) {}
}
namespace coop::power_puzzle {
void Install(net::Session*) {} void Tick() {} void OnDisconnect() {} void FlushInputs() {}
void Fill(void*,net::PowerGridPuzzle&) {}
bool SamePuzzle(const net::PowerGridPuzzle&,const net::PowerGridPuzzle&) { return true; }
bool Solved(void*) { return true; } bool Settling(void*) { return false; }
const char* HostTake(uint8_t,const net::PowerGridPayload&,void*,bool,bool,bool& wait) { wait=false;return nullptr; }
void OnRows(const net::PowerGridPayload&,uint16_t) {} void Reconcile(const std::vector<void*>&) {}
}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session&,uint8_t,float) { return {}; }
IntentSubject IntentTarget::Authorize(void*) const { return {!test::body ? IntentOutcome::NoBody : test::reach ? IntentOutcome::Ok : IntentOutcome::OutOfReach}; }
const char* OutcomeName(IntentOutcome) { return "test"; }
}
using namespace coop;using namespace coop::power_grid;
int main() {
    net::Session s;g_session.store(&s);
    net::PowerGridPayload p{};p.op=net::kPowerGridOpDoor;p.seq=1;p.value=1;
    OnReliable(p,1);CHECK(test::state.door==2 && g_ack[1]==1 && !s.grids.empty());
    const auto count=test::sets;OnReliable(p,1);CHECK(test::sets==count);
    p.seq=2;p.value=0;test::busy=true;OnReliable(p,1);CHECK(g_ack[1]==1 && g_waiting[1].size()==1);
    test::busy=false;HostDrainWaiting(&s);CHECK(g_ack[1]==2 && test::state.door==1);
    p.seq=3;p.value=1;test::reach=false;OnReliable(p,1);CHECK(g_ack[1]==3 && test::state.door==1);
    p.seq=4;p.value=2;OnReliable(p,1);CHECK(g_ack[1]==3);
    p.value=1;p.index=255;OnReliable(p,1);CHECK(g_ack[1]==3);
    net::PowerGridPayload row{};CHECK(ReadRows(row));auto changed=row;changed.rows[0].door=2;CHECK(!SameRows(row,changed));
    s.mode=net::Role::Client;g_pending.clear();g_seq=0;g_haveRows=true;g_rows=row;
    uint8_t open=1;ue_wrap::script_gate::Call call{};call.object=&test::actor;call.locals=&open;
    OnDoorPre(call);CHECK(g_pending.size()==1 && s.grids.back().op==net::kPowerGridOpDoor && s.grids.back().value==1);
    OnReliable(row,0);CHECK(test::state.door==2); // unacknowledged local door intent survives older canonical
    row.ack[1]=1;test::busy=true;OnReliable(row,0);CHECK(g_rowsWaiting && g_pending.empty());
    test::busy=false;Tick();CHECK(test::state.door==1 && !g_rowsWaiting);
    changed.rows[0].door=255;const auto before=test::sets;OnReliable(changed,0);CHECK(test::sets==before);
    g_seq=65535;CHECK(NextSeq()==1);
    s.sendOk=false;g_pending.clear();ClientSendOp(&s,net::kPowerGridOpDoor,0,0.f,1);CHECK(g_pending.size()==1);
    s.sendOk=true;Tick();CHECK(s.grids.back().op==net::kPowerGridOpDoor && s.grids.back().seq==g_pending.front().seq);
    s.mode=net::Role::Host;s.sendOk=false;g_ackDirty=true;HostSendRows(&s);CHECK(g_ackDirty);
    s.sendOk=true;HostSendRows(&s);CHECK(!g_ackDirty);
    g_owed=2;s.sendOk=false;HostServeOwed(&s);CHECK(g_owed==2);
    s.sendOk=true;HostServeOwed(&s);CHECK(!g_owed);
    std::puts("PASS: production transformer door: host/client, duplicates, busy wait, reach, malformed wire, prediction and late reconciliation");
}
