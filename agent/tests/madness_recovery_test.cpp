#define main LegacyMadnessMain
#include "madness_2154_test.cpp"
#undef main
namespace test {
namespace S=coop::madness_sync;using P=coop::net::MadnessStatePayload;
coop::net::Session scopedSession;
void Reset(bool client=false){
 callback=onClear={};clearOk=true;S::OnDisconnect();scopedSession={};++sessionSerial;++world;overrideMasterClass=false;masterClass=nullptr;
 actor.live=ui.live=true;++actor.serial;++ui.serial;now=1000;active=true;readOk=true;state={true,true,true,7,20};applies=clears=0;
 for(int i=0;i<coop::net::kMaxPeers;++i)people[i]=uint16_t(i+1);
 if(client)scopedSession.mode=coop::net::Role::Client;S::Install(&scopedSession);
}
P Packet(){return {1,1,7,20,1,1,1,0};}
void SessionRestart(){Reset(true);auto p=Packet();S::Receive(p,0);S::Tick();++sessionSerial;S::Tick();CHECK(!S::g_have&&!S::g_view.started);}
void ScopeRetirement(){for(int mode=0;mode<4;++mode){Reset(true);S::Receive(Packet(),0);S::Tick();int before=applies;
 if(mode==0)scopedSession.active=false;else if(mode==1)scopedSession.mode=coop::net::Role::Host;else if(mode==2)worldUnavailable=true;else ++world;
 S::Tick();CHECK(applies==before);worldUnavailable=false;
}}
void ClockRecovery(){Reset();S::Tick();scopedSession.sent.clear();now=500;S::QueueConnect(2);S::Tick();CHECK(scopedSession.sent.size()==1);
 P p{};std::memcpy(&p,scopedSession.sent[0].data.data(),sizeof(p));CHECK(p.elapsed==20&&std::isfinite(p.elapsed));
 Reset(true);S::Receive(Packet(),0);now=500;S::Tick();CHECK(applied.elapsed==20);
}
void HostShape(){for(int mode=0;mode<3;++mode){Reset();if(mode==0)state.active=false;else if(mode==1)state.kills=-1;else state.elapsed=NAN;S::Tick();CHECK(!S::g_have&&scopedSession.sent.empty());}}
void SourceIdentity(){Reset();callback=[]{++actor.serial;};S::Tick();CHECK(!S::g_have&&scopedSession.sent.empty());
 Reset();overrideMasterClass=true;masterClass=&ui;S::Tick();CHECK(S::g_state.active);active=false;masterClass=&actor;now+=100;S::Tick();CHECK(!S::g_state.active);}
void PausedAge(){Reset(true);auto p=Packet();p.playing=0;S::Receive(p,0);now+=2000;S::Tick();CHECK(applied.elapsed==20&&!applied.playing);
 Reset();state.playing=false;S::Tick();scopedSession.sent.clear();now+=500;S::QueueConnect(2);S::Tick();P wire{};std::memcpy(&wire,scopedSession.sent.back().data.data(),sizeof(wire));CHECK(wire.elapsed==20);
}
void RecipientIdentity(){for(bool person:{false,true}){Reset();scopedSession.sendOk=false;S::Tick();if(person)++people[2];else ++scopedSession.peerEpoch;
 scopedSession.sendOk=true;S::Tick();for(auto& sent:scopedSession.sent)CHECK(sent.slot!=2);}}
void RecipientReadiness(){Reset();scopedSession.perSlot=true;scopedSession.links.fill(true);scopedSession.lanes.fill(true);scopedSession.worlds.fill(false);scopedSession.generations.fill(1);
 S::Tick();CHECK(scopedSession.sent.empty()&&S::g_pending);scopedSession.worlds[2]=true;scopedSession.lanes[2]=false;S::Tick();CHECK(scopedSession.sent.empty());
 scopedSession.lanes[2]=true;S::Tick();CHECK(scopedSession.sent.size()==1&&scopedSession.sent[0].slot==2);}
void Departure(){Reset();scopedSession.sendOk=false;S::Tick();people[2]=0;scopedSession.sendOk=true;S::Tick();CHECK(!(S::g_pending&4));for(auto& sent:scopedSession.sent)CHECK(sent.slot!=2);}
void PacketOwnership(){Reset();bool changed=false;scopedSession.allowSend=[&](int slot,auto,const void* p,int){if(slot==1){const_cast<P*>(static_cast<const P*>(p))->kills=999;changed=true;}return true;};
 S::Tick();CHECK(changed);for(auto& sent:scopedSession.sent)if(sent.slot!=1){P p{};std::memcpy(&p,sent.data.data(),sizeof(p));CHECK(p.kills==7);}}
void QueueRevision(){Reset();S::Tick();scopedSession.sent.clear();S::QueueConnect(2);scopedSession.allowSend=[](int slot,auto,const void*,int){if(slot==2)S::QueueConnect(2);return true;};
 S::Tick();CHECK(S::g_pending&4);scopedSession.allowSend={};S::Tick();CHECK(!(S::g_pending&4));}
void ClearBeforeEvent(){Reset(true);auto p=Packet();S::Receive(p,0);S::Tick();S::g_view.audio.Set(&ui);auto next=p;++next.sequence;++next.eventId;next.kills=9;
 clearOk=false;S::Receive(next,0);now+=100;S::Tick();CHECK(S::g_appliedEvent==p.eventId&&applied.kills==7);
 clearOk=true;now+=100;S::Tick();CHECK(S::g_appliedEvent==next.eventId&&applied.kills==9);}
}
int main(){using namespace test;void(*groups[])()={SessionRestart,ScopeRetirement,ClockRecovery,HostShape,SourceIdentity,PausedAge,RecipientIdentity,RecipientReadiness,Departure,PacketOwnership,QueueRevision,ClearBeforeEvent};for(int i=0;i<12;++i){groups[i]();std::printf("PASS Madness recovery %d\n",i+1);}return 0;}
