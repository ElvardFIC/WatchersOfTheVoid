// Offline controller regression. Only engine objects/native calls and Session's
// transport edge are doubles; every byte crosses the real chunker and assembler.
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"
#include "coop/net/session.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace test {
const char* scenario = "setup";
struct Obj { int id, serial=1; bool live=true, busy=false; int pins=0, discards=0; std::wstring log; };
std::deque<Obj> objects;
std::vector<Obj*> slots{nullptr};
Obj* local=nullptr;
uint32_t world=1, sessionSerial=1;
std::array<std::string, coop::net::kMaxPeers> guids;
coop::net::Session session, other;
std::function<void(void*)> onDiscard;
std::function<void(void*)> onReadBusy;
std::function<void(void*)> onCreate, onInit, onWriteUsed;
std::wstring command=L"sv.ping";
int runs=0;
Obj* Make(bool busy=false) {
    objects.push_back({static_cast<int>(slots.size())});
    auto* obj=&objects.back(); obj->busy=busy; slots.push_back(obj); return obj;
}
void Fail(int line, const char* expression) {
    std::fprintf(stderr,"FAIL %s line %d: %s\n",scenario,line,expression); std::exit(1);
}
}
#define CHECK(expression) do { if(!(expression)) test::Fail(__LINE__,#expression); } while(0)

namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p) { return p?static_cast<test::Obj*>(p)->id:-1; }
int32_t SlotSerial(int32_t i) { return i>0 && size_t(i)<test::slots.size()?test::slots[i]->serial:0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* p,int32_t i) { return i>0 && size_t(i)<test::slots.size() && test::slots[i]==p && test::slots[i]->live; }
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
}
namespace ue_wrap::world_identity {
uint32_t Generation() { return test::world; }
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void*) { return CurrentWorld(); }
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
#include "facts_test_support.h"
#ifndef SAT_CONSOLE_SOURCE
#define SAT_CONSOLE_SOURCE "../../src/votv-coop/src/coop/interactables/sat_console_sync.cpp"
#endif
#include SAT_CONSOLE_SOURCE
#include "../../src/votv-coop/src/coop/net/blob_chunks.cpp"
#include "../../src/votv-coop/src/coop/items/save_record_wire.cpp"
#include "../../src/votv-coop/src/coop/interactables/signal_wire.cpp"
#include "../../src/votv-coop/src/coop/interactables/sat_console_table.cpp"

namespace ue_wrap {
bool GcPin::Pin(void* p) { Release();obj_=p;if(p)++static_cast<test::Obj*>(p)->pins;return p!=nullptr; }
void GcPin::Release() { if(obj_)--static_cast<test::Obj*>(obj_)->pins;obj_=nullptr; }
namespace game_thread { bool IsGameThread() { return true; } }
namespace script_gate {
bool IsInvocationActive(uint64_t invocation) { return invocation!=0; }
bool WatchName(const facts::Function&,int,PreFn,PostFn) { return true; }
bool UnwatchName(const facts::Function&,int,PreFn,PostFn) { return true; }
}
namespace console_desk { void* Instance() { return test::local; } }
namespace dish { int IndexOf(void*){return -1;} void* DishByIndex(int){return nullptr;} int Count(){return 0;} }
namespace sat_console {
struct Args {std::wstring text;uint8_t type;};
bool EnsureResolved(){return true;}
void* LocalTerminal(){return test::local;}
void* CreateTerminal(void*){auto* p=test::Make();const auto callback=test::onCreate;if(callback)callback(p);return p;}
void DiscardTerminal(void* p){if(!p)return;auto* obj=static_cast<test::Obj*>(p);++obj->discards;obj->live=false;const auto f=test::onDiscard;if(f)f(p);}
bool ReadProcessing(void* p,bool& busy){busy=static_cast<test::Obj*>(p)->busy;const auto f=test::onReadBusy;if(f)f(p);return true;}
bool WriteProcessing(void* p,bool busy){static_cast<test::Obj*>(p)->busy=busy;return true;}
bool WritePanel(void*,void*){return true;}
bool ReadEnterCommandLine(const uint8_t*,std::wstring& s){s=test::command;return true;}
bool ReadWriteToLogArgs(const uint8_t* p,std::wstring& s,uint8_t& type){const auto* args=reinterpret_cast<const Args*>(p);s=args->text;type=args->type;return true;}
bool ReadName(void*,std::wstring& s){s=L"root";return true;}
void* ReadUsed(void*){return nullptr;}
void* ReadActiveDish(void*){return nullptr;}
bool WriteUsed(void* p,void*){const auto callback=test::onWriteUsed;if(callback)callback(p);return true;}
std::wstring PanelName(void*){return {};}
void* FindPanelByName(const std::wstring&){return nullptr;}
bool CallInit(void* p,const std::wstring&,bool,void*){const auto callback=test::onInit;if(callback)callback(p);return true;}
bool ConsumeInput(void*){return true;}
bool ConsumeInputWhile(void* term,const std::function<bool()>& current){return current() && ConsumeInput(term) && current();}
bool CallWriteToLog(void* p,const std::wstring& text,uint8_t type){
 Args args{text,type};script_gate::Call c{};c.object=p;c.locals=reinterpret_cast<uint8_t*>(&args);c.fromOurCode=true;
 if(coop::sat_console_sync::OnWritePre(c)!=script_gate::Verdict::Cancel)static_cast<test::Obj*>(p)->log+=text;
 return true;
}
bool RunCommand(void* p,const std::wstring& text){++test::runs;CallWriteToLog(p,text,1);CallWriteToLog(p,L"answer",0);return true;}
bool ClearLog(void* p){static_cast<test::Obj*>(p)->log.clear();return true;}
}
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace coop::player_handshake {const std::string& GuidForSlot(int slot){return test::guids.at(slot);}}

namespace test {
namespace S=coop::sat_console_sync;
namespace W=coop::save_record_wire;
namespace B=coop::blob_chunks;
using coop::net::Role;
using Packet=coop::net::Session::Sent;
void Reset(Role role=Role::Host) {
    onDiscard={};onReadBusy={};onCreate={};onInit={};onWriteUsed={};session.onSend={};other.onSend={};S::OnDisconnect();
    for(const auto& object:objects)CHECK(object.pins==0);
    objects.clear();slots.resize(1);local=Make();world=1;++sessionSerial;runs=0;
    session=coop::net::Session{};other=coop::net::Session{};session.mode=role;
    for(int slot=0;slot<coop::net::kMaxPeers;++slot)guids[slot]="peer-"+std::to_string(slot);
    S::Install(&session);
}
void Peer(int slot,bool ready=true){session.connectedPeers[slot]=true;session.readyPeers[slot]=ready;}
std::vector<uint8_t> Line(std::wstring text,uint8_t type=0,uint8_t op=1){std::vector<uint8_t> b;W::AppU8(b,op);W::AppU8(b,type);W::AppWStr(b,text);return b;}
std::vector<uint8_t> Command(){std::vector<uint8_t> b;W::AppU8(b,0);W::AppI32(b,-1);W::AppWStr(b,L"root");W::AppWStr(b,L"");W::AppWStr(b,L"sv.ping");return b;}
coop::net::BlobChunkPayload Chunk(const Packet& p){CHECK(p.kind==coop::net::ReliableKind::SatConsole);CHECK(p.data.size()==sizeof(coop::net::BlobChunkPayload));coop::net::BlobChunkPayload c{};std::memcpy(&c,p.data.data(),sizeof(c));return c;}
std::vector<Packet> Packets(const std::vector<uint8_t>& b,uint32_t seq=100){coop::net::Session wire;CHECK(B::SendBlobToSlot(&wire,1,coop::net::ReliableKind::SatConsole,seq,b));return wire.sent;}
std::vector<std::vector<uint8_t>> Delivered(const coop::net::Session& wire,int slot){
 B::Assembler a;std::vector<std::vector<uint8_t>> delivered;
 for(const auto& p:wire.sent)if(p.slot==slot){std::vector<uint8_t> b;if(a.OnChunk(Chunk(p),uint8_t(slot),b))delivered.push_back(std::move(b));}
 return delivered;
}
void Incoming(int slot,const std::vector<uint8_t>& b,uint32_t seq=100){for(const auto& p:Packets(b,seq))S::OnChunk(Chunk(p),uint8_t(slot));}
void Flush(int slot){S::g_out[slot].nextTry={};S::Flush(uint8_t(slot));}
std::wstring Render(const std::vector<std::vector<uint8_t>>& blobs){
 std::wstring out;for(const auto& b:blobs){size_t offset=0;uint8_t op=0,type=0;std::wstring line;
  CHECK(W::RdU8(b,offset,op));if(op!=1)continue;
  CHECK(W::RdU8(b,offset,type)&&W::RdWStr(b,offset,line)&&offset==b.size());
  if(type==3)out.clear();else out+=line;
 }return out;
}

void Capacity(){
 Reset();for(int slot=1;slot<32;++slot){Peer(slot);Incoming(slot,Command(),100+slot);}
 CHECK(S::g_terms.size()==31);CHECK(runs==31);CHECK(S::ByGuid(guids[31])!=nullptr);
 CHECK(S::ByGuid(guids[31])->slot==31);CHECK(S::g_terms.back().pin.Held());
}

void BroadcastClear(){
 Reset();Peer(1);Peer(2);Peer(31);const std::wstring before(600,L'a');bool nested=false;
 session.onSend=[&](const Packet& p){if(p.slot==1&&!nested){nested=true;S::ShareLog(L"",3,-1,true);S::ShareLog(L"after clear",0,-1,true);}return true;};
 S::ShareLog(before,0,-1,true);CHECK(nested);CHECK(S::g_history.size()==1&&S::g_history.front().text==L"after clear");
 const auto canonical=Delivered(session,1);CHECK(Render(canonical)==L"after clear");
 for(int slot:{2,31}){const auto lines=Delivered(session,slot);CHECK(Render(lines)==L"after clear");CHECK(lines==canonical);}
 S::DrainDisplay();CHECK(local->log==L"after clear");
}

void NativeCommandSource(){
 // A terminal constructor can end the old session. Its failure belongs to the
 // retired request; it must not publish an old echo/error in the replacement.
 Reset();Peer(1);other.connectedPeers[31]=other.readyPeers[31]=true;
 Obj* created=nullptr;onCreate=[&](void* p){created=static_cast<Obj*>(p);onCreate={};S::OnDisconnect();++sessionSerial;S::Install(&other);};
 Incoming(1,Command());CHECK(created);CHECK(S::g_session.load()==&other);CHECK(runs==0);CHECK(other.sent.empty());
 CHECK(S::g_history.empty()&&S::g_display.empty());CHECK(created->pins==0&&created->discards==1);
 // Busy-read, context init and panel-write callbacks must retain both source
 // and recipient identity, including explicit leave/rebind with unchanged IDs.
 for(int stage=0;stage<3;++stage)for(int mode=0;mode<9;++mode){
  Reset();Peer(31);bool changed=false;
  const auto transition=[&](void*){if(changed)return;changed=true;switch(mode){
   case 0:++world;break;case 1:++sessionSerial;break;case 2:session.mode=Role::Client;break;
   case 3:++session.generations[31];break;case 4:guids[31]="different typist";break;
   case 5:session.connectedPeers[31]=false;break;case 6:session.readyPeers[31]=false;break;
   case 7:S::OnDisconnect();S::Install(&other);break;
   case 8:S::OnPeerGone(31);CHECK(S::BindPeer(&session,31));break;
  }};
  if(stage==0)onReadBusy=transition;else if(stage==1)onInit=transition;else onWriteUsed=transition;
  Incoming(31,Command());CHECK(changed);CHECK(runs==0);CHECK(session.sent.empty()&&other.sent.empty());
  CHECK(S::g_history.empty()&&S::g_display.empty());
  if(stage==1)for(const auto& term:S::g_terms)CHECK(!term.haveContext);
 }
}

void RecipientReset(){
 for(bool identityChange:{false,true}){
  Reset();Peer(31);session.sendOk=false;S::SendLog(31,L"old recipient",0);CHECK(S::g_out[31].q.size()==1);
  auto split=Packets(Line(std::wstring(400,L'x'),0,3));CHECK(split.size()>1);
  S::OnChunk(Chunk(split.front()),31);CHECK(!S::g_asm.Idle());
  if(identityChange)guids[31]="new-person";else ++session.generations[31];
  CHECK(S::BindPeer(&session,31));CHECK(S::g_out[31].q.empty());CHECK(S::g_asm.Idle());
  for(size_t i=1;i<split.size();++i)S::OnChunk(Chunk(split[i]),31);
  CHECK(!S::g_haveHistory);CHECK(S::g_display.empty());
  session.sendOk=true;Flush(31);CHECK(session.sent.empty());
 }
}

void QueueAppend(){
 Reset();Peer(1);const auto longLine=Line(std::wstring(600,L'a'));const auto tail=Line(L"tail");
 bool nested=false;session.onSend=[&](const Packet&){if(!nested){nested=true;S::Enqueue(1,tail);S::Flush(1);}return true;};
 S::Enqueue(1,longLine);CHECK(nested);CHECK(S::g_out[1].q.empty());
 const auto lines=Delivered(session,1);CHECK(lines.size()==2);CHECK(lines[0]==longLine);CHECK(lines[1]==tail);
 // Every long-line chunk shares one sequence; the recursive Flush sent no duplicate head.
 const auto parts=Packets(longLine).size();CHECK(session.sent.size()==parts+1);
 for(size_t i=0;i<parts;++i)CHECK(Chunk(session.sent[i]).blobSeq==Chunk(session.sent.front()).blobSeq);
 CHECK(Chunk(session.sent.back()).blobSeq!=Chunk(session.sent.front()).blobSeq);
}

void QueueReplacement(){
 for(bool disconnect:{false,true}){
  Reset();Peer(1);const auto old=Line(std::wstring(600,L'o'));const auto replacement=Line(L"replacement");
  bool callback=false;session.onSend=[&](const Packet&){if(!callback){callback=true;
   if(disconnect){S::OnDisconnect();S::Install(&session);}else S::OnPeerGone(1);
   S::Enqueue(1,replacement);
  }return true;};
  S::Enqueue(1,old);CHECK(session.sent.size()==Packets(old).size());CHECK(S::g_out[1].q.size()==1);CHECK(S::g_out[1].q.front()==replacement);
  Flush(1);const auto lines=Delivered(session,1);CHECK(lines==std::vector<std::vector<uint8_t>>({old,replacement}));CHECK(S::g_out[1].q.empty());
 }
 // Eviction cannot recall an admitted batch; its completion must leave the new head owned.
 Reset();Peer(1);session.sendOk=false;S::Enqueue(1,Line(std::wstring(600,L'e')));
 for(size_t i=1;i<S::kOutboxCap;++i)S::Enqueue(1,{2,0});
 bool callback=false;session.onSend=[&](const Packet&){if(!callback){callback=true;S::Enqueue(1,Line(L"newest"));}return true;};session.sendOk=true;
 Flush(1);CHECK(session.sent.size()==Packets(Line(std::wstring(600,L'e'))).size());CHECK(S::g_out[1].q.size()==S::kOutboxCap);CHECK(S::g_out[1].q.front()==std::vector<uint8_t>({2,0}));
 Flush(1);const auto delivered=Delivered(session,1);CHECK(delivered.size()==S::kOutboxCap+1);CHECK(delivered.back()==Line(L"newest"));
}

void SourceChanges(){
 for(int mode=0;mode<7;++mode){
  Reset();Peer(1);bool callback=false;session.onSend=[&](const Packet&){if(!callback){callback=true;switch(mode){
   case 0:++world;break;case 1:++sessionSerial;break;case 2:session.mode=Role::Client;break;
   case 3:session.linked=false;break;case 4:session.active=false;break;
   case 5:other.connectedPeers[1]=other.readyPeers[1]=true;S::Install(&other);break;
   case 6:++session.generations[1];break;
  }}return true;};
  S::Enqueue(1,Line(std::wstring(600,L'c')));CHECK(callback);CHECK(session.sent.size()==Packets(Line(std::wstring(600,L'c'))).size());
  CHECK(session.attempts.size()==session.sent.size());CHECK(other.sent.empty());
 }
 // Recipient identity also retires completion after batch admission.
 Reset();Peer(1);session.onSend=[&](const Packet&){guids[1]="replacement-guid";return true;};
 S::Enqueue(1,Line(std::wstring(600,L'g')));CHECK(session.sent.size()==Packets(Line(std::wstring(600,L'g'))).size());
}

void RefusedHistory(){
 Reset();Peer(1,false);S::ShareLog(L"one",0,-1,false);S::ShareLog(L"two",1,-1,false);Peer(1);
 session.sendOk=false;S::OnPeerWorldReady(1);CHECK(S::g_out[1].q.size()==4);CHECK(session.sent.empty());
 CHECK(runs==0);session.sendOk=true;Flush(1);
 const auto lines=Delivered(session,1);CHECK(lines==std::vector<std::vector<uint8_t>>({Line(L"",3),Line(L"one"),Line(L"two",1),{2,0}}));
 CHECK(S::g_out[1].q.empty());CHECK(runs==0);
}

void MutatingHistory(){
 for(int variant=0;variant<4;++variant){const bool clear=(variant&1)!=0,reseed=(variant&2)!=0;
  Reset();Peer(1,false);const std::wstring ownedLine(600,L'h');S::ShareLog(ownedLine,0,-1,false);S::ShareLog(L"old two",0,-1,false);Peer(1);
  bool changed=false;session.onSend=[&](const Packet& p){const auto chunk=Chunk(p);
   if(!changed && chunk.chunkIdx==0 && chunk.chunkLen>1 && chunk.data[0]==1 && chunk.data[1]!=3){changed=true;
    if(clear)S::ShareLog(L"",3,-1,false);
    S::ShareLog(L"new line",0,-1,false);
    // Reentrant join notification must leave the outer snapshot's retry owed.
    if(reseed)S::OnPeerWorldReady(1);
   }return true;};
  S::OnPeerWorldReady(1);CHECK(changed);CHECK(S::g_seedOwed[1]);
  session.onSend={};S::Tick();CHECK(!S::g_seedOwed[1]);CHECK(S::g_out[1].q.empty());
  const auto all=Delivered(session,1);size_t lastClear=all.size();
  for(size_t i=0;i<all.size();++i)if(all[i]==Line(L"",3))lastClear=i;
  CHECK(lastClear<all.size());
  const std::vector<std::vector<uint8_t>> tail(all.begin()+lastClear,all.end());
  if(clear)CHECK(tail==std::vector<std::vector<uint8_t>>({Line(L"",3),Line(L"new line"),{2,0}}));
  else CHECK(tail==std::vector<std::vector<uint8_t>>({Line(L"",3),Line(ownedLine),Line(L"old two"),Line(L"new line"),{2,0}}));
  CHECK(runs==0);
 }
}

void HistoryBusyRetry(){
 for(bool initialBusy:{false,true}){
  Reset();Peer(31);auto* t=S::TerminalFor(31,guids[31]);CHECK(t);auto* native=static_cast<Obj*>(t->pin.Raw());native->busy=initialBusy;
  session.sendOk=false;S::SendBusy(31,initialBusy);CHECK(S::g_out[31].q.size()==1);
  Peer(31,false);S::ShareLog(L"old history",0,-1,false);Peer(31);session.sendOk=true;
  bool changed=false;session.onSend=[&](const Packet&){if(!changed){changed=true;native->busy=!initialBusy;S::ShareLog(L"new history",0,-1,false);session.sendOk=false;}return true;};
  S::OnPeerWorldReady(31);CHECK(changed);CHECK(S::g_seedOwed[31]);
  session.onSend={};session.sendOk=true;S::Tick();CHECK(!S::g_seedOwed[31]);CHECK(S::g_out[31].q.empty());
  const auto all=Delivered(session,31);size_t lastClear=all.size();
  for(size_t i=0;i<all.size();++i)if(all[i]==Line(L"",3))lastClear=i;
  CHECK(lastClear<all.size());const std::vector<std::vector<uint8_t>> tail(all.begin()+lastClear,all.end());
  CHECK(tail==std::vector<std::vector<uint8_t>>({Line(L"",3),Line(L"old history"),Line(L"new history"),{2,uint8_t(!initialBusy)}}));
  CHECK(S::ByGuid(guids[31])->busySent==!initialBusy);CHECK(S::ByGuid(guids[31])->slot==31);
 }
}

void BusyWidget(){
 Reset(Role::Client);Obj* first=local;local=nullptr;Incoming(0,{2,0});CHECK(S::g_haveMirrorBusy&&!S::g_mirrorBusy);
 first->busy=true;local=first;S::Tick();CHECK(!first->busy);
 Incoming(0,{2,1});CHECK(first->busy);local=nullptr;Incoming(0,{2,0});CHECK(first->busy);
 Obj* replacement=Make(true);local=replacement;S::Tick();CHECK(!replacement->busy);
 // A local hold protects its own native widget, then expires with that widget's identity.
 replacement->busy=true;S::ClientAfterLocalBody(replacement);CHECK(S::g_localHolds);
 Incoming(0,{2,0});CHECK(replacement->busy);
 ++replacement->serial;S::Tick();CHECK(!replacement->busy);CHECK(!S::g_localHolds);
}

void NestedTeardown(){
 Reset();Peer(1);Peer(2);auto* a=S::TerminalFor(1,guids[1]);CHECK(a);auto* first=static_cast<Obj*>(a->pin.Raw());
 auto* b=S::TerminalFor(2,guids[2]);CHECK(b);auto* second=static_cast<Obj*>(b->pin.Raw());
 other.connectedPeers[31]=other.readyPeers[31]=true;other.sendOk=false;
 Obj* created=nullptr;bool nested=false;
 onDiscard=[&](void*){if(!nested){nested=true;CHECK(S::g_terms.empty());CHECK(S::g_session.load()==nullptr);
  S::Install(&other);auto* t=S::TerminalFor(31,guids[31]);CHECK(t);created=static_cast<Obj*>(t->pin.Raw());
  S::SendLog(31,L"new session reply",0);
 }};
 S::OnDisconnect();CHECK(nested&&created);CHECK(first->discards==1&&second->discards==1);CHECK(first->pins==0&&second->pins==0);
 CHECK(S::g_session.load()==&other);CHECK(S::g_terms.size()==1);CHECK(S::g_terms.front().pin.Raw()==created);CHECK(created->pins==1&&created->discards==0);
 CHECK(S::g_out[31].q.size()==1&&S::g_out[31].q.front()==Line(L"new session reply"));
 onDiscard={};other.sendOk=true;Flush(31);CHECK(Delivered(other,31)==std::vector<std::vector<uint8_t>>({Line(L"new session reply")}));
}

void NestedInstall(){
 Reset();Peer(1);auto* old=S::TerminalFor(1,guids[1]);CHECK(old);auto* retired=static_cast<Obj*>(old->pin.Raw());
 Obj* replacement=nullptr;bool nested=false;
 onDiscard=[&](void* p){if(p==retired&&!nested){nested=true;++sessionSerial;S::Install(&session);
  auto* made=S::TerminalFor(1,guids[1]);CHECK(made);replacement=static_cast<Obj*>(made->pin.Raw());
  session.sendOk=false;S::SendLog(1,L"nested install",0);
 }};
 S::Install(&other);CHECK(nested&&replacement);CHECK(S::g_session.load()==&session);
 CHECK(S::g_serial==sessionSerial&&S::g_world==world);CHECK(S::ByObject(replacement)!=nullptr);CHECK(replacement->pins==1);
 CHECK(S::g_out[1].q.size()==1&&S::g_out[1].q.front()==Line(L"nested install"));
 onDiscard={};session.sendOk=true;Flush(1);CHECK(Delivered(session,1)==std::vector<std::vector<uint8_t>>({Line(L"nested install")}));
}

void DetachedEviction(){
 Reset();for(int slot=1;slot<32;++slot){Peer(slot);CHECK(S::TerminalFor(uint8_t(slot),guids[slot]));}
 auto* retired=static_cast<Obj*>(S::ByGuid(guids[1])->pin.Raw());S::OnPeerGone(1);guids[1]="new person";
 bool nested=false;onDiscard=[&](void* p){if(p==retired){nested=true;CHECK(S::ByObject(retired)==nullptr);CHECK(S::g_terms.size()==30);S::OnDisconnect();S::Install(&other);}};
 other.connectedPeers[1]=other.readyPeers[1]=true;
 S::TerminalFor(1,guids[1]);CHECK(nested);CHECK(retired->discards==1);CHECK(retired->pins==0);
 // A source/session transition inside eviction must not publish an old-source terminal.
 CHECK(S::g_session.load()==&other);CHECK(S::g_terms.empty());
}
}

int main(int argc,char** argv){
 using namespace test;
 struct Case {const char* name;void(*run)();};
 const Case cases[]={{"capacity",Capacity},{"broadcast-clear",BroadcastClear},{"native-command-source",NativeCommandSource},{"recipient-reset",RecipientReset},{"queue-append",QueueAppend},{"queue-replacement",QueueReplacement},{"source-changes",SourceChanges},{"history-refusal",RefusedHistory},{"history-mutation",MutatingHistory},{"history-busy",HistoryBusyRetry},{"busy-widget",BusyWidget},{"nested-teardown",NestedTeardown},{"nested-install",NestedInstall},{"detached-eviction",DetachedEviction}};
 bool ran=false;for(const auto& item:cases)if(argc<2||std::string(argv[1])==item.name){scenario=item.name;item.run();std::printf("PASS %s\n",scenario);ran=true;}
 CHECK(ran);onDiscard={};onReadBusy={};onCreate={};onInit={};onWriteUsed={};session.onSend={};other.onSend={};S::OnDisconnect();
 for(const auto& object:objects)CHECK(object.pins==0);
 std::puts("PASS satellite console: production controller, wire codecs, chunker and assembler");return 0;
}
