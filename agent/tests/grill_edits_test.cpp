#define GRILL_EDITS_ACTUAL_TEST
#ifndef GRILL_EDITS_SOURCE
#define GRILL_EDITS_SOURCE "../../src/votv-coop/src/coop/interactables/grill_edits.cpp"
#endif
#include GRILL_EDITS_SOURCE
#include "coop/net/session_serial.h"
#define main PriorKitchenMain
#include "kitchen_2124_test.cpp"
#undef main
namespace ue_wrap::script_gate {
std::set<uint64_t> activeInvocations{1};
bool IsInvocationActive(uint64_t id){return activeInvocations.contains(id);}
}
namespace E=coop::grill_edits;
void EditReset(coop::net::Session& session,coop::net::Role role){
    Reset(session,role);test::state.kind=ue_wrap::kitchen_machine::Kind::Grill;
    test::state.hasDoorPose=false;test::state.coals=3;test::state.flags=ue_wrap::kitchen_machine::HasLid;
    ue_wrap::script_gate::activeInvocations={1};
}
coop::net::GrillEditPayload Edit(uint32_t sequence,uint8_t operation=1,int before=3,int after=4,uint8_t slot=1,const wchar_t* key=L"oven"){
    coop::net::GrillEditPayload p{};coop::net::WireKeyFromString(key,p.key);p.sequence=sequence;p.operation=operation;
    p.before=before;p.after=after;p.slot=slot;p.playerNo=coop::roster_ledger::Get(slot).playerNo;return p;
}
void ReadyEdits(){for(auto& row:E::g_host)row.next=0;for(auto& row:E::g_local)row.next=0;}
int main(){
    using namespace coop;net::Session s;EditReset(s,net::Role::Client);
    ue_wrap::script_gate::Call call{};call.object=&test::oven;call.stack=&call;call.depth=1;call.invocation=1;
    E::BeforeChange(call,1);++test::state.coals;E::AfterChange(call);CHECK(E::g_local.size()==1);
    s.sendOk=false;E::Tick();CHECK(s.grillEdits.empty() && E::g_local.size()==1);
    s.sendOk=true;ReadyEdits();E::Tick();CHECK(s.grillEdits.size()==1 && !s.grillEdits[0].result);
    auto coal=s.grillEdits[0];CHECK(coal.before==3 && coal.after==4);
    E::BeforeChange(call,2);test::state.flags=0;E::AfterChange(call);CHECK(E::g_local.size()==2);
    ReadyEdits();E::Tick();CHECK(s.grillEdits.size()==2 && s.grillEdits.back().operation==1); // same-key FIFO until reply
    auto reply=coal;reply.result=1;E::Receive(reply,1);CHECK(E::g_local.size()==2);E::Receive(reply,0);CHECK(E::g_local.size()==1);
    ReadyEdits();E::Tick();CHECK(s.grillEdits.back().operation==2);
    auto lid=s.grillEdits.back();
    EditReset(s,net::Role::Host);E::Receive(coal,1);E::Receive(lid,1);E::Tick();
    CHECK(test::state.coals==4 && !(test::state.flags&ue_wrap::kitchen_machine::HasLid) && test::stockWrites==2);
    CHECK(s.grillEdits.size()==2 && s.grillEdits[0].result==1 && test::records>=1);
    E::Receive(coal,1);E::Receive(lid,1);ReadyEdits();E::Tick();CHECK(test::stockWrites==2); // no repeat increment
    // Concurrent clients add one each using their stale observed count; no full-record replacement.
    EditReset(s,net::Role::Host);E::Receive(Edit(10),1);E::Receive(Edit(20,1,3,4,2),2);E::Tick();CHECK(test::state.coals==5);
    CHECK(test::state.flags&ue_wrap::kitchen_machine::HasLid);
    EditReset(s,net::Role::Host);test::mutationReady=false;E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==0);
    test::mutationReady=true;ReadyEdits();E::Tick();CHECK(test::stockWrites==1); // initial/parked record precedes edits
    // Missing target waits; unrelated later keys and other peers make bounded progress.
    EditReset(s,net::Role::Host);for(int i=1;i<=16;++i){const auto key=L"missing"+std::to_wstring(i);E::Receive(Edit(i,1,3,4,1,key.c_str()),1);}
    E::Receive(Edit(1,1,3,4,2,L"oven2"),2);E::Tick();ReadyEdits();E::Tick();ReadyEdits();E::Tick();CHECK(test::stockWrites==1);
    // Deferred native metadata, refused publication and reply retain their work.
    EditReset(s,net::Role::Host);auto p=Edit(1);test::stockReadOK=false;E::Receive(p,1);E::Tick();CHECK(test::stockWrites==0 && E::g_host.size()==1);
    test::stockReadOK=true;test::recordOK=false;s.sendOk=false;ReadyEdits();E::Tick();CHECK(test::stockWrites==1 && E::g_host.front().record && E::g_host.front().owed);
    s.sendOk=test::recordOK=true;ReadyEdits();E::Tick();CHECK(test::stockWrites==1 && !E::g_host.front().record && !E::g_host.front().owed);
    // An accepted edit keeps its exact terminal reply through transport backpressure.
    EditReset(s,net::Role::Host);s.sendOk=false;p=Edit(1);E::Receive(p,1);E::Tick();
    CHECK(test::stockWrites==1 && E::g_host.front().packet.result==1 && E::g_host.front().owed);
    E::g_host.front().kitchen=E::g_host.front().record=false;
    E::g_host.front().until=0;ReadyEdits();E::Tick();CHECK(E::g_host.size()==1 && E::g_host.front().owed);
    E::Receive(p,1);s.sendOk=true;ReadyEdits();E::Tick();
    CHECK(test::stockWrites==1 && s.grillEdits.back().result==1 && E::g_host.empty());
    // A presentation callback cannot publish the old record under a changed key.
    EditReset(s,net::Role::Host);p=Edit(1);test::callback=[]{test::keyOverride=L"renamed";};
    E::Receive(p,1);E::Tick();CHECK(test::stockWrites==1 && test::records==0);
    ReadyEdits();E::Tick();CHECK(test::records==0 && s.grillEdits.back().result==1);
    // Bound actor serial must not silently adopt its replacement.
    EditReset(s,net::Role::Host);test::stockReadOK=false;E::Receive(Edit(1),1);E::Tick();++test::oven.serial;
    test::stockReadOK=true;ReadyEdits();E::Tick();CHECK(test::stockWrites==0 && E::g_host.front().packet.result==2);
    // Partial native paint is terminal, including replay after the reply row expires.
    EditReset(s,net::Role::Host);p=Edit(1,2,1,0);test::stockResult=ue_wrap::grill::Edit::Interrupted;
    E::Receive(p,1);E::Tick();CHECK(test::stockWrites==1 && E::g_host.front().packet.result==3);
    E::g_host.front().until=0;ReadyEdits();E::Tick();
    CHECK(E::g_host.empty());
    E::Receive(p,1);E::Tick();CHECK(test::stockWrites==1 && E::g_host.front().packet.result==2);
    // World/session and occupant changes interrupt native work, sends and old captures.
    EditReset(s,net::Role::Host);test::stockReadCallback=[]{++test::occupancy;};E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==0);
    EditReset(s,net::Role::Host);test::stockReadCallback=[]{++test::world;};E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==0);
    EditReset(s,net::Role::Host);test::stockEditCallback=[]{E::OnDisconnect();};E::Receive(Edit(1),1);E::Tick();CHECK(E::g_host.empty() && s.grillEdits.empty());
    EditReset(s,net::Role::Client);E::BeforeChange(call,1);++test::state.coals;++test::oven.serial;E::AfterChange(call);CHECK(E::g_local.empty());
    EditReset(s,net::Role::Host);test::state.coals=16;E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==0 && E::g_host.front().packet.result==2);
    EditReset(s,net::Role::Host);test::reach=false;E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==0 && E::g_host.front().packet.result==2);
    EditReset(s,net::Role::Host);auto invalid=Edit(1);invalid.after=9;E::Receive(invalid,1);invalid=Edit(1);invalid.playerNo=9;E::Receive(invalid,1);CHECK(E::g_host.empty());
    EditReset(s,net::Role::Host);E::Receive(Edit(1),1);++test::oven.serial;E::Tick();CHECK(test::stockWrites==0); // receive captures the original target
    EditReset(s,net::Role::Host);test::stockReadCallback=[]{test::keyOverride=L"changed";};E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==0);
    EditReset(s,net::Role::Host);test::stockReadCallback=[]{E::OnPeerLeft(1);E::Receive(Edit(1),1);};
    E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==0);ReadyEdits();E::Tick();CHECK(test::stockWrites==1);
    EditReset(s,net::Role::Host);p=Edit(1);E::Receive(p,1);E::Tick();
    E::g_host.front().kitchen=E::g_host.front().record=false;E::g_host.front().owed=true;
    s.beforeSend=[&]{E::OnPeerLeft(1);E::Receive(p,1);E::Finish(p,2,E::Now());};E::Tick();
    CHECK(E::g_host.size()==1 && E::g_host.front().owed);
    EditReset(s,net::Role::Client);E::BeforeChange(call,1);++test::state.coals;E::AfterChange(call);CHECK(E::g_local.size()==1);
    ++test::occupancy;E::BeforeChange(call,1);++test::state.coals;E::AfterChange(call);E::Tick();
    CHECK(E::g_local.size()==1 && E::g_local.front().packet.playerNo==test::occupancy && s.grillEdits.size()==1);
    EditReset(s,net::Role::Client);call.tag=0x4B54000B;
    CHECK(coop::kitchen_sync::OnGrillCoal(call)==ue_wrap::script_gate::Verdict::Run);
    ++test::state.coals;coop::kitchen_sync::AfterGrillChange(call);CHECK(E::g_local.size()==1 && test::records==0);
    EditReset(s,net::Role::Client);call.tag=0x4B54000D;
    CHECK(coop::kitchen_sync::OnGrillLid(call)==ue_wrap::script_gate::Verdict::Run);
    test::state.flags=0;auto wrong=call;wrong.function=&test::local;E::AfterChange(wrong);CHECK(E::g_captures.size()==1 && E::g_local.empty());
    coop::kitchen_sync::AfterGrillChange(call);CHECK(E::g_local.size()==1 && E::g_local.front().packet.operation==2 && test::records==0);
    EditReset(s,net::Role::Client);E::BeforeChange(call,1);CHECK(E::g_captures.size()==1);
    ue_wrap::script_gate::activeInvocations.clear();E::Tick();CHECK(E::g_captures.empty());
    ue_wrap::script_gate::activeInvocations={1,2};E::BeforeChange(call,1);auto reused=call;reused.invocation=2;
    ++test::state.coals;E::AfterChange(reused);CHECK(E::g_local.empty() && E::g_captures.size()==1);
    E::AfterChange(call);CHECK(E::g_local.size()==1);
    EditReset(s,net::Role::Client);test::stockReadCallback=[]{++test::occupancy;};E::BeforeChange(call,1);CHECK(E::g_captures.empty());
    EditReset(s,net::Role::Client);E::BeforeChange(call,1);++test::state.coals;
    test::stockReadCallback=[]{E::OnPeerLeft(1);};E::AfterChange(call);CHECK(E::g_local.empty());
    // Session::Start can reuse the same Session, world, actor and roster occupant.
    // Neither queued stock edits nor terminal history belong to that new start.
    EditReset(s,net::Role::Host);E::Receive(Edit(1),1);
    net::session_serial::Next();E::Tick();CHECK(test::stockWrites==0 && E::g_host.empty() && s.grillEdits.empty());
    E::Receive(Edit(1),1);E::Tick();CHECK(test::stockWrites==1 && s.grillEdits.back().result==1);
    net::session_serial::Next();E::Install(&s);s.grillEdits.clear();E::Receive(Edit(1),1);E::Tick();
    CHECK(test::stockWrites==2 && s.grillEdits.size()==1 && s.grillEdits.back().result==1);
    EditReset(s,net::Role::Client);E::BeforeChange(call,1);++test::state.coals;E::AfterChange(call);
    net::session_serial::Next();E::Tick();CHECK(E::g_local.empty() && s.grillEdits.empty());
    E::BeforeChange(call,1);++test::state.coals;E::AfterChange(call);E::Tick();
    CHECK(E::g_local.size()==1 && s.grillEdits.size()==1);
    EditReset(s,net::Role::Client);E::BeforeChange(call,1);++test::state.coals;
    net::session_serial::Next();E::AfterChange(call);CHECK(E::g_local.empty());
    // Invalidate immediately inside native reads, before the next Tick/Install.
    EditReset(s,net::Role::Host);E::Receive(Edit(1),1);
    test::stockReadCallback=[]{net::session_serial::Next();};E::Tick();
    CHECK(test::stockWrites==0 && s.grillEdits.empty());
    EditReset(s,net::Role::Client);test::stockReadCallback=[]{net::session_serial::Next();};
    E::BeforeChange(call,1);CHECK(E::g_captures.empty());
    // Reinstalling during the native mutation cannot acknowledge or publish old work.
    EditReset(s,net::Role::Host);E::Receive(Edit(1),1);
    test::stockEditCallback=[&]{net::session_serial::Next();E::Install(&s);};E::Tick();
    CHECK(test::stockWrites==1 && test::records==0 && s.grillEdits.empty() && E::g_host.empty());
    puts("PASS grill edits: native hooks, additive coal, field-only lid, FIFO/retries, retained results, actor/world/person/session-start fences and replay history");
}
