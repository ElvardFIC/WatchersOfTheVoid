#define CORD_CONTINUATION_NO_MAIN
#include "cord_intent_2170_test.cpp"
int main(){using namespace test;using Kind=coop::net::ReliableKind;
 auto setup=[] {Host();objects[0].socket=&objects[1];objects[1].occupant=&objects[0];session.perSlot=true;
  session.links.fill(false);session.worlds.fill(false);session.generations.fill(1);
  for(int slot:{1,31})session.links[slot]=session.worlds[slot]=true;};
 auto count=[](int slot){size_t n=0;for(const auto& packet:session.sent)if(packet.slot==slot&&packet.kind==Kind::CordState)++n;return n;};
 setup();session.allowSend=[](int slot,Kind,const void*,int){return slot!=31;};
 CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==1&&count(31)==0&&!S::g_sent[0]->accepted);
 for(int i=0;i<8;++i)CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==1&&count(31)==0);
 session.allowSend={};CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==1&&count(31)==1&&S::g_sent[0]->accepted);
 puts("PASS UI04 cord partial acceptance, independent progress, retained refusal beyond five retries");
 setup();session.worlds[31]=false;S::SendAllToSlot(31);CHECK(count(31)==0);
 for(int i=0;i<8;++i){S::g_next=0;S::Tick();}CHECK(count(31)==0&&count(1)==1);
 objects[1].power=true;session.worlds[31]=true;S::g_next=0;S::Tick();CHECK(count(31)==1);
 coop::net::CordStatePayload received{};for(const auto& packet:session.sent)if(packet.slot==31)std::memcpy(&received,packet.data.data(),sizeof received);
 CHECK(received.powered==1);puts("PASS UI04 world-ready join retries use current canonical state");
 setup();CHECK(S::Publish(&objects[0],-1));++session.generations[31];CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==1&&count(31)==2);
 S::OnPeerLeft(31);CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==1&&count(31)==3);
 session.worlds[31]=false;CHECK(S::Publish(&objects[0],-1));session.worlds[31]=true;CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==1&&count(31)==4);
 puts("PASS UI04 generation/departure/readiness replacement resend only the affected recipient");
 setup();session.allowSend=[](int slot,Kind,const void*,int){if(slot==31){objects[1].power=true;S::Publish(&objects[0],-1);}return true;};
 CHECK(!S::Publish(&objects[0],-1));CHECK(!S::g_sent[0]->recipients[31].accepted);
 session.allowSend={};S::Tick();CHECK(S::g_sent[0]->recipients[1].p.powered==1&&S::g_sent[0]->recipients[31].p.powered==1);
 puts("PASS UI04 nested state change cannot commit an old recipient receipt");
 setup();session.links[31]=false;CHECK(S::Publish(&objects[0],-1));session.links[31]=true;CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==1&&count(31)==1);
 now+=2000;CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==2&&count(31)==2);
 --now;CHECK(S::Publish(&objects[0],-1));CHECK(count(1)==2&&count(31)==2);
 puts("PASS UI04 late recipient discovery, per-recipient heartbeat and backwards clock");
}
