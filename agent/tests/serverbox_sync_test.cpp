#include "../../src/votv-coop/src/coop/interactables/serverbox_sync.cpp"
#include "facts_test_support.h"
#include <cassert>
#include <cstdio>
#include <functional>
#include <vector>
namespace test {
uint32_t world=1,serial=1;
int gmSerial=1,boxSerial=1;
int gm=0,box=0;
void* gmPtr=&gm;
bool resolved=true,readable=true,writeable=true,broken=false,repair=true,skinSucceeds=true;
int serverCount=1,credits=0;
ue_wrap::serverbox::Aggregates totals{};
ue_wrap::serverbox::RepairState state{};
float best=0;
int writes=0,skins=0;
std::function<void()> onRead,onWrite,onSkin,onFix,onCredit;
struct Clear {coop::roster_ledger::PerSlotClearFn fn;void* self;};
std::vector<Clear>& Clears(){static std::vector<Clear> value;return value;}
void ClearSlot(int slot){for(auto& item:Clears())item.fn(item.self,slot);}
void Fire(std::function<void()>& source){auto fn=std::move(source);source={};if(fn)fn();}
void Poll(){coop::serverbox_sync::g_lastPollMs=0;coop::serverbox_sync::Tick();}
void Reset(coop::net::Session& s,coop::net::Role role=coop::net::Role::Host){
    coop::serverbox_sync::OnDisconnect();s={};s.mode=role;
    ++serial;++gmSerial;resolved=readable=writeable=repair=skinSucceeds=true;broken=false;totals={};state={};best=0;
    writes=skins=credits=0;onRead=onWrite=onSkin=onFix=onCredit={};gmPtr=&gm;serverCount=1;
    coop::serverbox_sync::Install(&s);
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* WorldOf(void*){return nullptr;}
void* CurrentWorld(){return nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::world_singleton {void* Gamemode(){return test::gmPtr;}}
namespace ue_wrap::game_thread {bool IsGameThread(){return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p!=nullptr;}
int32_t InternalIndexOf(void* p){return p==&test::gm?1:p==&test::box?2:-1;}
int32_t SlotSerial(int32_t index){return index==1?test::gmSerial:test::boxSerial;}
int32_t AllocateSlotSerial(int32_t index){return SlotSerial(index);}
bool IsLiveByIndex(void* p,int32_t index){return p&&(index==1?p==&test::gm:index==2&&p==&test::box);}
const FName& NameOf(void*){static FName value{};return value;}
std::wstring ToString(const FName&){return L"test";}
}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn fn,void* self){test::Clears().push_back({fn,self});}}
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function&,int,PreFn,PostFn){return true;}
void ResolvePendingNames(){}
bool NameWatchSettled(const facts::Function&,int){return true;}
bool NameWatchLive(const facts::Function&,int){return true;}
}
namespace ue_wrap::economy {bool AddPoints(int32_t){++test::credits;test::Fire(test::onCredit);return true;}}
namespace ue_wrap::serverbox {
bool EnsureBreakResolved(){return test::resolved;}
bool EnsureRepairResolved(){return test::repair;}
bool ReadAggregates(Aggregates& out){out=test::totals;test::Fire(test::onRead);return test::readable;}
bool WriteAggregates(const Aggregates& in){if(!test::writeable)return false;test::totals=in;++test::writes;test::Fire(test::onWrite);return true;}
size_t ReadServers(std::vector<void*>& out){out.clear();if(test::serverCount)out={&test::box};return out.size();}
bool ReadIsBroken(void*){return test::broken;}
bool ReadRepairState(void*,RepairState& out){out=test::state;return test::repair;}
bool WriteRepairState(void*,const RepairState& in){if(!test::repair)return false;test::state=in;return true;}
bool ReadRepairBest(float& out){out=test::best;return test::repair;}
bool WriteRepairBest(float in){if(!test::repair)return false;test::best=in;return true;}
bool ApplyBreak(void*,bool value){test::broken=value;++test::skins;test::Fire(test::onSkin);return test::skinSucceeds;}
bool IsRepairWidget(void*){return false;}
bool ReadRepairReward(void*,bool&,float&){return false;}
int32_t IndexOf(void*){return 0;}
bool CallFix(void*){test::broken=false;test::Fire(test::onFix);return true;}
}
int main(){
    namespace S=coop::serverbox_sync;using namespace test;using coop::net::Role;
    coop::net::Session s;Reset(s);
    Poll();s.sent.clear();s.allowed[1]=false;
    broken=true;totals.brokenServers=1;Poll();
    assert(s.sent.size()==1&&s.sent.back().slot==2);
    s.allowed[1]=true;Poll();
    assert(s.sent.size()==2&&s.sent.back().slot==1);
    Poll();assert(s.sent.size()==2);
    // All-refused transitions, join snapshots and repair refusal corrections
    // remain due even when the state itself never changes again.
    Reset(s);Poll();assert(s.sent.size()==2);s.sent.clear();
    s.allowed[1]=s.allowed[2]=false;broken=true;state={true,7};best=2.5f;
    Poll();Poll();assert(s.sent.empty());s.allowed[1]=s.allowed[2]=true;Poll();
    assert(s.sent.size()==2&&s.sent[0].state.damagedMask==1&&s.sent[0].state.minigame[0]==7&&s.sent[0].state.timeBest==2.5f);
    s.allowed[1]=false;S::QueueConnectBroadcastForSlot(1);Poll();assert(s.sent.size()==2);
    s.allowed[1]=true;Poll();assert(s.sent.size()==3&&s.sent.back().slot==1);
    s.allowed[2]=false;coop::net::ServerRepairPayload invalid{};invalid.box=63;
    S::OnRepair(invalid,2);Poll();assert(s.sent.size()==3);s.allowed[2]=true;Poll();assert(s.sent.size()==4&&s.sent.back().slot==2);
    s.peers[3]=true;s.generations[3]=3;Poll();assert(s.sent.size()==4);
    s.ready[3]=true;Poll();assert(s.sent.size()==5&&s.sent.back().slot==3);
    ++s.generations[1];Poll();assert(s.sent.size()==6&&s.sent.back().slot==1);
    ClearSlot(2);Poll();assert(s.sent.size()==7&&s.sent.back().slot==2);
    s.ready[1]=false;Poll();s.ready[1]=true;Poll();assert(s.sent.size()==8&&s.sent.back().slot==1);
    // No native resolution at the join edge: a static first row still arrives.
    Reset(s);resolved=false;S::QueueConnectBroadcastForSlot(1);Poll();assert(s.sent.empty());
    resolved=true;Poll();assert(s.sent.size()==2);Poll();assert(s.sent.size()==2);
    // The host rejects inbound state even if the receive gate was bypassed.
    coop::net::ServerStatePayload p{};p.serverCount=1;p.isBrokenMask=p.damagedMask=1;p.minigame[0]=6;p.brokenServers=1;p.timeBest=4;
    S::OnReliable(p);assert(writes==0);
    // A changed session, world or gamemode during the read/send must neither
    // acknowledge stale bytes nor continue into the next recipient.
    Reset(s);onRead=[]{S::OnDisconnect();};Poll();assert(s.sent.empty());
    Reset(s);++serial;Poll();assert(s.sent.empty());
    Reset(s);s.callback=[]{S::OnDisconnect();};Poll();assert(s.sent.size()==1);Poll();assert(s.sent.size()==1);
    Reset(s);s.callback=[]{++world;};Poll();assert(s.sent.size()==1);Poll();assert(s.sent.size()==3);
    Reset(s);s.callback=[]{gmPtr=&box;};Poll();assert(s.sent.size()==1);Poll();assert(s.sent.size()==3);
    Reset(s);s.callback=[]{++gmSerial;};Poll();assert(s.sent.size()==1);Poll();assert(s.sent.size()==3);
    Reset(s);s.callback=[&]{++s.generations[1];};Poll();assert(s.sent.size()==2);Poll();assert(s.sent.size()==3&&s.sent.back().slot==1);
    Reset(s);s.callback=[]{S::QueueConnectBroadcastForSlot(1);Poll();};Poll();assert(s.sent.size()==2);Poll();assert(s.sent.size()==3&&s.sent.back().slot==1);
    Reset(s);broken=true;coop::net::ServerRepairPayload good{};good.box=0;good.flags=coop::net::kRepairSettles;good.timeDs=20;
    onFix=[]{++world;};S::OnRepair(good,1);assert(credits==0&&s.sent.empty());
    Reset(s);broken=true;best=5;onCredit=[]{++world;};S::OnRepair(good,1);assert(credits==1&&best==5&&s.sent.empty());
    // A client's latest row survives its wrapper/list/repair load windows and
    // write refusal, with no duplicate skin once an already-applied flag matches.
    Reset(s,Role::Client);resolved=false;S::OnReliable(p);assert(writes==0);
    resolved=true;writeable=false;Poll();assert(writes==0);writeable=true;repair=false;Poll();
    assert(broken&&skins==1&&S::g_pending.present);repair=true;serverCount=0;Poll();assert(S::g_pending.present);
    serverCount=1;Poll();assert(!S::g_pending.present&&state.damaged&&state.minigame==6&&best==4&&skins==1);
    const auto settledWrites=writes;Poll();assert(writes==settledWrites);
    // ApplyBreak writes the raw flag before its native check() can refuse/fail.
    // Matching flags alone must not acknowledge that unfinished visual effect.
    Reset(s,Role::Client);skinSucceeds=false;S::OnReliable(p);
    assert(broken&&skins==1&&S::g_pending.present);
    Poll();assert(skins==2&&S::g_pending.present);
    S::OnReliable(p);assert(skins==3&&S::g_pending.present);
    skinSucceeds=true;Poll();assert(skins==4&&!S::g_pending.present);
    Poll();assert(skins==4);
    // A callback can replace the row while the older native effect fails. Its
    // unchanged flag must still carry that debt into the next latest-row apply.
    Reset(s,Role::Client);skinSucceeds=false;auto sameFlag=p;sameFlag.minigame[0]=3;
    onSkin=[&]{S::OnReliable(sameFlag);};S::OnReliable(p);
    assert(skins==1&&S::g_pending.present);skinSucceeds=true;Poll();
    assert(skins==2&&state.minigame==3&&!S::g_pending.present);
    // Debt belongs to the exact box incarnation, not just servers[] or pointer.
    Reset(s,Role::Client);skinSucceeds=false;S::OnReliable(p);
    ++boxSerial;skinSucceeds=true;Poll();assert(skins==1&&!S::g_pending.present);
    Reset(s,Role::Client);onSkin=[]{++boxSerial;};S::OnReliable(p);
    assert(skins==1&&S::g_pending.present);Poll();assert(skins==1&&!S::g_pending.present);
    // No gamemode yet is a load window; after binding, a replacement is teardown.
    Reset(s,Role::Client);gmPtr=nullptr;S::OnReliable(p);assert(writes==0);gmPtr=&gm;Poll();assert(writes==1&&broken);
    Reset(s,Role::Client);resolved=false;S::OnReliable(p);++world;resolved=true;Poll();assert(writes==0&&!broken);
    Reset(s,Role::Client);resolved=false;S::OnReliable(p);gmPtr=&box;resolved=true;Poll();assert(writes==0&&!broken);
    Reset(s,Role::Client);resolved=false;S::OnReliable(p);++gmSerial;resolved=true;Poll();assert(writes==0&&!broken);
    Reset(s,Role::Client);resolved=false;S::OnReliable(p);++serial;resolved=true;Poll();assert(writes==0&&!broken);
    Reset(s,Role::Client);onWrite=[]{S::OnDisconnect();};S::OnReliable(p);assert(writes==1&&skins==0);Poll();assert(skins==0);
    // A newer inbound row published by a native callback is not acknowledged
    // by completion of the older one; nested Tick cannot recurse the apply.
    Reset(s,Role::Client);auto latest=p;latest.brokenServers=0;latest.isBrokenMask=latest.damagedMask=0;latest.minigame[0]=2;
    onWrite=[&]{S::OnReliable(latest);Poll();};S::OnReliable(p);assert(writes==1&&broken&&S::g_pending.present);
    Poll();assert(writes==2&&!broken&&state.minigame==2&&!S::g_pending.present);Poll();assert(writes==2);
    Reset(s,Role::Client);onSkin=[]{++world;};S::OnReliable(p);assert(S::g_pending.present);Poll();assert(!S::g_pending.present);
    puts("PASS serverbox per-peer partial/all-refused delivery, static join/refusal debt, occupancy readiness, load-window apply retry, latest-row reentry and world/gamemode/session teardown guards");
}
