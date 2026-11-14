#define HANDTAKEN_FIXTURE
#define OMEGA_COMMAND_NO_MAIN
#include "omega_command_test.cpp"
namespace test {
bool inventoryOK=true,refreshAvailable=true,refreshResult=true,releaseOK=true;
bool clientMode=false;int classReads=0;
N::HandTakenPayload Notice(){N::HandTakenPayload p{};const char* name="Aprop_C";p.cls.len=uint8_t(strlen(name));memcpy(p.cls.data,name,p.cls.len);return p;}
void ResetTaken(N::Session& s,bool client=true){ResetOmega(s);s.mode=client?N::Role::Client:N::Role::Host;O::BindExecution();
 clientMode=client;inventoryOK=refreshAvailable=refreshResult=releaseOK=true;classReads=0;
 objects[7].hold=&objects[9];objects[7].grab=&objects[10];objects[7].handName={11,2};calls.clear();}
size_t Debt(){size_t size=0;for(auto& q:O::g_notices)size+=q.size();return size;}
void Receive(){O::OnHandTaken(Notice());}
void ReplaceTaken(int cause,bool host){if(cause==0)++world;if(cause==1)++serial;if(cause==2)++objects[host?5:7].serial;if(cause==3)++objects[1].serial;
 if(cause==4){if(host){auto old=O::g_notices[1].front();auto next=std::make_shared<O::TakenNotice>(*old);O::g_notices[1].front()=next;}
 else{auto next=std::make_shared<O::TakenCleanup>(*O::g_takenCleanup);O::g_takenCleanup=next;}}}
}
namespace coop::local_streams {void ArmKerfurTakeRelease(void* actor){CHECK(actor==&test::objects[10]||actor==&test::objects[9]);test::Call("armRelease");}}
namespace ue_wrap::inventory {bool WriteHeldItemWhile(const std::wstring&,const wchar_t*,const std::function<bool()>& current){CHECK(current());test::Call("inventoryClear");return current()&&test::inventoryOK;}}
namespace ue_wrap::engine {
bool CallMainPlayerUpdateHoldWhile(void* body,const std::function<bool()>& current,bool* dispatched){*dispatched=false;if(!current()||!test::refreshAvailable)return false;*dispatched=true;test::Call("updateHold");if(!current())return false;static_cast<test::Object*>(body)->hold=nullptr;return test::refreshResult;}
bool ReleaseMainPlayerGrabIfHoldingWhile(void* body,void*,const std::function<bool()>& current,bool* dispatched){*dispatched=false;if(!current()||!test::releaseOK)return false;*dispatched=true;test::Call("releaseGrab");if(!current()||!test::releaseOK)return false;static_cast<test::Object*>(body)->grab=nullptr;return true;}
bool DestroyActor(void* actor){test::Call("destroyHeld");static_cast<test::Object*>(actor)->live=false;return true;}
}
int main(){using namespace test;N::Session s,other;
 for(int n=1;n<=25;++n){ResetTaken(s,n>=11);switch(n){
 case 1:s.sendOk=false;Run(C::GetReports);CHECK(Debt()==1&&s.taken.size()==1);s.sendOk=true;O::Tick();CHECK(Debt()==0&&s.taken.size()==2);break;
 case 2:for(int cause=0;cause<4;++cause){ResetTaken(s,false);s.sendOk=false;Run(C::GetReports);if(cause==0)++s.generations[1];if(cause==1)++s.worldRevisions[1];if(cause==2)s.generations[1]=0;if(cause==3)s.ready[1]=false;O::Tick();CHECK(Debt()==0&&s.taken.size()==1);}break;
 case 3:for(int cause=0;cause<8;++cause){ResetTaken(s,false);s.sendOk=false;Run(C::GetReports);if(cause==0)++world;if(cause==1)++serial;if(cause==2)++s.stops;if(cause==3)s.mode=N::Role::Client;if(cause==4)s.linked=false;if(cause==5)s.active=false;if(cause==6)O::g_session.store(&other);if(cause==7)worldObject=&objects[46];O::Tick();CHECK(Debt()==0&&s.taken.size()==1);}break;
 case 4:s.sendOk=false;for(int i=0;i<65;++i){hasFloppy=false;Run(C::GetReports);}CHECK(Debt()==64&&Count("verbDispatch")==64);break;
 case 5:s.endSendOk[1]=false;Run(C::GetReports,1);hasFloppy=false;Run(C::GetReports,31);CHECK(Debt()==1&&s.takenAttempts.back().slot==31&&s.takenAttempts.back().accepted);break;
 case 6:{bool fired=false;hook=[&](const std::string& name){if(name=="handTaken"&&!fired){fired=true;auto successor=std::make_shared<O::TakenNotice>(*O::g_notices[1].front());O::g_notices[1].push_back(successor);s.sendOk=false;}};Run(C::GetReports);CHECK(fired&&Debt()==1);hook={};s.sendOk=true;O::Tick();CHECK(Debt()==0);}break;
 case 7:hook=[](const std::string& name){if(name=="handTaken"){O::Tick();O::PumpTakenNotices();}};Run(C::GetReports);CHECK(s.taken.size()==1&&Debt()==0);break;
 case 8:s.sendOk=false;Run(C::GetReports);{auto packet=s.taken.front();for(int i=0;i<8;++i)O::Tick();CHECK(Debt()==1&&Count("verbDispatch")==1);for(auto p:s.taken)CHECK(!memcmp(&p,&packet,sizeof p));}break;
 case 9:for(int cause=0;cause<4;++cause){ResetTaken(s,false);s.sendOk=false;Run(C::GetReports);if(cause==0)++objects[5].serial;if(cause==1)++objects[1].serial;if(cause==2)puppet.body=&objects[6];if(cause==3)coop::players::testPuppets[1]=&otherPuppet;O::Tick();CHECK(Debt()==0&&s.taken.size()==1);}break;
 case 10:s.sendOk=false;Run(C::GetReports);SP::Unserve(&objects[0]);++objects[0].serial;s.sendOk=true;O::Tick();CHECK(Debt()==0&&s.taken.size()==2&&Count("verbDispatch")==1);break;
 case 11:for(int cause=0;cause<4;++cause){ResetTaken(s);if(cause==0)s.mode=N::Role::Host;if(cause==1)s.active=false;if(cause==2)s.linked=false;if(cause==3)O::OnDisconnect();Receive();CHECK(!Count("armRelease")&&!Count("inventoryClear"));}break;
 case 12:for(int cause=0;cause<4;++cause){ResetTaken(s);auto p=Notice();if(cause==0)p.cls.len=0;if(cause==1)p.cls.len=64;if(cause==2)p.cls.data[2]=0;if(cause==3)p.cls.data[2]=char(128);O::OnHandTaken(p);CHECK(!Count("inventoryClear")&&!O::g_takenCleanup);}break;
 case 13:{bool fired=false;hook=[&](const std::string& name){if(name=="inventoryClear"&&!fired){fired=true;inventoryOK=false;auto next=std::make_shared<O::TakenCleanup>(*O::g_takenCleanup);O::g_takenCleanup=next;}};Receive();CHECK(fired&&O::g_takenCleanup&&!Count("updateHold"));}break;
 case 14:hook=[](const std::string& name){if(name=="handLayout")coop::players::testLocal=&objects[6];};Receive();CHECK(!Count("inventoryClear"));break;
 case 15:handSchema=false;Receive();CHECK(!Count("armRelease")&&!O::g_takenCleanup);ResetTaken(s);handSize=4;Receive();CHECK(!Count("inventoryClear"));break;
 case 16:hook=[](const std::string& name){if(name=="armRelease")++objects[9].serial;};Receive();CHECK(!Count("inventoryClear")&&!Count("destroyHeld"));break;
 case 17:hook=[](const std::string& name){if(name=="className")++objects[9].serial;};Receive();CHECK(!Count("inventoryClear"));ResetTaken(s);{auto p=Notice();p.cls.data[0]='Z';O::OnHandTaken(p);CHECK(!Count("armRelease")&&!Count("inventoryClear"));}break;
 case 18:hook=[](const std::string& name){if(name=="armRelease")objects[7].hold=&objects[11];};Receive();CHECK(objects[7].hold==&objects[11]&&!Count("inventoryClear"));break;
 case 19:inventoryOK=false;Receive();CHECK(O::g_takenCleanup&&Count("armRelease")==1&&!Count("updateHold"));inventoryOK=true;O::Tick();CHECK(!O::g_takenCleanup&&Count("armRelease")==1&&Count("inventoryClear")==2&&Count("updateHold")==1);break;
 case 20:hook=[](const std::string& name){if(name=="inventoryClear")++world;};Receive();CHECK(!Count("updateHold"));break;
 case 21:refreshAvailable=false;Receive();CHECK(O::g_takenCleanup&&Count("inventoryClear")==1);refreshAvailable=true;refreshResult=false;O::Tick();CHECK(!O::g_takenCleanup&&Count("inventoryClear")==1&&Count("updateHold")==1);O::Tick();CHECK(Count("updateHold")==1);break;
 case 22:Receive();CHECK(!objects[9].live&&objects[10].live&&Count("destroyHeld")==1);ResetTaken(s);objects[7].grab=&objects[9];Receive();CHECK(objects[9].live&&!objects[7].hold&&!objects[7].grab&&!Count("destroyHeld")&&!Count("inventoryClear"));break;
 case 23:hook=[](const std::string& name){if(name=="releaseGrab")objects[7].grab=&objects[11];};Receive();CHECK(objects[7].grab==&objects[11]&&!Count("takePhysics"));break;
 case 24:releaseOK=false;Receive();CHECK(O::g_takenCleanup&&objects[7].grab==&objects[10]&&!Count("takePhysics"));releaseOK=true;O::Tick();CHECK(!O::g_takenCleanup&&!objects[7].grab&&Count("releaseGrab")==1&&Count("takePhysics")==1);break;
 case 25:inventoryOK=false;Receive();CHECK(O::g_takenCleanup);O::OnDisconnect();CHECK(!O::g_takenCleanup);ResetTaken(s);Receive();O::Tick();CHECK(Count("updateHold")==1&&Count("releaseGrab")==1&&Count("takePhysics")==1);break;
 }Pass("HD",n);}
 const char* stages[]={"handTaken","handLayout","className","className","armRelease","inventoryClear","updateHold","destroyHeld","releaseGrab","takePhysics"};
 const char* later[]={"handTaken","className","armRelease","armRelease","inventoryClear","updateHold","destroyHeld","releaseGrab","takePhysics","takePhysics"};
 for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){ResetTaken(s,stage!=0);bool fired=false;int names=0;
 hook=[&](const std::string& name){if(stage==2||stage==3){if(name=="className")++names;if(names!=(stage==2?1:2))return;}
 if(!fired&&name==stages[stage]){fired=true;ReplaceTaken(cause,stage==0);if(stage==0)s.sendOk=false;}};
 if(stage==0)Run(C::GetReports);else Receive();CHECK(fired);hook={};
 if(stage==0){CHECK(Debt()==size_t(cause==4));if(cause==4){s.sendOk=true;O::Tick();CHECK(!Debt());}}
 else {if(stage<9)CHECK(!Count(later[stage]));else CHECK(Count("takePhysics")==1);if(cause==4)CHECK(O::g_takenCleanup);else CHECK(!O::g_takenCleanup);}
 Pass("HX",stage*5+cause+1);
 }
 puts("PASS HandTaken: 25 cases and 50 continuation replacement cells; actual Omega controller and service/registry stack");
}
