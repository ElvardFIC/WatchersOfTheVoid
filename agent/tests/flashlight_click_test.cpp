#ifndef FLASHLIGHT_CLICK_SOURCE
#define FLASHLIGHT_CLICK_SOURCE "../../src/votv-coop/src/coop/player/flashlight_click_sound.cpp"
#endif
#include FLASHLIGHT_CLICK_SOURCE
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <string>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id {Actor,Local,Sound,Attenuation,ActorClass,OtherClass,OtherActor,OtherSound,OtherAttenuation,Count};
struct Obj {int id=0,serial=1;bool live=true,actor=false;void* cls=nullptr;} objects[Count];
void* At(Id id){return &objects[id];}
uint32_t world=1,serial=1;
bool assets=true,attenuationReady=true,locationReady=true;
int finds=0,spawns=0,locations=0,plays=0;Id sound=Sound,attenuation=Attenuation;
ue_wrap::FVector position{12,34,56};
std::function<void()> findCallback,spawnCallback,locationCallback,playCallback;
std::array<coop::roster_ledger::Row,coop::players::kMaxPeers> people;
using Clear=std::pair<coop::roster_ledger::PerSlotClearFn,void*>;
std::vector<Clear>& Subscribers(){static std::vector<Clear> rows;return rows;}
void ClearSlot(int slot){for(auto [fn,self]:Subscribers())fn(self,slot);}
void Callback(std::function<void()>& callback){auto fn=std::move(callback);callback={};if(fn)fn();}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace coop::roster_ledger {
const Row& Get(int slot){CHECK(slot>=0 && slot<players::kMaxPeers);return test::people.at(slot);}
void RegisterPerSlotClear(PerSlotClearFn fn,void* self){test::Subscribers().push_back({fn,self});}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t AllocateSlotSerial(int32_t id){return test::objects[id].serial;}
int32_t SlotSerial(int32_t id){return test::objects[id].serial;}
bool IsLiveByIndex(void* p,int32_t id){return id>=0 && id<test::Count && p==&test::objects[id] && IsLive(p);}
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
void* FindObject(const wchar_t*,const wchar_t*){++test::finds;test::Callback(test::findCallback);return test::assets?test::At(test::sound):nullptr;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p && static_cast<test::Obj*>(p)->actor?CurrentWorld():nullptr;}
}
namespace ue_wrap::engine {
void* SpawnSoundAttenuation(const SoundAttenuationConfig& cfg){
 CHECK(std::fabs(cfg.extents[0]-2000.f/3.f)<.01f && std::fabs(cfg.falloffDistance-20000.f/3.f)<.01f);
 CHECK(cfg.spatialize && cfg.attenuate && cfg.shape==0 && cfg.distanceAlgorithm==2);
 ++test::spawns;test::Callback(test::spawnCallback);return test::attenuationReady?test::At(test::attenuation):nullptr;
}
bool TryGetActorLocation(void*,FVector& out){++test::locations;out=test::position;test::Callback(test::locationCallback);return test::locationReady;}
void PlaySoundAtLocation(void* actor,void* sound,const FVector& pos,void* attenuation,float volume,float pitch){
 CHECK(coop::players::Registry::Get().PeerIdOfActor(actor)<coop::players::kMaxPeers);
 CHECK(sound==test::At(test::sound) && attenuation==test::At(test::attenuation) && attenuation);
 CHECK(volume==1 && pitch==1 && pos.X==12 && pos.Y==34 && pos.Z==56);
 ++test::plays;test::Callback(test::playCallback);
}
}
int main(int argc,char** argv){
 using namespace test;namespace S=coop::flashlight_click_sound;
 const std::string selected=argc>1?argv[1]:"";const auto run=[&](const char* name){return selected.empty() || selected==name;};
 const auto reset=[] {
  ++world;++serial;for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].cls=&objects[i];}
  objects[Actor].actor=objects[Local].actor=objects[OtherActor].actor=true;
  objects[Actor].cls=objects[OtherActor].cls=At(ActorClass);
  auto& registry=coop::players::Registry::Get();registry.puppets={};registry.local=At(Local);registry.puppets[1]=At(Actor);
  for(int slot=0;slot<coop::players::kMaxPeers;++slot){people[slot].playerNo=uint16_t(slot+10);ClearSlot(slot);}
  S::g_sound={};S::g_attenuation={};S::g_constructingAttenuation=false;
  assets=attenuationReady=locationReady=true;finds=spawns=locations=plays=0;sound=Sound;attenuation=Attenuation;
  position={12,34,56};findCallback=spawnCallback=locationCallback=playCallback={};
 };
 const auto click=[](bool enabled=true){S::PlayIfStateChanged(At(Actor),1,enabled);};
 if(run("slot")){
  reset();coop::players::Registry::Get().puppets={};S::PlayIfStateChanged(At(Actor),255,true);CHECK(!finds && !spawns && !locations && !plays);
  auto& registry=coop::players::Registry::Get();registry.puppets[1]=nullptr;registry.puppets[31]=At(Actor);
  S::PlayIfStateChanged(At(Actor),31,true);CHECK(plays==1);puts("PASS flashlight click slot");
 }
 if(run("puppet")){
  reset();S::PlayIfStateChanged(At(Actor),2,true);CHECK(!finds && !plays);S::PlayIfStateChanged(At(Local),1,true);
  S::PlayIfStateChanged(At(OtherActor),1,true);CHECK(!finds && !plays);
  locationCallback=[]{coop::players::Registry::Get().puppets[1]=At(OtherActor);};click();CHECK(!plays);
  puts("PASS flashlight click puppet");
 }
 if(run("lifetime")){
  for(int cause=0;cause<4;++cause){reset();locationCallback=[cause]{if(cause==0)++objects[Actor].serial;if(cause==1)objects[Actor].live=false;
   if(cause==2)objects[Actor].cls=At(OtherClass);if(cause==3)++world;};click();CHECK(!plays && S::g_lastAppliedStateByPeer[1].state==-1);}
  puts("PASS flashlight click lifetime");
 }
 if(run("scope")){
  for(int cause=0;cause<4;++cause){reset();click();CHECK(plays==1);
   if(cause==0)++serial;if(cause==1)++world;if(cause==2)++objects[Actor].serial;if(cause==3)++people[1].playerNo;
   click();CHECK(plays==2);click();CHECK(plays==2);
  }
  puts("PASS flashlight click scope");
 }
 if(run("readiness")){
  for(bool asset:{false,true}){reset();assets=asset;locationReady=!asset;click();CHECK(!plays && S::g_lastAppliedStateByPeer[1].state==-1);
   assets=locationReady=true;click();CHECK(plays==1);click();CHECK(plays==1);
  }
  puts("PASS flashlight click readiness");
 }
 if(run("sound")){
  reset();click();++objects[Sound].serial;click(false);CHECK(finds==2 && plays==2);
  reset();click();objects[Sound].cls=At(OtherClass);sound=OtherSound;click(false);CHECK(finds==2 && plays==2);
  reset();locationCallback=[]{++objects[Sound].serial;};click();CHECK(!plays);click();CHECK(plays==1 && finds==2);
  puts("PASS flashlight click sound");
 }
 if(run("attenuation")){
  reset();click();++objects[Attenuation].serial;click(false);CHECK(spawns==2 && plays==2);
  reset();click();objects[Attenuation].cls=At(OtherClass);attenuation=OtherAttenuation;click(false);CHECK(spawns==2 && plays==2);
  reset();locationCallback=[]{++objects[Attenuation].serial;};click();CHECK(!plays);click();CHECK(plays==1 && spawns==2);
  puts("PASS flashlight click attenuation");
 }
 if(run("reentry")){
  reset();locationCallback=[&]{click();};click();CHECK(plays==1);
  reset();locationCallback=[&]{click(false);};click();CHECK(plays==1 && S::g_lastAppliedStateByPeer[1].state==0);click(false);CHECK(plays==1);
  reset();click(false);locationCallback=[&]{click(false);};click();CHECK(plays==1 && S::g_lastAppliedStateByPeer[1].state==0);
  reset();playCallback=[&]{click();};click();CHECK(plays==1);
  reset();locationCallback=[&]{++people[1].playerNo;ClearSlot(1);click(false);};click();CHECK(plays==1 && S::g_lastAppliedStateByPeer[1].state==0);
  reset();spawnCallback=[&]{click(false);};click();CHECK(!plays && spawns==1);click(false);CHECK(plays==1 && spawns==1);
  puts("PASS flashlight click reentry");
 }
 if(run("position")){
  for(int cause=0;cause<3;++cause){reset();if(cause==0)position.X=NAN;if(cause==1)position.Y=INFINITY;if(cause==2)position.Z=-INFINITY;
   click();CHECK(!plays && S::g_lastAppliedStateByPeer[1].state==-1);position={12,34,56};click();CHECK(plays==1);}
  puts("PASS flashlight click position");
 }
 if(run("no-fallback")){
  reset();attenuationReady=false;click();CHECK(!plays && !locations && S::g_lastAppliedStateByPeer[1].state==-1);
  attenuationReady=true;click();CHECK(plays==1 && spawns==2);
  puts("PASS flashlight click no-fallback");
 }
 return 0;
}
