#include "../../src/votv-coop/src/coop/world/event_active_sync.cpp"
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;const wchar_t* name=L"trigger_solarBoom_C";} objects[4];
struct Frame {uint8_t active=1;void* sender=&objects[1];};
uint32_t world=1,session=1;bool ready=true;int frameSize=sizeof(Frame);
std::vector<void*> members;std::function<void()> onRead;
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<4&&p==&test::objects[i]&&IsLive(p);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p;}
std::wstring ClassNameOf(void* p){return static_cast<test::Obj*>(p)->name;}
void* FindFunction(void*,const wchar_t*){return nullptr;}
int32_t FindParamOffset(void*,const wchar_t*){return -1;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace coop::net::session_serial {uint32_t Current(){return test::session;}}
namespace ue_wrap::active_events {
bool EnsureResolved(){return test::ready;}bool ReadCount(int32_t& out){out=1;return true;}
bool ReadSenders(Senders& out){out={test::members.data(),int32_t(test::members.size())};auto cb=std::move(test::onRead);test::onRead={};if(cb)cb();return true;}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return nullptr;}}
namespace coop::element {
Registry::Registry()=default;Registry::~Registry()=default;
Registry& Registry::Get(){static Registry r;return r;}Element* Registry::Get(ElementId)const{return nullptr;}
ElementId Registry::EidForActor(void*)const{return kInvalidId;}
}
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function&,int,PreFn,PostFn){return true;}
void ResolvePendingNames(){}bool NameWatchLive(const facts::Function&,int){return true;}
}
namespace ue_wrap::game_thread {bool IsGameThread(){return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::event_fire_sync {void ReplayInFlightRow(const std::string&){std::abort();}}
int main(){
 using namespace test;namespace E=coop::event_active_sync;
 for(int i=0;i<4;++i)objects[i].id=i;
 coop::net::Session s;E::Install(&s);
 E::g_setEventFn=&objects[0];E::g_offActive=offsetof(Frame,active);E::g_offContext=offsetof(Frame,sender);
 Frame f;ue_wrap::script_gate::Call call{};call.function=&objects[0];call.locals=reinterpret_cast<uint8_t*>(&f);
 E::OnSetEventPost(call);CHECK(E::g_began.size()==1);E::g_began.at(&objects[1]).ms=1;
 ++objects[1].serial;E::OnSetEventPost(call);CHECK(E::g_began.at(&objects[1]).ms>1);
 f.active=0;E::OnSetEventPost(call);CHECK(E::g_began.empty());
 call.locals=nullptr;E::OnSetEventPost(call);CHECK(E::g_began.empty());call.locals=reinterpret_cast<uint8_t*>(&f);
 f.active=1;frameSize=1;E::OnSetEventPost(call);CHECK(E::g_began.empty());frameSize=sizeof(Frame);
 members={&objects[1]};s.ok[1]=false;E::SendJoinSnapshotForSlot(1);CHECK(E::g_joinDebt[1].pending&&s.sent.empty());
 s.ok[1]=true;E::g_joinRetry={};E::Install(&s);CHECK(s.sent.size()==1&&!E::g_joinDebt[1].pending);
 s.sent.clear();s.ok[1]=false;E::SendJoinSnapshotForSlot(1);++s.generation[1];s.ok[1]=true;E::g_joinRetry={};E::Install(&s);
 CHECK(s.sent.empty()&&!E::g_joinDebt[1].pending);
 ready=false;E::SendJoinSnapshotForSlot(1);CHECK(E::g_joinDebt[1].pending);ready=true;E::g_joinRetry={};E::Install(&s);CHECK(s.sent.size()==1);
 s.sent.clear();members={&objects[1],&objects[2]};s.onSend=[] {members.clear();objects[2].live=false;};
 E::SendJoinSnapshotForSlot(1);CHECK(s.sent.size()==1);objects[2].live=true;
 s.sent.clear();members={&objects[1],&objects[2]};s.onSend=[] {E::OnDisconnect();};E::SendJoinSnapshotForSlot(1);
 CHECK(s.sent.size()==1&&!E::g_session);
 E::Install(&s);s.sent.clear();s.onSend=[] {++world;};E::SendJoinSnapshotForSlot(1);CHECK(s.sent.size()==1);
 E::Install(&s);s.sent.clear();s.onSend=[&] {++s.generation[1];};E::SendJoinSnapshotForSlot(1);CHECK(s.sent.size()==1);
 E::Install(&s);s.sent.clear();members.clear();E::SendJoinSnapshotForSlot(1);CHECK(!E::g_joinDebt[1].pending);
 E::OnDisconnect();puts("PASS active events: serial-safe begin/end, null/bounded frames, retained join retry, recycled recipients, readiness, copied membership and native teardown");
}
