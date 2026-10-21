#include "../../src/votv-coop/src/ue_wrap/actors/fire_extinguisher.cpp"
#include "../../src/votv-coop/src/coop/player/hand_extinguisher_sync.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj{int id,serial=1;bool live=true;ue_wrap::reflection::FName name{1,0};float charge=.3f,altCharge=.7f;uint8_t firing=0;void* effect=nullptr;void* audio=nullptr;} objects[4];
bool fields=true,useAlt=false;uint16_t people[4]={10,11,12,13};
std::function<void()> fxCallback;
uint32_t world=1,gen=0;void* local=nullptr;bool active=false,audio=false,ticking=true;int aims=0,activations=0;
bool water=false;int waterBursts=0,waterWrites=0;
bool trowel=false;int concrete=5,cementWrites=0;
bool bucket=false;int bucketWrites=0;ue_wrap::building_material::Water bucketWater{};
bool concreteBucket=false;int concreteBucketWrites=0;ue_wrap::building_material::Stock stock{};
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}void* ClassOf(void* a){return a;}
void* SuperStructOf(void*){return nullptr;}const FName& NameOf(void* a){return static_cast<test::Obj*>(a)->name;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<4&&a==&test::objects[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return i>=0&&i<4?test::objects[i].serial:0;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
int32_t FindPropertyOffset(void*,const wchar_t*){return !test::fields?-1:(test::useAlt?offsetof(test::Obj,altCharge):offsetof(test::Obj,charge));}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,firing);mask=1;return test::fields;}
int32_t InstanceOffset::Of(void*){return std::wstring(prop_)==L"eff_ext"?offsetof(test::Obj,effect):offsetof(test::Obj,audio);}
}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring& n){return {n==L"prop_fireExt_C"?1:2,0};}}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::component_calls {
bool SetActive(void* c,bool on,bool reset){CHECK(!reset);if(c==&test::objects[1])test::active=on;else test::audio=on;++test::activations;
 auto callback=std::move(test::fxCallback);test::fxCallback={};if(callback)callback();return true;}
bool SetVolumeMultiplier(void*,float v){CHECK(v==.5f);return true;}
bool SetAudioPlaying(void* c,bool on,const std::function<bool()>& current){return (!current || current()) && (test::audio==on || SetActive(c,on,false));}
}
namespace ue_wrap::engine {bool SetActorTickEnabled(void*,bool on){test::ticking=on;return true;}bool SetComponentWorldRotation(void*,const FRotator& r){CHECK(r.Yaw==34);++test::aims;return true;}}
namespace coop::hand_item {void* LocalHandActor(){return test::local;}uint32_t Generation(uint8_t){return test::gen;}
void* MirrorActorForSlot(uint8_t){return &test::objects[0];}uint32_t MirrorGeneration(uint8_t){return test::gen;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::roster_ledger {const Row& Get(int slot){static Row rows[4];rows[slot].playerNo=test::people[slot];return rows[slot];}}
namespace ue_wrap::pressure_washer {
bool IsWasher(void* a){return a&&test::water;}
bool Read(void*,bool& fire,float& level){fire=test::objects[0].firing!=0;level=test::objects[0].charge;return test::water;}
bool SetRemoteLevel(void*,float,const std::function<bool()>& current){if((current && !current()) || !test::water)return false;++test::waterWrites;return true;}
bool EmitRemote(void*,const FVector& p,const FRotator& r,const std::function<bool()>& current){CHECK(p.Z==3&&r.Yaw==34);if(current && !current())return false;++test::waterBursts;return true;}
}
namespace ue_wrap::building_material {
bool IsConcreteBucket(void* a){return a&&test::concreteBucket;}
bool ReadStock(void* a,Stock& s){s=test::stock;return IsConcreteBucket(a);}
bool SetRemoteConcreteBucketDisplay(void* a,int32_t units,bool dry,const std::function<bool()>& current){if((current && !current()) || !IsConcreteBucket(a))return false;
    test::stock={StockKind::ConcreteBucket,units,dry};++test::concreteBucketWrites;return true;}
bool IsBucket(void* a){return a&&test::bucket;}
bool ReadWater(void* a,Water& w){w=test::bucketWater;return IsBucket(a);}
bool SetRemoteBucketDisplay(void* a,float height,bool soap,bool alcohol,const std::function<bool()>& current){if((current && !current()) || !IsBucket(a))return false;
    test::bucketWater={height,soap?1:0,alcohol};++test::bucketWrites;return true;}
bool IsTrowel(void* a){return a&&test::trowel;}
bool ReadConcrete(void* a,int32_t& units){units=test::concrete;return IsTrowel(a);}
bool SetRemoteConcrete(void* a,int32_t units,const std::function<bool()>& current){if((current && !current()) || !IsTrowel(a))return false;test::concrete=units;++test::cementWrites;return true;}
}
int main(){using namespace test;namespace S=coop::hand_extinguisher_sync;namespace FX=ue_wrap::fire_extinguisher;
 for(int i=0;i<4;++i)objects[i].id=i;objects[0].effect=&objects[1];objects[0].audio=&objects[2];
 FX::ResolveNames();float charge=99;bool firing=false;fields=false;
 CHECK(!FX::ReadCharge(&objects[0],charge) && charge==99 && !FX::ReadSpraying(&objects[0],firing));fields=true;
 CHECK(FX::ReadCharge(&objects[0],charge) && charge==.3f && FX::ReadSpraying(&objects[0],firing));
 useAlt=true;++objects[0].serial;CHECK(FX::ReadCharge(&objects[0],charge) && charge==.7f);
 useAlt=false;++objects[0].serial;CHECK(FX::ReadCharge(&objects[0],charge) && charge==.3f);
 coop::net::Session session;auto& registry=coop::players::Registry::Get();registry.peer=0;registry.puppet.actor=&objects[3];S::Install(&session);
 coop::net::HandExtinguisherPayload p{};p.slot=1;p.playerNo=people[1];p.generation=8;p.sequence=1;p.spraying=1;p.charge=.3f;
 S::Receive(p,1);S::Tick();CHECK(!active&&session.sent.empty());gen=8;S::Tick();CHECK(active&&audio&&!ticking&&aims>0&&!session.sent.empty());
 p.sequence=2;p.spraying=0;p.charge=0;S::Receive(p,1);S::Tick();CHECK(!active&&!audio);
 p.sequence=3;p.spraying=1;p.charge=std::numeric_limits<float>::quiet_NaN();S::Receive(p,1);S::Tick();CHECK(!active);
 p.charge=.4f;coop::element::reach=false;S::Receive(p,1);S::Tick();CHECK(!active);coop::element::reach=true;S::Tick();CHECK(active);
 const int old=activations;++objects[0].serial;S::Tick();CHECK(activations>old); // recreated mirror, same hand generation
 S::OnDisconnect();++world;session={};session.mode=coop::net::Role::Client;registry.peer=1;local=&objects[0];gen=9;objects[0].firing=1;objects[0].charge=-.0001f;
 S::Install(&session);S::Tick();CHECK(session.sent.size()==1);coop::net::HandExtinguisherPayload sent{};std::memcpy(&sent,session.sent.back().data.data(),sizeof(sent));CHECK(!sent.spraying&&sent.charge==0);
 gen=10;S::g_nextPoll=0;S::Tick();CHECK(session.sent.size()==2); // equal charge still announces a different item
 p.slot=1;p.generation=10;p.sequence=100;S::Receive(p,0);CHECK(S::g_rows[1].state.sequence!=100); // no overwrite of owner's local item
 S::OnDisconnect();++world;local=nullptr;water=true;gen=11;registry.peer=0;session={};S::Install(&session);
 p={};p.slot=1;p.playerNo=people[1];p.generation=11;p.sequence=1;p.spraying=1;p.charge=8;p.tool=1;
 S::Receive(p,1);S::Tick();CHECK(waterWrites==1&&waterBursts==0&&!session.sent.empty());
 S::Tick();CHECK(waterBursts==0); // canonical projectile births own the particles now
 S::g_rows[1].sentAt=S::NowMs()-1600;
 S::Tick();CHECK(waterBursts==0);
 p.sequence=2;p.spraying=0;S::Receive(p,1);S::Tick();CHECK(waterBursts==0&&waterWrites==2);
 p.sequence=3;p.spraying=1;p.charge=11;S::Receive(p,1);CHECK(S::g_rows[1].state.sequence==2);
 p.charge=5;p.tool=2;S::Receive(p,1);CHECK(S::g_rows[1].state.sequence==2);
 p.tool=1;water=false;S::Receive(p,1);S::Tick();CHECK(waterWrites==2); // packet cannot turn another hand tool into a washer
 water=true;gen=12;S::Tick();CHECK(waterWrites==2); // stale hand generation
 S::OnDisconnect();++world;registry.peer=1;session={};session.mode=coop::net::Role::Client;
 local=&objects[0];objects[0].charge=7;objects[0].firing=1;S::Install(&session);S::Tick();
 CHECK(session.sent.size()==1);std::memcpy(&sent,session.sent.back().data.data(),sizeof(sent));
 CHECK(sent.tool==1&&sent.charge==7&&sent.spraying&&sent.generation==12);
 S::OnDisconnect();++world;water=false;trowel=true;local=nullptr;registry.peer=0;session={};gen=13;S::Install(&session);
 p={};p.slot=1;p.playerNo=people[1];p.generation=13;p.sequence=1;p.tool=2;p.charge=5;
 S::Receive(p,1);S::Tick();CHECK(cementWrites==1&&concrete==5&&!session.sent.empty());
 p.sequence=2;p.charge=0;S::Receive(p,1);S::Tick();CHECK(cementWrites==2&&concrete==0);
 p.sequence=3;p.charge=2.5f;S::Receive(p,1);CHECK(S::g_rows[1].state.sequence==2);
 p.charge=5;p.spraying=1;S::Receive(p,1);CHECK(S::g_rows[1].state.sequence==2);
 p.spraying=0;trowel=false;S::Receive(p,1);S::Tick();CHECK(cementWrites==2);trowel=true;
 S::OnDisconnect();++world;registry.peer=1;session={};session.mode=coop::net::Role::Client;
 local=&objects[0];concrete=3;S::Install(&session);S::Tick();CHECK(session.sent.size()==1);
 std::memcpy(&sent,session.sent.back().data.data(),sizeof(sent));CHECK(sent.tool==2&&sent.charge==3&&!sent.spraying);
 S::OnDisconnect();++world;trowel=false;bucket=true;local=nullptr;registry.peer=0;session={};gen=14;S::Install(&session);
 p={};p.slot=1;p.playerNo=people[1];p.generation=14;p.sequence=1;p.tool=3;p.charge=30;p.reserved=3;
 S::Receive(p,1);S::Tick();CHECK(bucketWrites==1&&bucketWater.height==30&&bucketWater.soap==1&&bucketWater.alcohol&&!session.sent.empty());
 p.sequence=2;p.charge=0;p.reserved=0;S::Receive(p,1);S::Tick();CHECK(bucketWrites==2&&bucketWater.height==0&&!bucketWater.soap&&!bucketWater.alcohol);
 for(int cause=0;cause<5;++cause){auto bad=p;bad.sequence=3;
  if(cause==0)bad.charge=39;if(cause==1)bad.spraying=1;if(cause==2)bad.reserved=4;
  if(cause==3)bad.reserved=2;if(cause==4){bad.tool=2;bad.reserved=1;}
  S::Receive(bad,1);CHECK(S::g_rows[1].state.sequence==2);}
 p.sequence=3;p.charge=15;p.reserved=1;bucket=false;S::Receive(p,1);S::Tick();CHECK(bucketWrites==2);
 bucket=true;gen=15;S::Tick();CHECK(bucketWrites==2); // stale hand generation never paints a replacement
 gen=14;S::Tick();CHECK(bucketWrites==3&&bucketWater.soap==1);++objects[0].serial;
 S::Tick();CHECK(bucketWrites==4); // recreate same logical mirror
 session.sent.clear();S::QueueConnect(2);S::Tick();CHECK(!session.sent.empty()); // late peer gets current display
 S::OnDisconnect();++world;registry.peer=1;session={};session.mode=coop::net::Role::Client;
 local=&objects[0];gen=16;bucketWater={23,9,true};S::Install(&session);S::Tick();CHECK(session.sent.size()==1);
 std::memcpy(&sent,session.sent.back().data.data(),sizeof(sent));CHECK(sent.tool==3&&sent.charge==23&&sent.reserved==3&&!sent.spraying&&bucketWater.soap==9);
 bucketWater.soap=0;S::g_nextPoll=0;S::Tick();CHECK(session.sent.size()==2); // flags-only change is observable
 session.sendOk=false;bucketWater.height=12;S::g_nextPoll=0;S::Tick();CHECK(session.sent.size()==2);
 session.sendOk=true;S::g_nextPoll=0;S::Tick();CHECK(session.sent.size()==3); // failed send does not swallow the update
 S::OnDisconnect();++world;bucket=false;concreteBucket=true;local=nullptr;registry.peer=0;session={};gen=17;S::Install(&session);
 p={};p.slot=1;p.playerNo=people[1];p.generation=17;p.sequence=1;p.tool=4;p.charge=12;p.reserved=1;
 S::Receive(p,1);S::Tick();CHECK(concreteBucketWrites==1&&stock.units==12&&stock.dry&&!session.sent.empty());
 p.sequence=2;p.charge=0;p.reserved=0;S::Receive(p,1);S::Tick();CHECK(concreteBucketWrites==2&&stock.units==0&&!stock.dry);
 for(int cause=0;cause<5;++cause){auto bad=p;bad.sequence=3;
  if(cause==0)bad.charge=21;if(cause==1)bad.charge=2.5f;if(cause==2)bad.spraying=1;
  if(cause==3)bad.reserved=2;if(cause==4)bad.charge=std::numeric_limits<float>::quiet_NaN();
  S::Receive(bad,1);CHECK(S::g_rows[1].state.sequence==2);}
 p.sequence=3;p.charge=5;concreteBucket=false;S::Receive(p,1);S::Tick();CHECK(concreteBucketWrites==2);
 concreteBucket=true;gen=18;S::Tick();CHECK(concreteBucketWrites==2);gen=17;S::Tick();CHECK(concreteBucketWrites==3);
 session.sent.clear();S::QueueConnect(2);S::Tick();CHECK(!session.sent.empty());
 S::OnDisconnect();++world;registry.peer=1;session={};session.mode=coop::net::Role::Client;local=&objects[0];gen=18;
 stock={ue_wrap::building_material::StockKind::ConcreteBucket,-2,true};S::Install(&session);S::Tick();
 CHECK(session.sent.size()==1);std::memcpy(&sent,session.sent.back().data.data(),sizeof(sent));
 CHECK(sent.tool==4&&sent.charge==0&&sent.reserved==1&&!sent.spraying&&stock.units==-2);
 stock.units=9;stock.dry=false;S::g_nextPoll=0;S::Tick();CHECK(session.sent.size()==2);
 std::memcpy(&sent,session.sent.back().data.data(),sizeof(sent));CHECK(sent.charge==9&&sent.reserved==0);
 S::OnDisconnect();++world;water=trowel=bucket=concreteBucket=false;local=nullptr;registry.peer=0;session={};gen=20;S::Install(&session);
 p={};p.slot=1;p.playerNo=people[1];p.generation=20;p.sequence=1;p.spraying=1;p.charge=.5f;
 S::Receive(p,1);S::Tick();CHECK(active && audio);
 S::g_rows[1].sentAt=S::NowMs()-1601;S::Tick();CHECK(!active && !audio);const auto stopped=activations;
 S::Tick();CHECK(activations==stopped); // an expired spray stops once, not every frame
 ++p.sequence;S::Receive(p,1);S::Tick();CHECK(active);++gen;S::Tick();CHECK(!active && !audio);
 p.generation=gen;++p.sequence;S::Receive(p,1);S::Tick();CHECK(active);
 ++people[1];S::Tick();CHECK(!active && !audio && !S::g_rows[1].have);
 S::Receive(p,1);CHECK(!S::g_rows[1].have);p.playerNo=people[1];p.sequence=1;S::Receive(p,1);S::Tick();CHECK(active);
 S::g_rows[1].sentAt=S::NowMs()-1601;session.sent.clear();S::QueueConnect(2);S::Tick();
 for(const auto& packet:session.sent){coop::net::HandExtinguisherPayload replay{};std::memcpy(&replay,packet.data.data(),sizeof(replay));CHECK(!replay.spraying);}
 S::OnDisconnect();CHECK(!active && !audio);
 S::Install(&session);S::Receive(p,1);S::Tick();CHECK(active);
 auto nextTool=p;nextTool.generation=gen+1;nextTool.sequence++;nextTool.tool=3;nextTool.spraying=0;nextTool.charge=20;
 S::Receive(nextTool,1);S::Tick();CHECK(!active && !audio); // retire the old tool's effects, not the pending tool's kind
 S::OnDisconnect();
 S::Install(&session);S::Receive(p,1);S::Tick();CHECK(active);
 auto replacing=p;replacing.playerNo=++people[1];replacing.sequence=1;
 fxCallback=[&]{auto newest=replacing;newest.playerNo=++people[1];newest.sequence=2;S::Receive(newest,1);};
 S::Receive(replacing,1);CHECK(S::g_rows[1].state.playerNo==people[1] && S::g_rows[1].state.sequence==2);S::Tick();CHECK(active);
 S::OnDisconnect();p.playerNo=people[1];
 S::Install(&session);S::Receive(p,1);S::Tick();CHECK(active && audio);
 S::g_rows[1].sentAt=S::NowMs()-1601;
 auto refreshed=p;refreshed.sequence++;refreshed.charge=.8f;
 fxCallback=[&]{S::Receive(refreshed,1);};S::Tick();
 CHECK(!S::g_rows[1].applied && S::g_rows[1].state.sequence==refreshed.sequence);
 S::Tick();CHECK(active && audio && objects[0].charge==.8f);
 S::OnDisconnect();S::Install(&session);
 auto stopPacket=p;stopPacket.sequence+=2;stopPacket.spraying=0;
 fxCallback=[&]{S::Receive(stopPacket,1);};S::Receive(p,1);S::Tick();
 CHECK(!audio && !S::g_rows[1].applied && S::g_rows[1].state.sequence==stopPacket.sequence);
 S::Tick();CHECK(!active && !audio);S::OnDisconnect();
 S::Install(&session);auto bucketPacket=p;bucketPacket.generation=gen+1;bucketPacket.sequence++;bucketPacket.tool=3;bucketPacket.spraying=0;bucketPacket.charge=20;
 fxCallback=[&]{bucket=true;++gen;S::Receive(bucketPacket,1);};S::Receive(p,1);S::Tick();
 CHECK(!S::g_rows[1].applied && S::g_rows[1].appliedActor.Get());S::Tick();CHECK(!active && !audio && S::g_rows[1].appliedTool==3);
 S::OnDisconnect();bucket=false;p.generation=gen;
 S::Install(&session);S::Receive(p,1);bool replacedRecipient=false;
 session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !replacedRecipient){replacedRecipient=true;++people[2];S::QueueConnect(2);}return true;};
 S::Tick();CHECK(replacedRecipient && (S::g_rows[1].pending&(1u<<2)));
 session.allowSend={};S::Tick();CHECK(!(S::g_rows[1].pending&(1u<<2)));S::OnDisconnect();
 S::Install(&session);session.ready=false;S::Receive(p,1);CHECK(!S::g_rows[1].have);session.ready=true;
 S::Receive(p,1);coop::element::callback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session.load() && !active);
 S::Install(&session);S::Receive(p,1);S::Tick();CHECK(active && audio);
 auto nativeCalls=activations;++world;S::Tick();CHECK(activations==nativeCalls && !S::g_rows[1].have);
 S::Receive(p,1);S::Tick();coop::net::Session replacementSession,requestedSession;
 fxCallback=[&]{S::Install(&replacementSession);S::Receive(p,1);S::Tick();};S::Install(&requestedSession);
 CHECK(S::g_session.load()==&replacementSession && S::g_rows[1].have && active);S::OnDisconnect();
 puts("PASS hand tools: missing-field/class-serial retry, lost-stop expiry, stow, occupant reuse, world-ready admission, silent stale join, newer-packet interruption and reentrant teardown");
 puts("PASS concrete bucket hand sync: stock/dry/empty, bounds/fraction/flags/class/generation, late join and owner depletion without mutation");
 puts("PASS hand bucket display: water/foam/alcohol, owner unchanged, validation, class/generation/recreation, join replay and retry");
 puts("PASS trowel amount/depletion/owner sample, foreign class/fraction/spray guards");
 puts("PASS washer relay/owner sampling, tool/level/generation guards and no duplicate hand burst beside canonical projectiles");
 puts("PASS actual extinguisher particle/audio without native gameplay tick, mirror ordering/recreation, reach/NaN, depletion, generation and owner guards");
}
