#define KERFUS_ACTION_NATIVE
#define KERFUS_NATIVE_NO_MAIN
#include "kerfus_native_test.cpp"
namespace test {
void ResetAction(){Reset();frameSize=32;playerOff=8;actionOff=2;playerBytes=8;actionBytes=1;playerType=L"ObjectProperty";actionType=L"ByteProperty";expectedPlayer=&objects[5];expectedAction=8;}
bool Action(bool& dispatched,const std::function<bool()>& current={}){return K::RunActionOptionIndexWhile(&objects[0],expectedPlayer,expectedAction,current,&dispatched);}
}
int main(){using namespace test;bool dispatched=false;
 ResetAction();CHECK(Action(dispatched)&&dispatched&&dispatches==1);
 for(auto stage:{"frame","playerField","actionField","playerSize","actionSize"})for(int cause=0;cause<5;++cause){ResetAction();bool fired=false;hook=[&](const char* name){if(!fired&&std::string(name)==stage){fired=true;if(cause==0)++world;if(cause==1)++objects[0].serial;if(cause==2)++objects[100].serial;if(cause==3)++objects[200].serial;if(cause==4)++objects[5].serial;}};dispatched=true;CHECK(!Action(dispatched)&&fired&&!dispatched&&!dispatches);}puts("PASS KA20");
 for(int fault=0;fault<12;++fault){ResetAction();if(fault==0)frameSize=0;if(fault==1)frameSize=4097;if(fault==2)playerOff=-1;if(fault==3)playerOff=25;if(fault==4)actionOff=-1;if(fault==5)actionOff=32;if(fault==6)actionOff=8;if(fault==7)playerBytes=4;if(fault==8)actionBytes=2;if(fault==9)playerType=L"Int64Property";if(fault==10)actionType=L"BoolProperty";if(fault==11)actionType=L"StrProperty";dispatched=true;CHECK(!Action(dispatched)&&!dispatched&&!dispatches);}ResetAction();actionType=L"EnumProperty";CHECK(Action(dispatched)&&dispatched);puts("PASS KA21");
 ResetAction();nativeResult=false;dispatched=false;CHECK(!Action(dispatched)&&dispatched&&dispatches==1);ResetAction();hook=[](const char* n){if(std::string(n)=="dispatch")++world;};CHECK(!Action(dispatched)&&dispatched&&dispatches==1);ResetAction();bool valid=true;hook=[&](const char* n){if(std::string(n)=="actionField")valid=false;};CHECK(!Action(dispatched,[&]{return valid;})&&!dispatched&&!dispatches);puts("PASS KA22");
 ResetAction();metadata=false;CHECK(!Action(dispatched)&&!dispatched);metadata=true;CHECK(Action(dispatched)&&dispatched);ResetAction();expectedPlayer=nullptr;expectedAction=14;CHECK(K::RunActionOptionIndex(&objects[0],nullptr,14)&&dispatches==1);ResetAction();++objects[200].serial;functionSerial=objects[200].serial;CHECK(Action(dispatched)&&dispatched);puts("PASS KA23");
 puts("PASS Kerfus native action wrapper: real frame validation, weak identities, dispatch receipt and legacy facade");
}
