#define HAND_TOY_FIXTURE_ONLY
#include "hand_toy_2157_test.cpp"
int main(){
 using namespace test;namespace S=coop::hand_toy_audio;using Packet=coop::net::HandToyAudioPayload;
 static_assert(coop::net::kMaxPeers==32);
 coop::net::Session session;ResetToy(session);session.mode=coop::net::Role::Client;
 coop::players::Registry::Get().peer=31;S::Install(&session);
 for(int slot=0;slot<32;++slot){people[slot]=uint16_t(slot+10);generations[slot]=mirrorGenerations[slot]=7;mirrors[slot]=&ui;}
 for(int slot=0;slot<31;++slot){Packet p{7,uint32_t(slot+1),.15f,uint8_t(slot),0,1,1,people[slot]};S::Receive(p,0);}
 const auto before=applies;S::Tick();CHECK(applies-before==8);
 for(int i=0;i<3;++i){now+=250;S::Tick();}
 for(int slot=0;slot<31;++slot)CHECK(S::g_rows[slot].painted.Get());
 for(int slot=0;slot<31;++slot){Packet p{7,uint32_t(slot+1),1.f,uint8_t(slot),1,0,0,people[slot]};S::Receive(p,0);}
 const auto played=squeaks;++now;S::Tick();CHECK(squeaks-played==8 && S::g_pulses.size()==23);
 for(int i=0;i<3;++i){const auto previous=squeaks;++now;S::Tick();CHECK(squeaks-previous<=8);}
 CHECK(S::g_pulses.empty());
 puts("PASS 32-peer held toys: eight native state/pulse presentations per tick and eventual complete drain");
}
