#define INSERT_READ_NATIVE
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
#ifndef SERVED_INSERT_SOURCE
#define SERVED_INSERT_SOURCE "served_insert.inc"
#endif
#include SERVED_INSERT_SOURCE
}
namespace test {
void ResetInsert(){Reset();insertFieldName=L"CallFunc_getMainPlayer_AsMain_Player_1";frameSize=64;energyOffset=8;energyWidth=8;energyType=L"ObjectProperty";}
}
int main(){using namespace test;uint8_t locals[64]{};auto* fn=&objects[200];namespace I=ue_wrap::kerfur;
 ResetInsert();CHECK(I::IsInsertPlayerRead(fn,locals,locals+8));CHECK(!I::IsInsertPlayerRead(fn,locals,locals+16));metadata=false;CHECK(!I::IsInsertPlayerRead(fn,locals,locals+8));metadata=true;++objects[100].serial;energyOffset=16;CHECK(I::IsInsertPlayerRead(fn,locals,locals+16));
 for(int fault=0;fault<7;++fault){ResetInsert();if(fault==0)frameSize=0;if(fault==1)frameSize=1024*1024+1;if(fault==2)energyOffset=-1;if(fault==3)energyOffset=60;if(fault==4)energyWidth=16;if(fault==5)energyType=L"Int64Property";if(fault==6)fields=false;CHECK(!I::IsInsertPlayerRead(fn,locals,locals+(fault==3?60:8)));}ResetInsert();insertFieldName=L"callfunc_getmainplayer_asmain_player_1";CHECK(I::IsInsertPlayerRead(fn,locals,locals+8));puts("PASS SF24");
 for(auto stage:{"frame","energyMeta","fields","outer","findUber"})for(int cause=0;cause<4;++cause){ResetInsert();bool fired=false,valid=true;hook=[&](const char* name){if(!fired&&std::string(name)==stage){fired=true;if(cause==0)++world;if(cause==1)++objects[100].serial;if(cause==2)++objects[200].serial;if(cause==3)valid=false;}};CHECK(!I::IsInsertPlayerReadWhile(fn,locals,locals+8,[&]{return valid;})&&fired);}
 ResetInsert();CHECK(!I::IsInsertPlayerRead(nullptr,locals,locals+8));CHECK(!I::IsInsertPlayerRead(fn,nullptr,locals+8));CHECK(!I::IsInsertPlayerRead(fn,locals,nullptr));puts("PASS SF25");puts("PASS actual insert-read layout: class/function lifetime, bounded pointer field and caller cancellation");
}
