// Reuse the actual production flashlight controller and its existing engine doubles.
#define main FlashlightLegacyMain
#include "flashlight_2145_test.cpp"
#undef main
#include <set>
namespace test {
void JoinReset(){
 ResetLifetime();
 auto& registry=coop::players::Registry::Get();registry.peer=0;registry.puppet.actor=nullptr;
 lifetimeSession.perSlot=true;
 for(int slot=0;slot<coop::players::kMaxPeers;++slot){
  people[slot].playerNo=static_cast<uint16_t>(slot+10);
  lifetimeSession.generations[slot]=100+slot;
  lifetimeSession.links[slot]=lifetimeSession.lanes[slot]=lifetimeSession.worlds[slot]=true;
 }
 S::g_pollPlayer.Set(&actor);S::g_pollWorld=world;S::g_nextStatePoll=UINT64_MAX;
}
void TickJoin(){S::TickConnect();}
auto& Sent(){return lifetimeSession.sent;}
coop::net::ItemActivatePayload SentState(size_t index){
 CHECK(Sent().at(index).kind==coop::net::ReliableKind::ItemActivate);
 coop::net::ItemActivatePayload value{};std::memcpy(&value,Sent().at(index).data.data(),sizeof(value));return value;
}
void CachePeer(int source,bool on=true,float intensity=5){
 auto p=Payload();p.state=on?1:0;p.intensity=intensity;p.senderElementId=1000+source;
 S::ApplyToPuppetOrDefer(static_cast<uint8_t>(source),nullptr,p);
}
void LocalRetry(){
 JoinReset();S::g_lastSentSig=123;S::QueueConnectBroadcastForSlot(2);
 CHECK(S::g_joinCount==1 && Sent().empty() && S::g_lastSentSig==123);
 lifetimeSession.sendOk=false;TickJoin();CHECK(Sent().empty() && S::g_joinCount==1);
 lifetimeSession.sendOk=true;TickJoin();CHECK(Sent().size()==1 && S::g_joinCount==0);
 CHECK(Sent()[0].slot==2 && Sent()[0].origin==0 && SentState(0).state==1);
 TickJoin();CHECK(Sent().size()==1);
}
void RelayRetry(){
 JoinReset();CachePeer(1);S::ReplayPeerStatesToSlot(2);CHECK(S::g_joinCount==1 && Sent().empty());
 lifetimeSession.sendOk=false;TickJoin();CHECK(Sent().empty() && S::g_joinCount==1);
 lifetimeSession.sendOk=true;TickJoin();CHECK(Sent().size()==1 && !S::g_joinCount);
 CHECK(Sent()[0].slot==2 && Sent()[0].origin==1 && SentState(0).senderElementId==1001);
}
void FreshSample(){
 JoinReset();haveLight=false;S::QueueConnectBroadcastForSlot(2);TickJoin();CHECK(Sent().empty() && S::g_joinCount==1);
 haveLight=true;light.intensity=9;light.outerConeAngle=30;lightState.mode=1;
 TickJoin();CHECK(Sent().size()==1 && SentState(0).intensity==9 && SentState(0).mode==1 && SentState(0).outerConeAngle==30);
 JoinReset();S::QueueConnectBroadcastForSlot(2);lifetimeSession.sendOk=false;TickJoin();
 lightState.hasFlashlight=false;lifetimeSession.sendOk=true;TickJoin();CHECK(Sent().size()==1 && !SentState(0).state);
}
void OffState(){
 JoinReset();lightState.flashlight=false;CachePeer(1,false);S::QueueConnectBroadcastForSlot(2);S::ReplayPeerStatesToSlot(2);
 TickJoin();CHECK(Sent().size()==2 && !S::g_joinCount);
 CHECK(!SentState(0).state && !SentState(1).state);
}
void Recipient(){
 for(int change=0;change<3;++change){
  JoinReset();CachePeer(1);S::QueueConnectBroadcastForSlot(2);S::ReplayPeerStatesToSlot(2);CHECK(S::g_joinCount==2);
  if(change==0)++people[2].playerNo;if(change==1)++lifetimeSession.generations[2];
  if(change==2){S::OnDisconnectForSlot(2);CHECK(!S::g_joinCount);}
  TickJoin();CHECK(Sent().empty() && !S::g_joinCount);
  S::QueueConnectBroadcastForSlot(2);TickJoin();CHECK(Sent().size()==1 && !S::g_joinCount);
 }
 // The target can also retire inside local engine sampling.
 JoinReset();S::QueueConnectBroadcastForSlot(2);onSnapshot=[]{++lifetimeSession.generations[2];};
 TickJoin();CHECK(Sent().empty());TickJoin();CHECK(!S::g_joinCount);
}
void Author(){
 for(int change=0;change<4;++change){
  JoinReset();CachePeer(1);S::ReplayPeerStatesToSlot(2);S::QueueConnectBroadcastForSlot(2);
  if(change==0)++people[1].playerNo;if(change==1)++lifetimeSession.generations[1];
  if(change==2){S::OnDisconnectForSlot(1);CHECK(S::g_joinCount==1);}
  if(change==3)lifetimeSession.links[1]=false;
  TickJoin();CHECK(Sent().size()==1 && Sent()[0].origin==0 && !S::g_joinCount);
 }
 JoinReset();people[1].playerNo=0;CachePeer(1);S::ReplayPeerStatesToSlot(2);CHECK(!S::g_joinCount);
 JoinReset();lifetimeSession.generations[1]=0;CachePeer(1);S::ReplayPeerStatesToSlot(2);CHECK(!S::g_joinCount);
}
void PacketOwnership(){
 JoinReset();CachePeer(1);S::ReplayPeerStatesToSlot(2);
 bool changed=false;lifetimeSession.allowSend=[&](int target,coop::net::ReliableKind,const void*,int){
  CHECK(target==2);if(!changed){changed=true;CachePeer(1,false,8);}return true;
 };
 TickJoin();CHECK(Sent().size()==1 && SentState(0).state==1 && SentState(0).intensity==5 && S::g_joinCount==1);
 lifetimeSession.allowSend={};TickJoin();CHECK(Sent().size()==2 && !SentState(1).state && SentState(1).intensity==8 && !S::g_joinCount);
}
void Completion(){
 JoinReset();S::QueueConnectBroadcastForSlot(2);bool changed=false;
 lifetimeSession.allowSend=[&](int target,coop::net::ReliableKind,const void*,int){
  if(target>=0 && !changed){changed=true;lightState.flashlight=false;S::QueueConnectBroadcastForSlot(2);TickJoin();}return true;
 };
 TickJoin();CHECK(Sent().size()==1 && SentState(0).state==1 && S::g_joinCount==1);
 lifetimeSession.allowSend={};TickJoin();CHECK(Sent().size()==2 && !SentState(1).state && !S::g_joinCount);
 // A nested ordinary publication also leaves a fresh targeted correction owed.
 JoinReset();S::QueueConnectBroadcastForSlot(2);changed=false;
 lifetimeSession.allowSend=[&](int target,coop::net::ReliableKind,const void*,int){
  if(target>=0 && !changed){changed=true;light.intensity=9;CHECK(S::PublishState(&actor));}return true;
 };
 TickJoin();CHECK(S::g_joinCount==1);lifetimeSession.allowSend={};TickJoin();
 CHECK(Sent().back().slot==2 && SentState(Sent().size()-1).intensity==9 && !S::g_joinCount);
}
void Budget(){
 static_assert(coop::players::kMaxPeers==32);
 JoinReset();for(int peer=1;peer<32;++peer)CachePeer(peer);
 for(int target=1;target<32;++target){S::QueueConnectBroadcastForSlot(target);S::ReplayPeerStatesToSlot(target);}
 CHECK(S::g_joinCount==31*31 && Sent().empty());
 std::set<std::pair<int,int>> attempted;
 int attempts=0;lifetimeSession.allowSend=[&](int target,coop::net::ReliableKind,const void*,int){
  ++attempts;CHECK(target>0);return false;
 };
 TickJoin();CHECK(attempts==64 && Sent().empty() && S::g_joinCount==31*31);
 const auto cursor=S::g_joinCursor;TickJoin();CHECK(attempts==128 && S::g_joinCursor!=cursor);
 lifetimeSession.allowSend={};
 for(int turn=0;turn<20 && S::g_joinCount;++turn){const auto before=Sent().size();TickJoin();CHECK(Sent().size()-before<=64);}
 CHECK(!S::g_joinCount && Sent().size()==31*31);
 for(const auto& sent:Sent())CHECK(attempted.emplace(sent.slot,sent.origin).second);
}
void Admission(){
 JoinReset();const auto signature=S::g_lastSentSig.load();
 for(int slot:{-1,0,32,255})S::QueueConnectBroadcastForSlot(slot);
 CHECK(!S::g_joinCount && S::g_lastSentSig==signature);
 people[2].playerNo=0;S::QueueConnectBroadcastForSlot(2);CHECK(!S::g_joinCount);people[2].playerNo=12;
 lifetimeSession.generations[2]=0;S::QueueConnectBroadcastForSlot(2);CHECK(!S::g_joinCount);lifetimeSession.generations[2]=102;
 S::QueueConnectBroadcastForSlot(2);
 for(int phase=0;phase<3;++phase){
  lifetimeSession.links[2]=phase!=0;lifetimeSession.lanes[2]=phase!=1;lifetimeSession.worlds[2]=phase!=2;
  TickJoin();CHECK(Sent().empty() && S::g_joinCount==1);
 }
 lifetimeSession.links[2]=lifetimeSession.lanes[2]=lifetimeSession.worlds[2]=true;
 TickJoin();CHECK(Sent().size()==1 && !S::g_joinCount);
 for(int change=0;change<6;++change){
  JoinReset();S::QueueConnectBroadcastForSlot(2);
  if(change==0)++world;if(change==1)++sessionSerial;if(change==2)lifetimeSession.active=false;
  if(change==3)lifetimeSession.linked=false;if(change==4)lifetimeSession.mode=coop::net::Role::Client;
  if(change==5)S::Install(&otherSession);
  TickJoin();for(const auto& sent:Sent())CHECK(sent.slot<0);CHECK(!S::g_joinCount);
 }
 JoinReset();lifetimeSession.mode=coop::net::Role::Client;S::Install(&lifetimeSession);
 S::g_pollPlayer.Set(&actor);S::g_pollWorld=world;S::g_nextStatePoll=UINT64_MAX;
 lifetimeSession.generations[0]=0;S::QueueConnectBroadcastForSlot(2);S::ReplayPeerStatesToSlot(2);CHECK(!S::g_joinCount);
 S::QueueConnectBroadcastForSlot(0);TickJoin();CHECK(Sent().size()==1 && Sent()[0].slot==0 && !S::g_joinCount);
 JoinReset();S::QueueConnectBroadcastForSlot(2);onSnapshot=[]{lifetimeSession.active=false;};
 TickJoin();CHECK(Sent().empty());TickJoin();CHECK(!S::g_joinCount);
}
}
int main(int argc,char** argv){
 using namespace test;struct Case{const char* name;void(*run)();};
 const Case cases[]={{"local-retry",LocalRetry},{"relay-retry",RelayRetry},{"fresh",FreshSample},{"off",OffState},
  {"recipient",Recipient},{"author",Author},{"packet",PacketOwnership},{"completion",Completion},{"budget",Budget},{"admission",Admission}};
 bool ran=false;for(const auto& item:cases)if(argc<2 || std::string(argv[1])==item.name){
  item.run();std::printf("PASS flashlight join %s\n",item.name);ran=true;
 }
 CHECK(ran);S::OnDisconnect();CHECK(!S::g_joinCount);
}
