#define KERFUS_COMMAND_FIXTURE
#define KERFUS_STATE_NO_MAIN
#include "kerfus_state_test.cpp"
#include "coop/element/player.h"
#ifndef KERFUS_INTENT_SOURCE
#define KERFUS_INTENT_SOURCE "../../src/votv-coop/src/coop/creatures/kerfus_intent.cpp"
#endif
#include KERFUS_INTENT_SOURCE
namespace test {
namespace I=coop::kerfus_intent;using V=ue_wrap::script_gate::Verdict;
bool hasBody=true,reach=true,actionAvailable=true,actionResult=true,invokeGate=true;
int actionOffset=2,nameOffset=8,callSize=32,actionWidth=1,nameWidth=16;
std::wstring actionType=L"ByteProperty",nameType=L"StrProperty";
std::vector<uint8_t> pressers;uint32_t commandId=1;
void ResetIntent(N::Session& s,bool client=false){I::OnDisconnect();ResetState(s,client);I::g_session.store(&s);I::g_actionWatched=I::g_nameWatched=I::g_saidLive=true;
 coop::players::testLocal=&objects[5];coop::players::localHook=[]{Call("localPlayer");};hasBody=reach=actionAvailable=actionResult=invokeGate=true;actionOffset=2;nameOffset=8;callSize=32;actionWidth=1;nameWidth=16;actionType=L"ByteProperty";nameType=L"StrProperty";pressers.clear();commandId=1;calls.clear();}
N::KerfusIntentPayload Command(uint32_t id=1,uint8_t action=8){N::KerfusIntentPayload p{};p.elementId=id;p.action=action;return p;}
void Host(N::Session& s,uint8_t action=8,uint8_t slot=1){commandId=Register(0,E::ElementType::Prop)->GetId();I::OnIntent(s,Command(commandId,action),slot);}
ue_wrap::script_gate::Call Open(uint8_t* bytes,bool named=false,int actor=0){ue_wrap::script_gate::Call c{};c.object=&objects[actor];c.function=&objects[named?43:42];c.locals=bytes;return c;}
V Press(uint8_t action=8){uint8_t bytes[32]{};bytes[2]=action;return I::OnActionPre(Open(bytes));}
V Named(const wchar_t* text){struct Header{const wchar_t* data;int32_t num,max;};uint8_t bytes[32]{};Header h{text,int32_t(std::wcslen(text)+1),int32_t(std::wcslen(text)+1)};std::memcpy(bytes+8,&h,sizeof h);return I::OnNamePre(Open(bytes,true));}
size_t QueueSize(int slot=1){return I::g_slots[slot]?I::g_slots[slot]->queue.size():0;}
void IntentTick(N::Session& s){now+=250;I::Tick(s);}
}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session& s,uint8_t slot,float reach){CHECK(reach==400);IntentTarget t;t.session_=&s;t.slot_=slot;t.reachUU_=reach;return t;}
IntentSubject IntentTarget::Authorize(void* actor)const{test::Call("reach");IntentSubject s;s.actor=actor;s.slot=slot_;s.outcome=!test::hasBody?IntentOutcome::NoBody:!test::reach?IntentOutcome::OutOfReach:IntentOutcome::Ok;return s;}
}
namespace ue_wrap::reflection {
void* FindDispatchFunctionCached(void*,const wchar_t* name){return std::wstring(name)==L"actionName"?&test::objects[43]:&test::objects[42];}
int32_t FunctionFrameSize(void*){return test::callSize;}
int32_t FindParamOffset(void*,const wchar_t* name){const bool named=std::wstring(name)==L"name";test::Call(named?"nameMeta":"actionMeta");return named?test::nameOffset:test::actionOffset;}
int32_t FindParamSize(void*,const wchar_t* name){return std::wstring(name)==L"name"?test::nameWidth:test::actionWidth;}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){const bool named=std::wstring(name)==L"name";offset=named?test::nameOffset:test::actionOffset;type=named?test::nameType:test::actionType;return true;}
}
namespace coop::kerfus_follow {void OnToggle(void*,uint8_t slot){test::pressers.push_back(slot);test::Call("attribution");}}
namespace ue_wrap::kerfus {
bool RunActionOptionIndexWhile(void* actor,void* player,uint8_t action,const std::function<bool()>& current,bool* dispatched){
 if(dispatched)*dispatched=false;if(!current()||!test::actionAvailable)return false;
 test::Call("actionFunction");if(!current())return false;test::Call("actionParam");if(!current())return false;
 CHECK(player==&test::objects[5]);if(dispatched)*dispatched=true;
 if(test::invokeGate){uint8_t bytes[32]{};bytes[2]=action;auto c=test::Open(bytes);c.object=actor;c.fromOurCode=true;if(test::I::OnActionPre(c)==test::V::Cancel)return false;}
 test::Call("actionDispatch");return current()&&test::actionResult;
}
}
int main(){using namespace test;N::Session s,other;
 for(int n=1;n<=25;++n){if(n>=20&&n<=23)continue;ResetIntent(s);
 switch(n){
 case 1: for(int cause=0;cause<7;++cause){ResetIntent(s);Host(s);if(cause==0)++world;if(cause==1)++serial;if(cause==2)++s.stops;if(cause==3)s.mode=N::Role::Client;if(cause==4)s.linked=false;if(cause==5)I::g_session.store(&other);if(cause==6)worldObject=&objects[46];I::Tick(s);CHECK(!Count("actionDispatch")&&QueueSize()==0);}break;
 case 2: Host(s);{auto row=I::g_slots[1];hook=[&](const std::string& name){if(name=="reach"){I::OnDisconnect();CHECK(row->queue.size()==1);}};I::Tick(s);CHECK(!Count("actionDispatch")&&QueueSize()==0);}break;
 case 3: for(int cause=0;cause<4;++cause){ResetIntent(s);Host(s);if(cause==0)++s.generations[1];if(cause==1)++s.worldRevisions[1];if(cause==2)s.generations[1]=0;if(cause==3)s.ready[1]=false;I::Tick(s);CHECK(!Count("actionDispatch")&&QueueSize()==0);}break;
 case 4: {commandId=Register(0,E::ElementType::Prop)->GetId();for(auto id:{0u,E::kHostRangeSize})I::OnIntent(s,Command(id),1);for(uint8_t a:{0,14,255})I::OnIntent(s,Command(commandId,a),1);auto p=Command(commandId);p._pad[1]=1;I::OnIntent(s,p,1);I::OnIntent(s,Command(commandId),0);I::OnIntent(s,Command(commandId),32);CHECK(!QueueSize());}break;
 case 5: for(int cause=0;cause<5;++cause){ResetIntent(s);Host(s);Replace(cause,false,commandId);I::Tick(s);CHECK(!Count("actionDispatch"));}break;
 case 6: Host(s);hook=[](const std::string& name){if(name=="reach")owners.clear();};I::Tick(s);CHECK(!Count("classGate")&&!Count("actionDispatch"));break;
 case 7: Host(s);hasBody=false;for(int i=0;i<20;++i)I::Tick(s);CHECK(QueueSize()==1&&!Count("actionDispatch"));hasBody=true;actionAvailable=false;I::Tick(s);CHECK(QueueSize()==1&&!Count("actionDispatch"));actionAvailable=true;I::Tick(s);CHECK(Count("actionDispatch")==1&&!QueueSize());break;
 case 8: Host(s);for(int i=0;i<15;++i)I::OnIntent(s,Command(commandId,6),1);for(int i=0;i<11;++i)I::Tick(s);CHECK(Count("actionDispatch")==10&&QueueSize()==6);now+=100;I::Tick(s);CHECK(Count("actionDispatch")==11&&QueueSize()==5);break;
 case 9: {Host(s);for(int i=0;i<30;++i)I::OnIntent(s,Command(commandId,6),1);I::OnIntent(s,Command(commandId,4),31);CHECK(QueueSize()==16&&QueueSize(31)==1);bool first=true;hook=[&](const std::string& name){if(name=="reach"){hasBody=!first;first=false;}};I::Tick(s);CHECK(QueueSize()==16&&!QueueSize(31)&&Count("actionDispatch")==1);}break;
 case 10: Host(s);hook=[&](const std::string& name){if(name=="reach"){I::Tick(s);I::OnDisconnect();CHECK(I::g_pumping);}};I::Tick(s);CHECK(!I::g_pumping&&!Count("actionDispatch")&&!QueueSize());break;
 case 11: ResetIntent(s,true);Mirror();s.sendOk=false;CHECK(Press()==V::Cancel&&Named(L"pat")==V::Cancel);CHECK(I::g_client.size()==2);s.sendOk=true;IntentTick(s);CHECK(I::g_client.empty()&&s.commands.size()==3&&s.commands[1].packet.action==8&&s.commands[2].packet.action==6);ResetIntent(s,true);Mirror();s.sendOk=false;for(int i=0;i<20;++i)Press(6);CHECK(I::g_client.size()==16);s.sendOk=true;IntentTick(s);CHECK(I::g_client.size()==8);I::Tick(s);CHECK(I::g_client.empty());break;
 case 12: ResetIntent(s,true);Mirror();s.sendOk=false;Press();I::Tick(s);CHECK(s.commands.size()==1);{bool fired=false;s.sendOk=true;hook=[&](const std::string& name){if(name=="commandSend"&&!fired){fired=true;Press(6);I::Tick(s);}};IntentTick(s);CHECK(fired&&I::g_client.size()==1);hook={};IntentTick(s);CHECK(I::g_client.empty()&&s.commands.back().packet.action==6);}break;
 case 13: for(int cause=0;cause<5;++cause){ResetIntent(s,true);Mirror();s.sendOk=false;Press();Replace(cause,true,1);s.sendOk=true;IntentTick(s);CHECK(s.commands.size()==1&&I::g_client.empty());}break;
 case 14: for(int fault=0;fault<8;++fault){ResetIntent(s,true);Mirror();uint8_t bytes[32]{};bytes[2]=8;auto c=Open(bytes);if(fault==0)c.locals=nullptr;if(fault==1)actionOffset=-1;if(fault==2)actionOffset=32;if(fault==3)callSize=0;if(fault==4)callSize=4097;if(fault==5)actionWidth=2;if(fault==6)actionType=L"BoolProperty";if(fault==7)++objects[42].serial,c.function=&objects[43];CHECK(I::OnActionPre(c)==V::Cancel&&s.commands.empty());}break;
 case 15: {ResetIntent(s,true);Mirror();struct Header{const wchar_t* data;int32_t num,max;};for(int fault=0;fault<6;++fault){uint8_t bytes[32]{};wchar_t text[2]{L'x',0};Header h{text,2,2};if(fault==0)h.num=65;if(fault==1)h.max=1;if(fault==2)h.num=-1;if(fault==3)h.data=nullptr;if(fault==4)h.data=reinterpret_cast<wchar_t*>(1);if(fault==5)text[1]=L'x';std::memcpy(bytes+8,&h,16);CHECK(I::OnNamePre(Open(bytes,true))==V::Cancel&&s.commands.empty());}CHECK(Named(L"activate")==V::Cancel&&s.commands.back().packet.action==8);}break;
 case 16: ResetIntent(s,true);Mirror();CHECK(Press(14)==V::Run&&Named(L"colour")==V::Run&&s.commands.empty());s.active=false;CHECK(Press()==V::Run);break;
 case 17: ResetIntent(s,true);Mirror();worldObject=nullptr;CHECK(Press()==V::Cancel&&s.commands.empty());worldObject=&objects[47];s.linked=false;CHECK(Press()==V::Cancel&&s.commands.empty());s.linked=true;{uint8_t bytes[32]{};bytes[2]=8;auto c=Open(bytes);c.fromOurCode=true;CHECK(I::OnActionPre(c)==V::Cancel&&s.commands.empty());}break;
 case 18: ResetIntent(s,true);Mirror();hook=[](const std::string& name){if(name=="actionMeta")++objects[42].serial;};CHECK(Press()==V::Cancel&&s.commands.empty());break;
 case 19: Host(s);actionResult=false;I::Tick(s);I::Tick(s);CHECK(Count("actionDispatch")==1&&pressers.size()==1&&pressers[0]==1&&!I::g_running);CHECK(Press()==V::Run&&pressers.back()==0);break;
 case 24: Host(s);I::g_actionWatched=I::g_nameWatched=I::g_saidLive=false;hook=[](const std::string& name){if(name=="watch")++world;};I::Tick(s);CHECK(!I::g_actionWatched&&!Count("reach"));ResetIntent(s);I::g_actionWatched=I::g_nameWatched=false;{bool fired=false;hook=[&](const std::string& name){if(name=="watch"&&!fired){fired=true;I::OnDisconnect();I::Install(&s);}};I::Install(&s);CHECK(fired&&!I::g_actionWatched&&Count("watch")==1);hook={};I::Tick(s);CHECK(I::g_actionWatched&&I::g_nameWatched);}break;
 case 25: Host(s);{bool fired=false;hook=[&](const std::string& name){if(name=="actionDispatch"&&!fired){fired=true;I::OnDisconnect();I::OnIntent(s,Command(commandId,6),1);I::Tick(s);}};I::Tick(s);CHECK(fired&&QueueSize()==1&&!I::g_run);hook={};I::Tick(s);CHECK(Count("actionDispatch")==2&&!QueueSize());}break;
 }Pass("KA",n);}
 const char* stages[]={"actionMeta","nameMeta","commandSend","clock","reach","classGate","localPlayer","actionFunction","actionParam","actionDispatch"};
 for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){bool client=stage<3;ResetIntent(s,client);if(client)Mirror();else Host(s);bool fired=false;hook=[&](const std::string& name){if(!fired&&name==stages[stage]){fired=true;Replace(cause,client,client?1:commandId);}};
 if(client){if(stage==1)CHECK(Named(L"activate")==V::Cancel);else CHECK(Press()==V::Cancel);}else I::Tick(s);
 CHECK(fired);hook={};CHECK(Count("actionDispatch")==int(stage==9));if(client)CHECK(s.commands.size()==size_t(stage==2));else CHECK(!I::g_run);Pass("KB",stage*5+cause+1);
 }
 ResetIntent(s);H::OnDisconnect();puts("PASS Kerfus commands: 21 base cases and 50 replacement cells; actual controller, Registry and claims");
}
