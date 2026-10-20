#include "../../src/votv-coop/src/coop/session/teleport_client.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long CurrentThreadId(){return 0;}unsigned long long CallerRva(const void*){return 0;}}
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
uint64_t now=1000;uint32_t world=1;int player=0,fn=0,moves=0;bool moveOk=true;
ue_wrap::FVector position{};
struct Frame {ue_wrap::FTransform transform;bool useRotation,trueRotation;};
}
namespace ue_wrap::reflection {
void* ClassOf(void* p){return p;}void* FindFunction(void*,const wchar_t*){return &test::fn;}
int32_t FunctionFrameSize(void*){return sizeof(test::Frame);}
int32_t InternalIndexOf(void*){return 0;}int32_t AllocateSlotSerial(int32_t){return 1;}
int32_t SlotSerial(int32_t){return 1;}void* ObjectAt(int32_t){return &test::fn;}
std::vector<ParamInfo> FunctionParams(void*){
 std::vector<ParamInfo> r(3);r[0].name=L"NewTransform";r[0].offset=offsetof(test::Frame,transform);
 r[1].name=L"useRotation";r[1].offset=offsetof(test::Frame,useRotation);
 r[2].name=L"trueRotation";r[2].offset=offsetof(test::Frame,trueRotation);return r;
}
bool CallFunction(void*,void*,void* data){if(!test::moveOk)return false;
 const auto& f=*static_cast<test::Frame*>(data);CHECK(!f.useRotation&&!f.trueRotation);
 test::position={f.transform.TX,f.transform.TY,f.transform.TZ};++test::moves;return true;}
}
namespace ue_wrap::engine {
bool TeleportTo(void*,const FVector&,const FRotator&){return false;}bool SetActorLocation(void*,const FVector&){return false;}
bool SetActorRotation(void*,const FRotator&){return true;}void* GetController(void*){return nullptr;}
void SetControlRotation(void*,const FRotator&){}
bool TryGetActorLocation(void*,FVector&){return false;}bool TryGetActorRotation(void*,FRotator&){return false;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}}
namespace ue_wrap::game_thread {void Post(Task t){t();}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
int main(){using namespace test;namespace T=coop::teleport_client;
 coop::net::Session s;coop::players::Registry::Get().local=&player;T::SetSession(&s);
 // Queue pressure retries, successful target gets exactly one message, initiator never moves.
 s.sendOk[1]=false;T::GatherOthersTo(0,100,200,300,45);CHECK(s.sent.empty()&&!moves);
 s.sendOk[1]=true;T::TickGather();CHECK(s.sent.empty());now+=201;T::TickGather();CHECK(s.sent.size()==1);
 coop::net::TeleportClientPayload p{};std::memcpy(&p,s.sent[0].bytes.data(),sizeof(p));
 CHECK(p.locZ==300&&p.rotYaw==45&&s.sent[0].slot==1&&s.sent[0].kind==coop::net::ReliableKind::TeleportClient);
 now+=1000;T::TickGather();CHECK(s.sent.size()==1);
 // A client initiator moves the host through the actual native teleport path.
 T::GatherOthersTo(1,1000,2000,3000,20);CHECK(moves==1&&position.Z==3000&&s.sent.size()==1);
 moveOk=false;T::GatherOthersTo(1,1000,2000,4000,20);CHECK(moves==1);
 moveOk=true;now+=201;T::TickGather();CHECK(moves==2&&position.Z==4000);
 // A newer anchor supersedes an older failed target; that new initiator is not dragged back.
 s.sendOk[1]=false;T::GatherOthersTo(0,1,2,3,0);T::GatherOthersTo(1,4,5,6,0);
 s.sendOk[1]=true;now+=201;T::TickGather();CHECK(s.sent.size()==1&&moves==3);
 // World/readiness/deadline/disconnect discard old requests instead of replaying in a new world.
 s.sendOk[1]=false;T::GatherOthersTo(0,1,2,3,0);++world;s.sendOk[1]=true;now+=201;T::TickGather();CHECK(s.sent.size()==1);
 s.sendOk[1]=false;T::GatherOthersTo(0,1,2,3,0);s.ready[1]=false;T::TickGather();s.ready[1]=s.sendOk[1]=true;now+=201;T::TickGather();CHECK(s.sent.size()==1);
 s.sendOk[1]=false;T::GatherOthersTo(0,1,2,3,0);s.sendOk[1]=true;now+=5001;T::TickGather();CHECK(s.sent.size()==1);
 s.sendOk[1]=false;T::GatherOthersTo(0,1,2,3,0);s.linked=false;T::TickGather();s.linked=s.sendOk[1]=true;now+=201;T::TickGather();CHECK(s.sent.size()==1);
 // Reentrant send must not erase a newer gather installed by its callback.
 s.onSend=[&]{s.sendOk[1]=false;T::GatherOthersTo(0,10,20,999,90);};
 T::GatherOthersTo(0,1,2,3,0);CHECK(s.sent.size()==2);s.sendOk[1]=true;now+=201;T::TickGather();CHECK(s.sent.size()==3);
 std::memcpy(&p,s.sent.back().bytes.data(),sizeof(p));CHECK(p.locZ==999);
 // Invalid inputs and a client role cannot issue a gather.
 T::GatherOthersTo(255,1,2,3,0);T::GatherOthersTo(0,std::numeric_limits<float>::quiet_NaN(),2,3,0);
 s.mode=coop::net::Role::Client;T::GatherOthersTo(0,1,2,3,0);CHECK(s.sent.size()==3);
 s.mode=coop::net::Role::Host;s.sendOk[1]=false;T::GatherOthersTo(0,1,2,3,0);T::ClearGather();s.sendOk[1]=true;now+=201;T::TickGather();CHECK(s.sent.size()==3);
 puts("PASS gather: native host movement, retry without duplicate, initiator protection, supersession/reentry, world/readiness/deadline/disconnect, invalid inputs and roles");
}
