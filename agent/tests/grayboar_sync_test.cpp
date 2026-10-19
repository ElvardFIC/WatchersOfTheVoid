#include "../../src/votv-coop/src/coop/creatures/grayboar_sync.cpp"
#include <cassert>
#include <cwchar>
#include <cstdio>
#include <limits>
#include <cstddef>
namespace test {int body=1,claims=0,watches=0;uint32_t eid=0;bool watchOk=true;
int victim=2,local=3,source=4,relays=0;float lastDamage=0;int lastSlot=-1;
bool sourceLive=true,mirror=false,parked=false;
struct Class {ue_wrap::reflection::FName name{};Class* parent=nullptr;};
Class boar{{1,0}},child{{2,0},&boar},other{{3,0}};Class* sourceClass=&boar;
struct Args {void* source=nullptr;float damage=25;};
int sourceOffset=offsetof(Args,source),damageOffset=offsetof(Args,damage),frameSize=sizeof(Args);
ue_wrap::script_gate::PreFn pre=nullptr,damagePre=nullptr;}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* fn,int,PreFn pre,PostFn post){
 assert(!post);
 if(!wcscmp(cls,L"grayboar_C")){assert(!wcscmp(fn,L"ReceiveTick"));test::pre=pre;}
 else {assert(!wcscmp(cls,L"mainPlayer_C")&&!wcscmp(fn,L"Add Player Damage"));test::damagePre=pre;}
 ++test::watches;return test::watchOk;
}
}
namespace coop::prop_element_tracker {
coop::element::ElementId GetPropElementIdForActor(void* p){assert(p==&test::body);return test::eid;}
}
namespace coop::prop_drive_host {
void Claim(void* p,const char*){assert(p==&test::body);++test::claims;}
}
namespace coop::prop_drive_stream {bool IsParked(void*){return test::parked;}}
namespace coop::prop_echo_suppress {bool IsMirrorSpawn(void*){return test::mirror;}}
namespace coop::player_damage {void SendPlayerDamage(int slot,float damage){++test::relays;test::lastSlot=slot;test::lastDamage=damage;}}
namespace ue_wrap::reflection {
bool IsLive(void* p){return test::sourceLive&&p==&test::source;}
void* ClassOf(void*){return test::sourceClass;}
void* SuperStructOf(void* cls){return static_cast<test::Class*>(cls)->parent;}
const FName& NameOf(void* cls){return static_cast<test::Class*>(cls)->name;}
bool NameEquals(const FName& name,const wchar_t* text){return name.ComparisonIndex==1&&!wcscmp(text,L"grayboar_C");}
int32_t FindParamOffset(void*,const wchar_t* name){return !wcscmp(name,L"source")?test::sourceOffset:test::damageOffset;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
}
int main(){
 using namespace test;namespace B=coop::grayboar_sync;
 coop::net::Session session;ue_wrap::script_gate::Call c{};c.object=&body;
 watchOk=false;B::Install(&session);assert(watches==2);
 watchOk=true;B::Install(&session);assert(watches==4);B::Install(&session);assert(watches==4);
 auto tick=[&]{assert(pre(c)==ue_wrap::script_gate::Verdict::Run);};
 tick();assert(claims==0);eid=coop::element::kInvalidId;tick();assert(claims==0);
 eid=coop::element::kHostRangeSize;tick();assert(claims==0);
 eid=42;tick();assert(claims==1); // Late registration recovers without a scan.
 tick();assert(claims==2); // Claim's existing idempotency owns the same generation.
 session.mode=coop::net::Role::Client;tick();assert(claims==2);
 session.mode=coop::net::Role::Host;session.active=false;tick();assert(claims==2);
 session.active=true;B::OnDisconnect();tick();assert(claims==2);
 B::Install(&session);tick();assert(claims==3&&watches==4);
 c.object=nullptr;tick();assert(claims==3);
 puts("PASS actual grayboar body feeder: host identity, delayed registration, client/offline refusal, reconnect and hook retry");
 namespace SG=ue_wrap::script_gate;
 auto& players=coop::players::Registry::Get();players.local=&local;players.puppet=&victim;
 Args args{&source,25};c.object=&victim;c.function=&body;c.locals=reinterpret_cast<uint8_t*>(&args);
 auto hit=[&](SG::Verdict verdict){assert(damagePre(c)==verdict);};
 hit(SG::Verdict::Cancel);assert(relays==1&&lastSlot==2&&lastDamage==25);
 c.object=&local;hit(SG::Verdict::Run);assert(relays==1);
 c.object=&body;hit(SG::Verdict::Cancel);assert(relays==1); // Unknown foreign body is never host health.
 c.object=&victim;sourceClass=&child;hit(SG::Verdict::Cancel);assert(relays==2);
 sourceClass=&other;hit(SG::Verdict::Run);assert(relays==2);
 other.parent=&other;hit(SG::Verdict::Run);other.parent=nullptr; // Broken ancestry stays bounded.
 sourceClass=&boar;sourceLive=false;hit(SG::Verdict::Run);sourceLive=true;
 args.source=nullptr;hit(SG::Verdict::Run);args.source=&source; // Wire replay has no source.
 for(float value : {0.f,-1.f,1001.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  args.damage=value;hit(SG::Verdict::Cancel);assert(relays==2);
 }
 args.damage=25;damageOffset=frameSize;hit(SG::Verdict::Cancel);assert(relays==2);
 damageOffset=offsetof(Args,damage);sourceOffset=-1;hit(SG::Verdict::Run);sourceOffset=offsetof(Args,source);
 session.mode=coop::net::Role::Client;c.object=&local;
 mirror=true;hit(SG::Verdict::Cancel);assert(relays==2);
 mirror=false;parked=true;hit(SG::Verdict::Cancel);parked=false;
 hit(SG::Verdict::Run); // Native non-mirror/personal boar still hits normally.
 c.object=&victim;hit(SG::Verdict::Cancel);assert(relays==2);c.object=&local;
 session.linked=false;mirror=true;hit(SG::Verdict::Run);session.linked=true;
 session.active=false;hit(SG::Verdict::Run);session.active=true;
 B::OnDisconnect();hit(SG::Verdict::Run);
 puts("PASS boar damage routing: host victim preserved, client-owner relay, foreign health guard, mirror dedup, subclasses and malformed frames");
}
