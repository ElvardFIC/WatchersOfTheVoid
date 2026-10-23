// Native callback lifetime tests. The controller, command table, codecs and
// chunk transport are real; UObject state and native UI calls are test doubles.
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"
#include "coop/net/session.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace test {
const char* scenario="setup";
struct Obj {int id,serial=1;bool live=true,busy=false;int pins=0,reads=0;std::wstring command=L"sv.ping",log;};
struct Args {std::wstring text;uint8_t type=0;void* owner=nullptr;};
struct Watch {int tag;ue_wrap::script_gate::PreFn pre;ue_wrap::script_gate::PostFn post;};
std::map<std::wstring,Watch> watches;
std::set<uint64_t> active;
uint64_t sequence=0;
std::deque<Obj> objects;
std::vector<Obj*> slots{nullptr};
Obj* local=nullptr;
coop::net::Session session,other;
uint32_t world=1,serial=1;
std::array<std::string,coop::net::kMaxPeers> guids;
std::function<void(const char*,void*)> onNative;
std::function<bool(void*)> onConsume;
std::function<bool(void*,const std::wstring&,uint8_t)> onPrint;
std::function<void(void*,const std::wstring&,uint8_t)> beforePrint;
std::vector<std::wstring> printAttempts;
int consumes=0,nativeRuns=0;
bool failWriteWatch=false;
void Fail(int line,const char* expression){std::fprintf(stderr,"FAIL %s line %d: %s\n",scenario,line,expression);std::exit(1);}
Obj* Make(){objects.push_back({int(slots.size())});auto* p=&objects.back();slots.push_back(p);return p;}
void Native(const char* name,void* p){if(p){auto* obj=static_cast<Obj*>(p);if(!obj->live)Fail(__LINE__,"native call on retired widget");++obj->reads;}const auto callback=onNative;if(callback)callback(name,p);}
}
#define CHECK(expression) do{if(!(expression))test::Fail(__LINE__,#expression);}while(0)
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return i>0&&size_t(i)<test::slots.size()?test::slots[i]->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>0&&size_t(i)<test::slots.size()&&test::slots[i]==p&&test::slots[i]->live;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
#include "facts_test_support.h"
#ifndef SAT_CONSOLE_SOURCE
#define SAT_CONSOLE_SOURCE "../../src/votv-coop/src/coop/interactables/sat_console_sync.cpp"
#endif
#include SAT_CONSOLE_SOURCE
#include "../../src/votv-coop/src/coop/net/blob_chunks.cpp"
#include "../../src/votv-coop/src/coop/items/save_record_wire.cpp"
#include "../../src/votv-coop/src/coop/interactables/signal_wire.cpp"
#include "../../src/votv-coop/src/coop/interactables/sat_console_table.cpp"

namespace test {
namespace S=coop::sat_console_sync;
namespace W=coop::save_record_wire;
namespace B=coop::blob_chunks;
namespace sg=ue_wrap::script_gate;
using coop::net::Role;
using V=sg::Verdict;
struct Frame {Args args;sg::Call call{};Watch watch{};bool runs=false;};
V Enter(Frame& f,void* object,const std::wstring& line,const wchar_t* fn=L"enterCommand",bool ours=false){
 f.args={line,0,object};f.watch=watches.at(fn);f.call={};f.call.object=object;f.call.locals=reinterpret_cast<uint8_t*>(&f.args);
 f.call.tag=f.watch.tag;f.call.invocation=++sequence;f.call.fromOurCode=ours;active.insert(f.call.invocation);
 const auto verdict=f.watch.pre?f.watch.pre(f.call):V::Run;f.runs=verdict==V::Run;if(!f.runs)active.erase(f.call.invocation);return verdict;
}
void Post(Frame& f){if(f.watch.post)f.watch.post(f.call);active.erase(f.call.invocation);}
V Write(void* object,const std::wstring& text,uint8_t type=0,bool ours=false){
 Args args{text,type,object};auto watch=watches.at(L"writeToLog");sg::Call call{};call.object=object;call.locals=reinterpret_cast<uint8_t*>(&args);
 call.tag=watch.tag;call.fromOurCode=ours;call.invocation=++sequence;active.insert(call.invocation);
 const auto result=watch.pre?watch.pre(call):V::Run;active.erase(call.invocation);return result;
}
}
namespace ue_wrap {
bool GcPin::Pin(void* p){Release();obj_=p;if(p)++static_cast<test::Obj*>(p)->pins;return p!=nullptr;}
void GcPin::Release(){if(obj_)--static_cast<test::Obj*>(obj_)->pins;obj_=nullptr;}
namespace game_thread {bool IsGameThread(){return true;}}
namespace script_gate {
bool WatchName(const facts::Function& fn,int tag,PreFn pre,PostFn post){if(test::failWriteWatch&&std::wstring(fn.Name())==L"writeToLog")return false;test::watches[fn.Name()]={tag,pre,post};return true;}
bool UnwatchName(const facts::Function& fn,int tag,PreFn pre,PostFn post){const auto found=test::watches.find(fn.Name());if(found==test::watches.end()||found->second.tag!=tag||found->second.pre!=pre||found->second.post!=post)return false;test::watches.erase(found);return true;}
bool IsInvocationActive(uint64_t id){return test::active.count(id)!=0;}
}
namespace console_desk {void* Instance(){return test::local;}}
namespace dish {int IndexOf(void*){return -1;}void* DishByIndex(int){return nullptr;}int Count(){return 0;}}
namespace sat_console {
bool EnsureResolved(){return true;}
void* LocalTerminal(){return test::local;}
void* CreateTerminal(void*){return test::Make();}
void DiscardTerminal(void* p){if(p)static_cast<test::Obj*>(p)->live=false;}
bool ReadProcessing(void* p,bool& busy){test::Native("ReadProcessing",p);busy=static_cast<test::Obj*>(p)->busy;return true;}
bool WriteProcessing(void* p,bool busy){test::Native("WriteProcessing",p);static_cast<test::Obj*>(p)->busy=busy;return true;}
bool WritePanel(void*,void*){return true;}
bool ReadEnterCommandLine(const uint8_t* raw,std::wstring& line){auto* args=reinterpret_cast<const test::Args*>(raw);line=args->text;test::Native("ReadEnterCommandLine",args->owner);return true;}
bool ReadWriteToLogArgs(const uint8_t* raw,std::wstring& text,uint8_t& type){const auto* args=reinterpret_cast<const test::Args*>(raw);text=args->text;type=args->type;test::Native("ReadWriteToLogArgs",args->owner);return true;}
bool ReadName(void* p,std::wstring& name){name=L"root";test::Native("ReadName",p);return true;}
bool ReadLog(void* p,std::wstring& log){log=static_cast<test::Obj*>(p)->log;return true;}
void* ReadUsed(void* p){test::Native("ReadUsed",p);return nullptr;}
void* ReadActiveDish(void* p){test::Native("ReadActiveDish",p);return nullptr;}
bool WriteUsed(void*,void*){return true;}
std::wstring PanelName(void* p){test::Native("PanelName",p);return {};}
void* FindPanelByName(const std::wstring&){return nullptr;}
bool CallInit(void*,const std::wstring&,bool,void*){return true;}
bool ConsumeInput(void* p){test::Native("ConsumeInput",p);++test::consumes;const auto callback=test::onConsume;if(callback&&!callback(p))return false;static_cast<test::Obj*>(p)->command.clear();return true;}
bool ConsumeInputWhile(void* p,const std::function<bool()>& current){return current&&current()&&ConsumeInput(p)&&current();}
bool CallWriteToLog(void* p,const std::wstring& text,uint8_t type){
 test::printAttempts.push_back(text);test::Args args{text,type,p};const auto hook=test::watches.at(L"writeToLog");script_gate::Call call{};
 call.object=p;call.locals=reinterpret_cast<uint8_t*>(&args);call.tag=hook.tag;call.fromOurCode=true;call.invocation=++test::sequence;test::active.insert(call.invocation);
 const auto before=test::beforePrint;if(before)before(p,text,type);
 const auto verdict=hook.pre?hook.pre(call):script_gate::Verdict::Run;
 const auto callback=test::onPrint;const bool success=verdict==script_gate::Verdict::Cancel||!callback||callback(p,text,type);
 if(success&&verdict==script_gate::Verdict::Run)static_cast<test::Obj*>(p)->log+=text;
 test::active.erase(call.invocation);return success;
}
bool ClearLog(void* p){static_cast<test::Obj*>(p)->log.clear();return CallWriteToLog(p,L"",0);}
bool RunCommand(void*,const std::wstring&){++test::nativeRuns;return true;}
}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace coop::player_handshake {const std::string& GuidForSlot(int slot){return test::guids.at(slot);}}

namespace test {
void Reset(Role role=Role::Client){
 onNative={};onConsume={};onPrint={};beforePrint={};session.onSend={};other.onSend={};S::OnDisconnect();
 for(const auto& p:objects)CHECK(p.pins==0);
 objects.clear();slots.resize(1);local=Make();world=1;++serial;consumes=nativeRuns=0;active.clear();printAttempts.clear();
 session=coop::net::Session{};other=coop::net::Session{};session.mode=other.mode=role;
 for(int i=0;i<coop::net::kMaxPeers;++i)guids[i]="peer-"+std::to_string(i);
 S::Install(&session);
}
std::vector<uint8_t> Line(const std::wstring& text,uint8_t type=0,uint8_t op=1){std::vector<uint8_t>b;W::AppU8(b,op);W::AppU8(b,type);W::AppWStr(b,text);return b;}
std::vector<std::vector<uint8_t>> Delivered(const coop::net::Session& wire,int slot=0){
 B::Assembler assembler;std::vector<std::vector<uint8_t>> out;
 for(const auto& p:wire.sent)if(p.slot==slot){CHECK(p.data.size()==sizeof(coop::net::BlobChunkPayload));coop::net::BlobChunkPayload chunk{};std::memcpy(&chunk,p.data.data(),sizeof(chunk));std::vector<uint8_t>b;if(assembler.OnChunk(chunk,uint8_t(slot),b))out.push_back(std::move(b));}
 return out;
}
void Incoming(const std::vector<uint8_t>& blob){
 coop::net::Session wire;CHECK(B::SendBlobToSlot(&wire,0,coop::net::ReliableKind::SatConsole,++sequence,blob));
 for(const auto& packet:wire.sent){coop::net::BlobChunkPayload chunk{};CHECK(packet.data.size()==sizeof(chunk));std::memcpy(&chunk,packet.data.data(),sizeof(chunk));S::OnChunk(chunk,0);}
}
void HostPeer(){session.connectedPeers[1]=session.readyPeers[1]=true;}
void DrainAll(){for(int i=0;i<30;++i){const auto before=printAttempts.size();S::DrainDisplay();if(before==printAttempts.size())return;}CHECK(false);}

void ConsumeFailure(){
 Reset();onConsume=[](void*){return false;};Frame f;CHECK(Enter(f,local,L"sv.ping")==V::Cancel);
 CHECK(session.sent.empty());CHECK(S::LaneCounts().linesSent==0);CHECK(consumes==1);CHECK(nativeRuns==0);
 CHECK(local->command==L"sv.ping");
 onConsume={};Frame retry;CHECK(Enter(retry,local,L"sv.ping")==V::Cancel);CHECK(consumes==2);CHECK(Delivered(session).size()==1);CHECK(S::LaneCounts().linesSent==1);
}
void ClientRetirement(){
 const char* stages[]={"ReadEnterCommandLine","ReadProcessing","ReadName","ReadActiveDish","ReadUsed","PanelName","ConsumeInput"};
 for(const char* stage:stages)for(int mode=0;mode<9;++mode){
  Reset();Obj* original=local;bool changed=false;
  onNative=[&](const char* name,void*){if(changed||std::string(name)!=stage)return;changed=true;switch(mode){
   case 0:++world;break;case 1:++serial;break;case 2:session.mode=Role::Host;break;
   case 3:S::OnDisconnect();S::Install(&other);break;case 4:local=Make();break;
   case 5:++original->serial;break;case 6:original->live=false;break;case 7:++session.generations[0];break;
   case 8:S::OnPeerGone(0);CHECK(S::BindPeer(&session,0));break;
  }};
  Frame f;Enter(f,original,L"sv.ping");CHECK(changed);CHECK(session.sent.empty()&&other.sent.empty());CHECK(S::LaneCounts().linesSent==0);
 }
}
void ConsumeReentry(){
 Reset();Frame nested;bool reentered=false;onConsume=[&](void* p){if(!reentered){reentered=true;CHECK(Enter(nested,p,L"sv.ping")==V::Cancel);}return true;};
 Frame outer;CHECK(Enter(outer,local,L"sv.ping")==V::Cancel);CHECK(reentered);CHECK(consumes==1);CHECK(Delivered(session).size()==1);CHECK(S::LaneCounts().linesSent==1);
}
void ConsumeBeforeSend(){
 Reset();bool observed=false;session.onSend=[&](const coop::net::Session::Sent&){observed=true;CHECK(consumes==1);CHECK(local->command.empty());return true;};
 Frame f;CHECK(Enter(f,local,L"sv.ping")==V::Cancel);CHECK(observed);CHECK(Delivered(session).size()==1);
}
void StalePost(){
 for(bool sessionChange:{false,true}){
  Reset();Frame old;CHECK(Enter(old,local,L"help")==V::Run);
  if(sessionChange){S::OnDisconnect();S::Install(&session);}else ++world;
  if(sessionChange){CHECK(Write(local,L"old active body output")==V::Cancel);CHECK(session.sent.empty());Frame fresh;CHECK(Enter(fresh,local,L"help")==V::Run);Post(old);CHECK(Write(local,L"new output")==V::Cancel);CHECK(Delivered(session)==std::vector<std::vector<uint8_t>>({Line(L"new output",0,3)}));Post(fresh);}
  else {const auto reads=local->reads;Post(old);CHECK(local->reads==reads);}
 }
}
void NestedClear(){
 for(bool outerClear:{false,true}){
  Reset(Role::Host);HostPeer();Frame outer,inner;
  CHECK(Enter(outer,local,outerClear?L"clr":L"help")==V::Run);
  CHECK(Enter(inner,local,outerClear?L"help":L"clr")==V::Run);CHECK(Write(local,L"",0)==V::Cancel);Post(inner);
  CHECK(Write(local,L"",0)==V::Cancel);Post(outer);
  const auto expected=std::vector<std::vector<uint8_t>>({Line(L"",outerClear?0:3),Line(L"",outerClear?3:0)});CHECK(Delivered(session,1)==expected);
 }
}
void UnwoundFrame(){
 Reset();Frame f;CHECK(Enter(f,local,L"help")==V::Run);active.erase(f.call.invocation); // firewall unwind skips POST
 CHECK(Write(local,L"periodic mirror alert")==V::Cancel);CHECK(session.sent.empty());
}
void LocalGraph(){
 Reset();Frame enter;CHECK(Enter(enter,local,L"help")==V::Run);local->busy=true;Post(enter);
 Frame graph;CHECK(Enter(graph,local,L"",L"ExecuteUbergraph_ui_console")==V::Run);
 CHECK(Write(local,L"latent local output")==V::Cancel);local->busy=false;Post(graph);
 CHECK(Delivered(session)==std::vector<std::vector<uint8_t>>({Line(L"latent local output",0,3)}));
 CHECK(Write(local,L"later periodic mirror alert")==V::Cancel);CHECK(Delivered(session).size()==1);
}
void WriteRetirement(){
 for(int mode=0;mode<7;++mode){
  Reset();Obj* original=local;Frame frame;CHECK(Enter(frame,original,L"help")==V::Run);bool changed=false;
  onNative=[&](const char* name,void*){if(changed||std::string(name)!="ReadWriteToLogArgs")return;changed=true;switch(mode){
   case 0:local=Make();break;case 1:++original->serial;break;case 2:original->live=false;break;
   case 3:++world;break;case 4:++serial;break;case 5:S::OnDisconnect();S::Install(&other);break;
   case 6:++session.generations[0];break;
  }};
  Write(original,L"retired output");CHECK(changed);CHECK(session.sent.empty()&&other.sent.empty());CHECK(S::LaneCounts().linesReturned==0);
 }
}
void DisplayFailureClear(){
 Reset();S::ClientOnBlob(Line(L"old"));bool nested=false;
 onPrint=[&](void*,const std::wstring& text,uint8_t){if(text==L"old"&&!nested){nested=true;S::ClientOnBlob(Line(L"",3));S::ClientOnBlob(Line(L"new"));return false;}return true;};
 S::DrainDisplay();CHECK(nested);onPrint={};DrainAll();CHECK(local->log==L"new");CHECK(std::count(printAttempts.begin(),printAttempts.end(),L"old")==1);
}
void DisplayFailureAppend(){
 Reset();S::ClientOnBlob(Line(L"first"));bool nested=false;
 onPrint=[&](void*,const std::wstring&,uint8_t){if(!nested){nested=true;S::ClientOnBlob(Line(L"second"));return false;}return true;};
 S::DrainDisplay();onPrint={};DrainAll();CHECK(local->log==L"firstsecond");CHECK(std::count(printAttempts.begin(),printAttempts.end(),L"first")==2);
}
void DisplayRetirement(){
 for(int mode=0;mode<7;++mode){
  Reset();Obj* first=local;S::ClientOnBlob(Line(L"first"));S::ClientOnBlob(Line(L"second"));bool changed=false;
  onPrint=[&](void*,const std::wstring&,uint8_t){if(!changed){changed=true;switch(mode){
   case 0:++world;break;case 1:++serial;break;case 2:session.mode=Role::Host;break;
   case 3:local=Make();break;case 4:++first->serial;break;case 5:first->live=false;local=Make();break;
   case 6:S::OnDisconnect();S::Install(&other);S::ClientOnBlob(Line(L"replacement"));break;
  }}return true;};
  S::DrainDisplay();CHECK(changed);CHECK(printAttempts.size()==1);if(mode==6){onPrint={};DrainAll();CHECK(local->log==L"firstreplacement");}
 }
}
void HostDisplayReset(){
 for(int mode=0;mode<3;++mode)for(bool accepted:{false,true}){
  Reset();CHECK(S::BindPeer(&session,0));Incoming({2,1});CHECK(local->busy);
  Incoming(Line(L"old first"));Incoming(Line(L"old tail"));bool changed=false;
  onPrint=[&](void*,const std::wstring&,uint8_t){if(!changed){changed=true;if(mode==0)++session.generations[0];else{S::OnPeerGone(0);CHECK(S::BindPeer(&session,0));}}return accepted;};
  if(mode==2){++session.generations[0];changed=true;}
  S::DrainDisplay();CHECK(changed);CHECK(printAttempts.size()==(mode==2?0:1));
  onPrint={};DrainAll();CHECK(std::count(printAttempts.begin(),printAttempts.end(),L"old first")==size_t(mode==2?0:1));
  CHECK(std::count(printAttempts.begin(),printAttempts.end(),L"old tail")==0);
  S::Tick();CHECK(!local->busy); // the retired host's true level cannot stick
  Incoming({2,1});CHECK(local->busy);Incoming({2,0});CHECK(!local->busy);
  Incoming(Line(L"",3));Incoming(Line(L"fresh host"));DrainAll();CHECK(local->log==L"fresh host");
  CHECK(std::count(printAttempts.begin(),printAttempts.end(),L"old tail")==0);
 }
}
void ReplayAttribution(){
 for(int target=0;target<2;++target){
  Reset(Role::Host);HostPeer();void* native=target?S::TerminalFor(1,guids[1])->pin.Raw():local;
  bool nested=false;onPrint=[&](void*,const std::wstring& text,uint8_t type){if(!nested){nested=true;CHECK(Write(native,text,type,true)==V::Cancel);}return true;};
  S::QueueDisplay(L"same text",0);S::DrainDisplay();CHECK(nested);CHECK(Delivered(session,1)==std::vector<std::vector<uint8_t>>({Line(L"same text")}));
 }
}
void ReplayNewSession(){
 Reset(Role::Host);HostPeer();other.connectedPeers[1]=other.readyPeers[1]=true;bool nested=false;
 onPrint=[&](void*,const std::wstring&,uint8_t){if(!nested){nested=true;S::OnDisconnect();S::Install(&other);CHECK(Write(local,L"new session alert")==V::Cancel);}return true;};
 S::QueueDisplay(L"old replay",0);S::DrainDisplay();CHECK(nested);CHECK(Delivered(other,1)==std::vector<std::vector<uint8_t>>({Line(L"new session alert")}));
 onPrint={};DrainAll();CHECK(local->log==L"old replaynew session alert");
}
void ReplayPreclaim(){
 for(bool duringProbe:{false,true}){
  Reset(Role::Host);HostPeer();bool nested=false;
  const auto alert=[&]{if(!nested){nested=true;CHECK(Write(local,L"setup alert",0,true)==V::Cancel);}};
  if(duringProbe)onNative=[&](const char* name,void*){if(std::string(name)=="ReadWriteToLogArgs")alert();};
  else beforePrint=[&](void*,const std::wstring&,uint8_t){alert();};
  S::QueueDisplay(L"original replay",0);S::DrainDisplay();CHECK(nested);
  CHECK(Delivered(session,1)==std::vector<std::vector<uint8_t>>({Line(L"setup alert")}));
  CHECK(local->log==L"original replaysetup alert");
 }
}
void WatchRollback(){
 Reset();watches.clear();S::g_watched=false;S::g_watchRetry={};failWriteWatch=true;S::EnsureWatched();
 CHECK(!S::g_watched);CHECK(watches.empty());
 failWriteWatch=false;S::g_watchRetry={};S::EnsureWatched();CHECK(S::g_watched&&watches.size()==3);
 CHECK(watches.at(L"ExecuteUbergraph_ui_console").pre==S::OnBodyPre);
}
}
int main(int argc,char** argv){using namespace test;struct Case{const char* name;void(*run)();};const Case cases[]={
 {"consume-failure",ConsumeFailure},{"client-retirement",ClientRetirement},{"consume-reentry",ConsumeReentry},{"consume-before-send",ConsumeBeforeSend},
 {"stale-post",StalePost},{"nested-clear",NestedClear},{"unwound-frame",UnwoundFrame},{"local-graph",LocalGraph},{"write-retirement",WriteRetirement},{"display-failure-clear",DisplayFailureClear},{"display-failure-append",DisplayFailureAppend},
 {"display-retirement",DisplayRetirement},{"host-display-reset",HostDisplayReset},{"replay-attribution",ReplayAttribution},{"replay-new-session",ReplayNewSession},{"replay-preclaim",ReplayPreclaim},{"watch-rollback",WatchRollback}};
 bool ran=false;for(const auto& item:cases)if(argc<2||std::string(argv[1])==item.name){scenario=item.name;item.run();std::printf("PASS %s\n",scenario);ran=true;}CHECK(ran);
 onNative={};onConsume={};onPrint={};beforePrint={};session.onSend={};other.onSend={};S::OnDisconnect();for(const auto& p:objects)CHECK(p.pins==0);
 std::puts("PASS satellite console native callbacks: real controller, codec and transport");return 0;
}
