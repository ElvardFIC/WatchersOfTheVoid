#include "coop/session/session_manager.h"
#include "coop/net/lobby_announcer.h"
#include "coop/net/peer_limits.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#define UE_LOGI(...) ((void)0)
#define UE_LOGW(...) ((void)0)
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
bool master=true,blocked=false;
int announces=0,announcedLimit=0,deferredLimit=0;
coop::net::Config queued;
struct Thread{template<class F>explicit Thread(F f){f();}void detach(){}};
}
namespace coop::shutdown {bool IsShuttingDown(){return false;}}
namespace coop::join_progress {void Reset(){}}
namespace coop::net::master_slots {struct SelectedSlot{std::string url="test-master";};SelectedSlot Selected(){return {};}
std::string LogName(const std::string& s){return s;}}
namespace coop::session_manager {
namespace net=coop::net;namespace lobby=coop::net::lobby;namespace slots=coop::net::master_slots;
std::atomic<bool> g_actionBusy=false,g_listedState=true,g_hostIsDirect=false;
std::mutex g_pendHostMu;
struct Pending{net::Config cfg;SaveChoice save;bool listed=false;}g_pendingHost;
bool g_hasPendingHost=false;
struct FakeAnnouncer {
 lobby::HostInfo Host(const std::string&,const std::string&,const std::string&,bool,int limit,int,int=0){
  ++test::announces;test::announcedLimit=limit;lobby::HostInfo out;out.ok=test::master;return out;}
 void SetListed(bool){}
};
FakeAnnouncer& Announcer(){static FakeAnnouncer a;return a;}
bool HostRefusedBySubstrate(net::Refusal* =nullptr){return test::blocked;}
net::Config LobbyP2PConfig(net::Role role,const lobby::HostInfo&){net::Config c;c.role=role;c.topology=net::Topology::P2P;return c;}
void QueueStart(const net::Config& c){test::queued=c;}
uint16_t PlayerListenPort(){return net::kDefaultPort;}
void SetHostStatus(const std::string&){}
void EndHostedLobby(){}
std::string UnlistedDirectStatus(const char*,const char*,bool,uint16_t){return {};}
void ArmDeferredAnnounce(const std::string&,const std::string&,const std::string&,bool,int limit,int){test::deferredLimit=limit;}
#include "host_players_flow.inc"
}
int main(){namespace S=coop::session_manager;using coop::session::Reachability;
 S::SaveChoice choice;choice.newGame=true;choice.newName="new save";
 for(int limit=2;limit<=32;++limit)for(bool direct:{false,true})for(bool listed:{false,true})for(bool master:{false,true})for(bool newGame:{false,true}){
  choice.newGame=newGame;choice.slot="existing save";
  test::master=master;test::announces=test::announcedLimit=test::deferredLimit=0;S::g_hasPendingHost=false;
  coop::net::Refusal refused;
  CHECK(S::HostWithSave(choice,"test",false,"",limit,"test-master",{direct?Reachability::Direct:Reachability::Brokered,listed},&refused));
  CHECK(S::g_hasPendingHost && S::g_pendingHost.cfg.playerLimit==limit);
  CHECK(S::g_pendingHost.save.newName==choice.newName);
  CHECK(S::g_pendingHost.save.newGame==newGame && S::g_pendingHost.save.slot==choice.slot);
  if(direct&&!listed)CHECK(test::announces==0 && test::deferredLimit==limit && !S::g_pendingHost.listed);
  else CHECK(test::announces==1 && test::announcedLimit==limit && S::g_pendingHost.listed==master);
  CHECK(!S::g_actionBusy);
 }
 test::master=true;
 for(int limit=2;limit<=32;++limit){S::HostLobby("test","world",false,limit);CHECK(test::queued.playerLimit==limit && test::announcedLimit==limit);
  S::AnnounceEnvHostHidden("test","world",limit);CHECK(test::announcedLimit==limit);}
 S::HostWithSave(choice,"test",false,"",999,"test-master",{Reachability::Direct,false},nullptr);CHECK(S::g_pendingHost.cfg.playerLimit==32);
 S::HostWithSave(choice,"test",false,"",-1,"test-master",{Reachability::Direct,false},nullptr);CHECK(S::g_pendingHost.cfg.playerLimit==2);
 std::puts("PASS real hosting controllers: all 31 limits through new-save/direct/hidden/listed/brokered/offline fallback, legacy host and env announcement");
}
