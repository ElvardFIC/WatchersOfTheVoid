#define WATOY_FIXTURE_ONLY
#include "watoy_sync_test.cpp"
int main(){
 using namespace test;using coop::net::Role;
 static_assert(coop::net::kMaxPeers==32);
 coop::net::Session session;Start(session,Role::Client);coop::players::Registry::Get().peer=31;
 S::ReconcileOccupants();mirror=&ui;
 for(int slot=0;slot<31;++slot)S::Receive(Hand(uint32_t(slot+1),1,uint8_t(slot)),0);
 const auto before=applies;S::Tick();CHECK(applies-before==8);
 for(int i=0;i<3;++i){now+=100;S::Tick();}
 for(int slot=0;slot<31;++slot)CHECK(S::g_handViews[slot].Actor().Get());
 Start(session,Role::Host);int attempts=0;
 const auto blocked=std::wstring(L"0000");
 for(int i=0;i<10;++i){const auto label=L"000"+std::to_wstring(i);auto& row=S::g_worldRows[label];
  row.state=World(uint32_t(i+1));coop::net::WireKeyFromString(label,row.state.key);row.pending=S::Mask();S::Bind(row.targets,row.pending);}
 session.allowSend=[&](int,coop::net::ReliableKind,const void* raw,int){++attempts;
  const auto& p=*static_cast<const Packet*>(raw);return coop::net::StringFromWireKey(p.key)!=blocked;};
 S::FlushRows();CHECK(attempts==64);
 for(int i=0;i<20;++i){attempts=0;S::FlushRows();CHECK(attempts<=64);}
 for(const auto& [label,row]:S::g_worldRows)CHECK(label==blocked?row.pending==S::Mask():row.pending==0);
 Start(session,Role::Client);const auto heard=cues;
 for(unsigned i=0;i<64;++i){S::Outbound item{};item.state=World(i+1,2);item.expires=now+1500;item.play=true;S::g_out.push_back(item);}
 S::Drain(now);CHECK(cues-heard==8 && S::g_out.size()==56);
 for(int i=0;i<7;++i){const auto previous=cues;S::Drain(now);CHECK(cues-previous<=8);}
 CHECK(S::g_out.empty() && cues-heard==64);
 Start(session,Role::Host);const auto activated=activations;
 for(unsigned i=0;i<9;++i){S::Outbound item{};item.state=World(i+1,1);
  if(i<8)coop::net::WireKeyFromString(L"missing-"+std::to_wstring(i),item.state.key);
  item.expires=now+3000;item.authorize=true;item.origin=1;item.source=S::CaptureSource(1);S::g_out.push_back(item);
 }
 S::Drain(now);CHECK(activations==activated && S::g_out.size()==9);
 CHECK(coop::net::StringFromWireKey(S::g_out.front().state.key)==key);
 S::Drain(now);CHECK(activations==activated+1);
 puts("PASS 32-peer watoy: eight native hand/cue presentations, 64 sends and fair refused-row retries");
}
