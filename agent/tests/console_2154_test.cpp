#include "support_2135.h"
#include "../../src/votv-coop/src/coop/interactables/sat_console_sync.cpp"
#include "../../src/votv-coop/src/coop/interactables/sat_console_table.cpp"
#include "../../src/votv-coop/src/coop/items/save_record_wire.cpp"
#include "../../src/votv-coop/src/coop/interactables/signal_wire.cpp"
namespace test {
struct Args {std::wstring text; uint8_t type=0;};
std::wstring command=L"wrong.command",log;
bool available=true,busy=false,writeOk=true;
int runs=0,prints=0;
std::vector<std::pair<int,std::vector<uint8_t>>> sent;
}
namespace ue_wrap {
bool GcPin::Pin(void* a){obj_=a;return a!=nullptr;} void GcPin::Release(){obj_=nullptr;}
namespace game_thread {bool IsGameThread(){return true;}}
namespace script_gate {bool UnwatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return true;}}
namespace console_desk {void* Instance(){return &test::actor;}}
namespace dish {int IndexOf(void*){return -1;} void* DishByIndex(int){return nullptr;}}
namespace sat_console {
bool EnsureResolved(){return true;}
void* LocalTerminal(){return test::available?&test::actor:nullptr;}
void* CreateTerminal(void*){return &test::ui;} void DiscardTerminal(void*){}
bool ReadProcessing(void*,bool& b){b=test::busy;return true;}
bool WriteProcessing(void*,bool b){test::busy=b;return true;}
bool WritePanel(void*,void*){return true;}
bool ReadEnterCommandLine(const uint8_t*,std::wstring& s){s=test::command;return true;}
bool ReadWriteToLogArgs(const uint8_t* raw,std::wstring& s,uint8_t& t){auto* a=reinterpret_cast<const test::Args*>(raw);s=a->text;t=a->type;return true;}
bool ReadName(void*,std::wstring& s){s=L"root";return true;}
void* ReadUsed(void*){return nullptr;} void* ReadActiveDish(void*){return nullptr;}
bool WriteUsed(void*,void*){return true;}
std::wstring PanelName(void*){return {};}
void* FindPanelByName(const std::wstring&){return nullptr;}
bool CallInit(void*,const std::wstring&,bool,void*){return true;}
bool ConsumeInput(void*){return true;}
bool CallWriteToLog(void* t,const std::wstring& s,uint8_t type){
 test::Args a{s,type};script_gate::Call c{};c.object=t;c.locals=reinterpret_cast<uint8_t*>(&a);c.fromOurCode=true;
 coop::sat_console_sync::OnWritePre(c);test::Callback();
 if(!test::writeOk)return false;++test::prints;test::log+=s;return true;
}
bool RunCommand(void* t,const std::wstring& s){++test::runs;CallWriteToLog(t,s,1);CallWriteToLog(t,L"answer",0);return true;}
bool ClearLog(void* t){test::log.clear();return CallWriteToLog(t,L"",0);}
}
}
namespace coop {
namespace player_handshake {const std::string& GuidForSlot(int){static std::string s="typist";return s;}}
namespace blob_chunks {
bool SendBlobToSlot(net::Session* s,int slot,net::ReliableKind,uint32_t,const std::vector<uint8_t>& b){if(!s->sendOk)return false;test::sent.emplace_back(slot,b);return true;}
bool Assembler::OnChunk(const net::BlobChunkPayload&,uint8_t,std::vector<uint8_t>&){return false;}
void Assembler::Sweep(std::chrono::steady_clock::time_point,std::chrono::seconds){}
void Assembler::Clear(){map_.clear();} void Assembler::ClearSlot(uint8_t){}
}
}
std::vector<uint8_t> Line(uint8_t op,std::wstring text,uint8_t type=0){std::vector<uint8_t>b;namespace W=coop::save_record_wire;W::AppU8(b,op);W::AppU8(b,type);W::AppWStr(b,text);return b;}
void Write(void* obj,std::wstring text,uint8_t type=0){test::Args a{text,type};ue_wrap::script_gate::Call c{};c.object=obj;c.locals=reinterpret_cast<uint8_t*>(&a);coop::sat_console_sync::OnWritePre(c);}
int main(){using namespace test;namespace S=coop::sat_console_sync;namespace W=coop::save_record_wire;using V=ue_wrap::script_gate::Verdict;
 coop::net::Session session;session.mode=coop::net::Role::Client;S::Install(&session);
 ue_wrap::script_gate::Call c{};c.object=&actor;c.tag=S::kTagEnter;
 CHECK(S::OnEnterPre(c)==V::Run);Write(&actor,command,1);Write(&actor,L"<r>err</>");S::OnBodyPost(c);
 CHECK(sent.size()==2&&sent[0].first==0&&sent[0].second[0]==3&&S::g_localCommandDepth==0);
 Write(&actor,L"periodic alert");CHECK(sent.size()==2); // not a command; no cyclic alert relay
 auto echo=sent[0].second,answer=sent[1].second;
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);sent.clear();
 S::HostOnBlob(1,echo);S::HostOnBlob(1,answer);CHECK(sent.size()==4&&runs==0);
 for(const auto& p:sent)CHECK(p.first!=1&&p.second[0]==1); // origin printed locally, no second echo
 S::DrainDisplay();CHECK(prints==2&&test::log==L"wrong.command<r>err</>"&&sent.size()==4);
 // Host commands share the same echo/error, without replaying their world effects.
 CHECK(S::OnEnterPre(c)==V::Run);Write(&actor,L"host invalid",1);Write(&actor,L"err");S::OnBodyPost(c);CHECK(sent.size()==10);
 // A client shared-world command executes once in the private host terminal;
 // all clients and the host's visible terminal receive its lines.
 std::vector<uint8_t> request;W::AppU8(request,0);W::AppI32(request,-1);W::AppWStr(request,L"root");W::AppWStr(request,L"");W::AppWStr(request,L"sv.ping");
 S::HostOnBlob(1,request);CHECK(runs==1&&sent.size()==16&&S::g_display.size()==2);
 auto bad=Line(3,L"bad",3);S::HostOnBlob(1,bad);CHECK(sent.size()==16);
 bad=Line(3,L"bad");bad.push_back(0);S::HostOnBlob(1,bad);CHECK(sent.size()==16);
 S::OnPeerGone(1);CHECK(S::g_logRate[1].lines==0);
 // A late atlas and a temporary reflection failure retain replies in order.
 S::OnDisconnect();session.mode=coop::net::Role::Client;S::Install(&session);sent.clear();available=false;
 S::ClientOnBlob(Line(1,L"first"));S::ClientOnBlob(Line(1,L"second"));S::DrainDisplay();CHECK(S::g_display.size()==2);
 available=true;writeOk=false;S::DrainDisplay();CHECK(S::g_display.size()==2);
 writeOk=true;auto old=prints;S::DrainDisplay();CHECK(prints==old+2&&sent.empty());
 // Refused transport keeps the complete line; no extra native execution.
 session.sendOk=false;command=L"help";S::OnEnterPre(c);Write(&actor,L"help",1);S::OnBodyPost(c);CHECK(S::g_out[0].q.size()==1);
 session.sendOk=true;S::Flush(0);CHECK(S::g_out[0].q.empty()&&sent.size()==1);
 command=L"cLr";S::OnEnterPre(c);Write(&actor,L"cLr",1);Write(&actor,L"");S::OnBodyPost(c);S::Flush(0);CHECK(sent.back().second==Line(3,L"",3));
 S::ClientOnBlob(Line(1,L"",3));S::DrainDisplay();CHECK(test::log.empty());
 // Teardown during widget dispatch cannot resurrect a popped line.
 S::ClientOnBlob(Line(1,L"gone"));writeOk=false;callback=[&]{S::OnDisconnect();S::Install(&session);};S::DrainDisplay();CHECK(S::g_display.empty());
 writeOk=true;S::ClientOnBlob(Line(1,L"old world"));++world;S::Install(&session);CHECK(S::g_display.empty());
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);sent.clear();
 S::ShareLog(L"before clear",1,-1,false);S::ShareLog(L"",3,-1,false);S::ShareLog(L"after clear",0,-1,false);
 sent.clear();session.sendOk=false;S::OnPeerWorldReady(2);CHECK(S::g_out[2].q.size()==2);
 session.sendOk=true;S::Flush(2);CHECK(sent.size()==2&&sent[0].second==Line(1,L"",3));
 const auto history=sent;const auto runCount=runs;
 S::OnDisconnect();session.mode=coop::net::Role::Client;S::Install(&session);available=false;
 S::ClientOnBlob(Line(1,L"obsolete pending"));for(const auto& p:history)S::ClientOnBlob(p.second);
 available=true;S::DrainDisplay();CHECK(test::log==L"after clear"&&runs==runCount);
 S::QueueDisplay(L"",3);for(int i=0;i<200;++i)S::QueueDisplay(L"line",0);
 CHECK(S::g_display.size()==S::kDisplayCap&&S::g_display.front().type==3);
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);session.ready=false;
 for(int i=0;i<100;++i)S::ShareLog(std::wstring(200,L'x'),0,-1,false);
 CHECK(S::g_history.size()<=64&&S::g_historyChars<=S::kMaxLogChars);
 S::OnDisconnect();CHECK(S::g_history.empty()&&!S::g_haveHistory);
 puts("PASS real console controller plus2155: shared output, execution once, retry, late history/clear, bounded queue, world/teardown");
}
