#define OMEGA_COMMAND_FIXTURE
#define SERVED_PLAYER_NO_MAIN
#include "served_player_test.cpp"
#ifndef OMEGA_COMMAND_SOURCE
#define OMEGA_COMMAND_SOURCE "../../src/votv-coop/src/coop/creatures/kerfur_command.cpp"
#endif
#include OMEGA_COMMAND_SOURCE
#include "../../src/votv-coop/src/coop/net/refusal_tally.cpp"
namespace test {
namespace O=coop::kerfur_command;using C=O::Command;
bool jobReadable=true,verbAvailable=true,verbResult=true,kill=false,hasFloppy=false,consumeMirror=false;
uint8_t jobState=0;void* heldNow=nullptr;void* handMirror=nullptr;void* worldHeld=nullptr;uint32_t npcId=0;
int holdReads=0;C requested=C::Follow;int handSize=sizeof(Object);bool handSchema=true;uint8_t replacementSlot=31;
void ResetOmega(N::Session& s){hook={};O::OnDisconnect();ResetService(s);O::g_session.store(&s);O::BindExecution();
 O::g_killLive=O::g_killWatched=true;O::g_killRefused=false;O::g_killClass.Set(&objects[46]);O::g_killFn.Set(&objects[37]);
 coop::players::testLocal=&objects[7];coop::players::localHook=[]{Call("localPlayer");};
 for(auto& object:objects){object.hold=object.grab=object.viewer=nullptr;object.handName={};}
 objects[5].hold=&objects[9];objects[5].grab=&objects[10];objects[5].handName={11,2};objects[8].live=objects[6].live=true;
 jobReadable=verbAvailable=verbResult=handSchema=true;kill=hasFloppy=consumeMirror=false;jobState=0;heldNow=worldHeld=nullptr;handMirror=&objects[8];holdReads=0;requested=C::Follow;handSize=sizeof(Object);
 replacementSlot=31;npcId=Register(0,E::ElementType::Npc)->GetId();Register(8,E::ElementType::Prop);calls.clear();}
void Run(C command=C::Follow,uint8_t slot=1){requested=command;O::ExecuteHostCommand(npcId,&objects[0],command,slot);}
void Change(int cause){if(cause==0)++world;if(cause==1)++serial;if(cause==2)++objects[0].serial;if(cause==3)++objects[1].serial;if(cause==4){SP::Lease lease;
 if(SP::g_arming){replacementSlot=0;lease=SP::AssignOwned(SP::CaptureAssignment(&objects[0]),0,0);}else lease=Service(SP::kAll,31);CHECK(lease);replacementToken=lease.token;}}
void ReuseNpc(uint32_t id){owners.clear();for(uint32_t n=0;n<E::kHostRangeSize;++n){auto p=std::make_unique<E::Element>(E::ElementType::Npc);CHECK(E::Registry::Get().AllocHostId(p.get())!=E::kInvalidId);bool match=p->GetId()==id;if(match){p->SetTypeName("test_C");p->SetActor(&objects[0],0);objects[0].eid=id;}owners.push_back(std::move(p));if(match)break;}CHECK(E::Registry::Get().EidForActor(&objects[0])==id);}
ue_wrap::script_gate::Call KillCall(){ue_wrap::script_gate::Call c{};c.object=&objects[0];c.function=&objects[37];return c;}
}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*){}}
namespace coop::npc_sync {element::ElementId GetNpcIdForActor(void* actor){return element::Registry::Get().EidForActor(actor);}}
namespace coop::hand_item {void* MirrorActorForSlot(uint8_t){test::Call("handSource");return test::handMirror;}std::wstring HeldItem(uint8_t){test::Call("heldName");return L"floppy";}}
namespace coop::remote_prop {void* DrivenActor(int){test::Call("worldHeld");return test::worldHeld;}void EndAnyHoldOn(void*){test::Call("endHold");}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){test::Call("fname");return {77,1};}}
namespace ue_wrap::engine {bool SetActorSimulatePhysics(void*,bool simulate){CHECK(!simulate);test::Call("takePhysics");return true;}}
#ifndef HANDTAKEN_FIXTURE
namespace coop::local_streams {void ArmKerfurTakeRelease(void*){CHECK(false);}}
namespace ue_wrap::engine {
bool ReleaseMainPlayerGrabIfHolding(void*,void*){CHECK(false);return false;}
bool CallMainPlayerUpdateHold(void*){CHECK(false);return false;}
bool DestroyActor(void*){CHECK(false);return false;}
}
namespace ue_wrap::inventory {bool WriteHeldItem(const std::wstring&,const wchar_t*){CHECK(false);return false;}}
namespace ue_wrap::reflected_offset {int32_t MainPlayer_holding_actor(){CHECK(false);return -1;}}
namespace ue_wrap::engine {bool ReleaseMainPlayerGrabIfHoldingWhile(void*,void*,const std::function<bool()>&,bool*){CHECK(false);return false;}bool CallMainPlayerUpdateHoldWhile(void*,const std::function<bool()>&,bool*){CHECK(false);return false;}}
namespace ue_wrap::inventory {bool WriteHeldItemWhile(const std::wstring&,const wchar_t*,const std::function<bool()>&){CHECK(false);return false;}}
#endif
namespace ue_wrap::reflection {
int32_t StructSize(void*){
#ifdef HANDTAKEN_FIXTURE
test::Call("handLayout");
#endif
return test::handSize;}
std::vector<StructFieldInfo> EnumerateStructFields(void*){if(!test::handSchema)return {};return {
 {L"holding_actor",offsetof(test::Object,hold),sizeof(void*),0},{L"grabbing_actor",offsetof(test::Object,grab),sizeof(void*),0},{L"holding_name",offsetof(test::Object,handName),sizeof(FName),0}};}
void* FindDispatchFunctionCached(void*,const wchar_t*){return &test::objects[37];}
}
namespace ue_wrap::kerfur {
bool IsKerfurActor(void*){return true;}bool ReadKill(void*){test::Call("incomingKill");return test::kill;}
bool ReadKerfurState(void*,uint8_t& state,bool& spooky,uint8_t& face){test::Call("jobRead");state=test::jobState;spooky=false;face=0;return test::jobReadable;}
bool ReadHasFloppy(void*,bool& out){test::Call("floppyRead");out=test::hasFloppy;return true;}
bool ReadKerfurFloppy(void*,bool&,uint8_t&){CHECK(false);return false;}
void* ReadHoldObject(void*){++test::holdReads;test::Call(test::holdReads==1?"heldBefore":"heldAfter");return test::heldNow;}
bool RunActionNameWhile(void*,void*,const wchar_t*,const std::function<bool()>& current,bool* dispatched){
 if(dispatched)*dispatched=false;if(!current()||!test::verbAvailable)return false;
 test::Call("verbPrepare");if(!current())return false;
 if(dispatched)*dispatched=true;CHECK(!test::O::TryRecordMenuCommand(&test::objects[0],L"follow",false));
 if(test::requested==test::C::TakeObject)test::heldNow=test::objects[5].hold;
 if(test::requested==test::C::GetReports)test::hasFloppy=true;
 if(test::consumeMirror)test::objects[8].live=false;
 test::Call("verbDispatch");return current()&&test::verbResult;
}
}

#ifndef OMEGA_COMMAND_NO_MAIN
int main(){using namespace test;N::Session s,other;
 for(int n=1;n<=25;++n){if(n==23)continue;ResetOmega(s);
 switch(n){
 case 1: for(int cause=0;cause<7;++cause){ResetOmega(s);hook=[&](const std::string& name){if(name=="jobRead"){if(cause==0)++world;if(cause==1)++serial;if(cause==2)++s.stops;if(cause==3)s.linked=false;if(cause==4)s.mode=N::Role::Client;if(cause==5)O::g_session.store(&other);if(cause==6)worldObject=&objects[46];}};Run();CHECK(!Count("verbDispatch")&&!O::g_boundExecution->Current());}break;
 case 2: hook=[](const std::string& name){if(name=="jobRead"){ReuseNpc(npcId);}};Run();CHECK(!Count("verbDispatch"));break;
 case 3: for(int cause=0;cause<3;++cause){ResetOmega(s);O::g_killLive=false;O::g_killRefused=true;hook=[&](const std::string& name){if(name=="localPlayer"){if(cause==0)++s.generations[1];if(cause==1)++s.worldRevisions[1];if(cause==2)s.ready[1]=false;}};Run();CHECK(!Count("verbDispatch"));}break;
 case 4: {bool fired=false;hook=[&](const std::string& name){if(name=="jobRead"&&!fired){fired=true;Run();}};Run();CHECK(fired&&Count("verbDispatch")==1&&O::g_executions.empty());}break;
 case 5: {auto prior=Service(SP::kPlayerPawn,31);CHECK(prior);verbAvailable=false;Run();CHECK(SP::Owns(prior)&&SP::ServedSlot(&objects[0])==31);}break;
 case 6: {auto before=SP::CaptureAssignment(&objects[0]);auto fresh=Service(SP::kAll,31);CHECK(fresh&&!SP::AssignOwned(before,1,SP::kAll)&&SP::Owns(fresh));}break;
 case 7: {auto prior=Service(SP::kAll,31);verbAvailable=false;hook=[](const std::string& name){if(name=="localPlayer")++test::leaseSession->generations[31];};Run();CHECK(!SP::Owns(prior)&&SP::ServedSlot(&objects[0])==0&&SP::g_records.empty());}ResetOmega(s);{auto prior=Service(SP::kAll,1);SP::Lease newer;hook=[&](const std::string& name){if(name=="localPlayer")newer=Service(SP::kAll,31);};Run();hook={};CHECK(newer&&SP::Owns(newer)&&!SP::Owns(prior));}break;
 case 8: {auto prior=Service(SP::kAll,31);Run(C::Follow,0);CHECK(!SP::Owns(prior)&&SP::ServedSlot(&objects[0])==0);auto newer=Service(SP::kAll,1);CHECK(newer);auto old=SP::CaptureAssignment(&objects[0]);auto reserved=SP::AssignOwned(old,0,0);CHECK(reserved&&SP::Owns(reserved));auto newest=Service(SP::kAll,31);CHECK(!SP::RestoreOwned(reserved,old)&&SP::Owns(newest));}break;
 case 9: {auto prior=Service(SP::kAll,1);bool fired=false;hook=[&](const std::string& name){if(name=="armNative"&&!fired){fired=true;Change(4);}};O::OnStartKillPre(KillCall());hook={};CHECK(fired&&!SP::Owns(prior)&&SP::g_records.at(&objects[0])->lease.token==replacementToken);}break;
 case 10: O::g_killWatched=O::g_killLive=false;hook=[](const std::string& name){if(name=="watchMain")++world;};O::Install(&s);CHECK(!O::g_killWatched);break;
 case 11: O::t_inHostExec=true;Run();CHECK(O::t_inHostExec);O::t_inHostExec=false;break;
 case 12: jobReadable=false;Run();CHECK(!Count("verbDispatch")&&SP::ServedSlot(&objects[0])==0);ResetOmega(s);jobState=4;Run();CHECK(!Count("verbDispatch"));Run(C::Equipment);CHECK(Count("verbDispatch")==1);break;
 case 13: hook=[](const std::string& name){if(name=="localPlayer")++objects[7].serial;};Run();CHECK(Count("verbDispatch")==1);break;
 case 14: hook=[](const std::string& name){if(name=="handSource")++objects[5].serial;};Run(C::GetReports);CHECK(!Count("verbDispatch"));break;
 case 15: handSchema=false;Run(C::GetReports);CHECK(!Count("verbDispatch")&&objects[5].hold==&objects[9]);ResetOmega(s);handSize=4;Run(C::GetReports);CHECK(!Count("verbDispatch"));break;
 case 16: hook=[](const std::string& name){if(name=="verbDispatch")++objects[9].serial;};Run(C::GetReports);CHECK(objects[5].hold==nullptr);break;
 case 17: hook=[](const std::string& name){if(name=="verbDispatch")objects[5].hold=&objects[11];};Run(C::GetReports);CHECK(objects[5].hold==&objects[11]&&objects[5].handName.ComparisonIndex==11);break;
 case 18: hook=[](const std::string& name){if(name=="verbDispatch"){auto old=O::g_stages.at(&objects[5]);auto next=std::make_shared<O::StagedHand>(*old);next->attempt=++O::g_stageAttempt;O::g_stages[&objects[5]]=next;objects[5].hold=&objects[11];}};Run(C::GetReports);CHECK(objects[5].hold==&objects[11]&&O::g_stages.size()==1);break;
 case 19: hook=[](const std::string& name){if(name=="floppyRead")Change(4);};Run(C::GetReports);CHECK(!Count("verbDispatch")&&SP::ServedSlot(&objects[0])==31);break;
 case 20: hook=[](const std::string& name){if(name=="takePhysics")Change(4);};Run(C::TakeObject);CHECK(Count("takePhysics")==1&&!Count("endHold")&&s.releases.empty());break;
 case 21: hook=[](const std::string& name){if(name=="endHold")Change(4);};Run(C::TakeObject);CHECK(s.releases.empty()&&s.taken.empty());break;
 case 22: Run(C::GetReports,31);CHECK(s.taken.size()==1&&s.taken[0].cls.len);break;
 case 24: {auto prior=Service(SP::kAll,31);verbResult=false;Run();CHECK(Count("verbDispatch")==1&&!SP::Owns(prior)&&SP::ServedSlot(&objects[0])==1);}break;
 case 25: CHECK(O::TryRecordMenuCommand(&objects[0],L"follow",false));++serial;O::Tick();CHECK(!Count("verbDispatch")&&O::g_queue.empty());ResetOmega(s);{for(uint8_t command:{6,7,8}){N::KerfurCommandPayload p{};p.elementId=npcId;p.command=command;O::OnCommandRequest(p,1);}CHECK(O::g_slots[1].queue.size()==3);hook=[&](const std::string& name){if(name=="incomingKill")++s.generations[1];};O::ServeQueues();CHECK(!Count("verbDispatch"));}break;
 }Pass("OC",n);}
 const char* stages[]={"jobRead","armNative","localPlayer","handSource","floppyRead","verbPrepare","verbDispatch","heldAfter","takePhysics","handTaken"};
 for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){ResetOmega(s);bool fired=false;hook=[&](const std::string& name){if(!fired&&name==stages[stage]){fired=true;Change(cause);}};
 C command=(stage==3||stage==4||stage==9)?C::GetReports:(stage>=7?C::TakeObject:C::Follow);Run(command);CHECK(fired);hook={};
 CHECK(Count("verbDispatch")==int(stage>=6));if(stage==8)CHECK(!Count("endHold"));if(stage<9)CHECK(s.taken.empty());if(cause==4)CHECK(SP::ServedSlot(&objects[0])==replacementSlot);Pass("OX",stage*5+cause+1);
 }
 ResetOmega(s);O::OnDisconnect();SP::OnDisconnect();F::OnDisconnect();H::OnDisconnect();puts("PASS Omega transactions: 24 base cases and 50 replacement cells; real controller, registry and services");
}

#endif
