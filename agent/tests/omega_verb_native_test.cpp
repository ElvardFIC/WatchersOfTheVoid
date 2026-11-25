#define OMEGA_VERB_NATIVE
#define KERFUS_NATIVE_NO_MAIN
#include "kerfus_native_test.cpp"
#include "ue_wrap/actors/kerfur.h"
#include "ue_wrap/core/sdk_profile.h"
namespace ue_wrap::reflection {
void* OuterOf(void*){test::Call("outer");return &test::objects[100];}
void* FindFunction(void*,const wchar_t*){test::Call("findUber");return test::metadata?&test::objects[200]:nullptr;}
}
namespace ue_wrap::kerfur {
namespace R=ue_wrap::reflection;namespace P=ue_wrap::profile;
#ifndef OMEGA_VERB_SOURCE
#define OMEGA_VERB_SOURCE "omega_verb.inc"
#endif
#include OMEGA_VERB_SOURCE
}
namespace test {
void ResetVerb(){Reset();frameSize=64;playerOff=8;actionOff=24;playerBytes=8;actionBytes=16;playerType=L"ObjectProperty";actionType=L"StrProperty";expectedText=L"follow";expectedPlayer=&objects[5];}
}
int main(){using namespace test;namespace V=ue_wrap::kerfur;bool dispatched=false;ResetVerb();CHECK(V::RunActionNameWhile(&objects[0],expectedPlayer,L"follow",{},&dispatched)&&dispatched&&dispatches==1);
 for(int fault=0;fault<12;++fault){ResetVerb();if(fault==0)frameSize=0;if(fault==1)frameSize=4097;if(fault==2)playerOff=-1;if(fault==3)playerOff=60;if(fault==4)actionOff=-1;if(fault==5)actionOff=56;if(fault==6)actionOff=8;if(fault==7)playerBytes=4;if(fault==8)actionBytes=8;if(fault==9)playerType=L"Int64Property";if(fault==10)actionType=L"NameProperty";if(fault==11)metadata=false;dispatched=true;CHECK(!V::RunActionNameWhile(&objects[0],expectedPlayer,L"follow",{},&dispatched)&&!dispatched&&!dispatches);}
 for(auto stage:{"frame","playerField","actionField","playerSize","actionSize"})for(int cause=0;cause<5;++cause){ResetVerb();bool fired=false,valid=true;hook=[&](const char* name){if(!fired&&std::string(name)==stage){fired=true;if(cause==0)++world;if(cause==1)++objects[0].serial;if(cause==2)++objects[100].serial;if(cause==3)++objects[200].serial;if(cause==4)valid=false;}};CHECK(!V::RunActionNameWhile(&objects[0],expectedPlayer,L"follow",[&]{return valid;},&dispatched)&&fired&&!dispatched&&!dispatches);}
 ResetVerb();std::wstring name=L"follow";hook=[&](const char* stage){if(std::string(stage)=="frame")name=L"changed";};CHECK(V::RunActionNameWhile(&objects[0],expectedPlayer,name.c_str(),{},&dispatched)&&dispatched);
 ResetVerb();nativeResult=false;CHECK(!V::RunActionNameWhile(&objects[0],expectedPlayer,L"follow",{},&dispatched)&&dispatched&&dispatches==1);
 ResetVerb();expectedPlayer=nullptr;CHECK(V::RunActionName(&objects[0],nullptr,L"follow"));ResetVerb();std::wstring longName(64,L'x');CHECK(!V::RunActionNameWhile(&objects[0],expectedPlayer,longName.c_str(),{},&dispatched)&&!dispatched);puts("PASS OC23");puts("PASS actual Omega native replay: typed bounded frame, owned FString and dispatch receipt");
}
