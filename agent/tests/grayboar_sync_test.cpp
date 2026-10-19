#include "../../src/votv-coop/src/coop/creatures/grayboar_sync.cpp"
#include <cassert>
#include <cwchar>
#include <cstdio>
#include <limits>
#include <cstddef>
namespace test {int body=1,claims=0,watches=0;uint32_t eid=0;bool watchOk=true;
int victim=2,local=3,source=4,relays=0;float lastDamage=0;int lastSlot=-1;
int anim=5,lookups=0,hookAttempts=0;bool resolveVelocity=false,installVelocity=false,presentation=false;
ue_wrap::FVector visualVelocity{100,25,0};void* result=nullptr;
ue_wrap::ufunction_hook::PostNativeCallback velocityPost=nullptr;
bool sourceLive=true,mirror=false,parked=false;
struct Class {ue_wrap::reflection::FName name{};Class* parent=nullptr;};
Class boar{{1,0}},child{{2,0},&boar},other{{3,0}},animation{{4,0}};Class* sourceClass=&boar;
struct Args {void* source=nullptr;float damage=25;};
int sourceOffset=offsetof(Args,source),damageOffset=offsetof(Args,damage),frameSize=sizeof(Args);
ue_wrap::script_gate::PreFn pre=nullptr,damagePre=nullptr,authorityPre[coop::grayboar_sync::kAuthorityEntryCount]{};
int authorityWatches=0,failAuthority=-1;}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::grayboar_actions {
void Install(net::Session*){} void OnDisconnect(){}
void Before(const ue_wrap::script_gate::Call&,uint8_t){}
void After(const ue_wrap::script_gate::Call&,uint8_t){}
}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* fn,int,PreFn pre,PostFn post){
 for(int i=0;i<coop::grayboar_sync::kAuthorityEntryCount;++i) if(!wcscmp(fn,coop::grayboar_sync::kAuthorityEntries[i])) {
  assert(post==&coop::grayboar_sync::AfterNativeAuthority);
  assert(!wcscmp(cls,L"grayboar_C"));test::authorityPre[i]=pre;++test::authorityWatches;
  return test::watchOk && test::failAuthority!=i;
 }
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
namespace coop::prop_drive_stream {
bool IsParked(void*){return test::parked;}
bool TryGetPresentationVelocity(void*,ue_wrap::FVector& out){out=test::visualVelocity;return test::presentation;}
}
namespace ue_wrap::ufunction_hook {
bool InstallPostHook(void*,PostNativeCallback callback,bool){++test::hookAttempts;test::velocityPost=callback;return test::installVelocity;}
void* CurrentResult(){return test::result;}
}
namespace coop::prop_echo_suppress {bool IsMirrorSpawn(void*){return test::mirror;}}
namespace coop::player_damage {void SendPlayerDamage(int slot,float damage){++test::relays;test::lastSlot=slot;test::lastDamage=damage;}}
namespace ue_wrap::reflection {
bool IsLive(void* p){return test::sourceLive&&p==&test::source;}
void* ClassOf(void* p){return p==&test::anim?&test::animation:test::sourceClass;}
void* SuperStructOf(void* cls){return static_cast<test::Class*>(cls)->parent;}
const FName& NameOf(void* cls){return static_cast<test::Class*>(cls)->name;}
bool NameEquals(const FName& name,const wchar_t* text){return (name.ComparisonIndex==1&&!wcscmp(text,L"grayboar_C"))||
 (name.ComparisonIndex==4&&!wcscmp(text,L"grayboar_Skeleton_AnimBlueprint_C"));}
void* FindDispatchFunctionCached(void*,const wchar_t* name){assert(!wcscmp(name,L"GetVelocity"));++test::lookups;return test::resolveVelocity?&test::body:nullptr;}
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
 auto& entities=coop::element::Registry::Get();entities.row.actor=&source;entities.id=42;
 hit(SG::Verdict::Cancel);assert(relays==2); // Adopted save actor: no fresh-spawn mark or drive row.
 parked=true;hit(SG::Verdict::Cancel);parked=false;
 hit(SG::Verdict::Cancel); // Hand takes over: stable host identity still prevents another bite.
 entities.row.actor=&body;hit(SG::Verdict::Run);entities.row.actor=&source;
 entities.row.mirror=false;hit(SG::Verdict::Run);entities.row.mirror=true;
 entities.row.live=false;hit(SG::Verdict::Run);entities.row.live=true;
 entities.present=false;hit(SG::Verdict::Run);entities.present=true;
 entities.id=0;hit(SG::Verdict::Run);
 entities.id=coop::element::kHostRangeSize;hit(SG::Verdict::Run); // Peer-local identity stays native.
 entities.id=coop::element::kInvalidId;hit(SG::Verdict::Run);
 c.object=&victim;hit(SG::Verdict::Cancel);assert(relays==2);c.object=&local;
 session.linked=false;mirror=true;hit(SG::Verdict::Run);session.linked=true;
 session.active=false;hit(SG::Verdict::Run);session.active=true;
 B::OnDisconnect();hit(SG::Verdict::Run);
 puts("PASS boar damage routing: owner relay, host health, adopted mirror/hand transitions, live identity, subclasses and malformed frames");
 assert(authorityWatches==2*B::kAuthorityEntryCount); // Failed registrations retry; successful ones stay installed.
 B::Install(&session);assert(authorityWatches==2*B::kAuthorityEntryCount);
 c.object=&source;entities.id=42;mirror=false;parked=false;
 auto bite=[&](SG::Verdict expected){for(auto guard:authorityPre)assert(guard(c)==expected);};
 bite(SG::Verdict::Cancel); // Adopted boar, including a notify from an earlier montage.
 entities.id=0;bite(SG::Verdict::Run);
 mirror=true;bite(SG::Verdict::Cancel);mirror=false;
 parked=true;bite(SG::Verdict::Cancel);parked=false;
 entities.id=42;entities.row.mirror=false;bite(SG::Verdict::Run);entities.row.mirror=true;
 entities.row.live=false;bite(SG::Verdict::Run);entities.row.live=true;
 entities.row.actor=&body;bite(SG::Verdict::Run);entities.row.actor=&source;
 entities.id=coop::element::kHostRangeSize;bite(SG::Verdict::Run);entities.id=42;
 sourceClass=&child;bite(SG::Verdict::Cancel);sourceClass=&other;bite(SG::Verdict::Run);sourceClass=&boar;
 sourceLive=false;bite(SG::Verdict::Run);sourceLive=true;
 c.object=nullptr;bite(SG::Verdict::Run);c.object=&source;
 session.mode=coop::net::Role::Host;bite(SG::Verdict::Run);session.mode=coop::net::Role::Client;
 session.linked=false;bite(SG::Verdict::Run);session.linked=true;
 session.active=false;bite(SG::Verdict::Run);session.active=true;
 B::OnDisconnect();bite(SG::Verdict::Run);
 puts("PASS native authority: bite overlap/notify, target selection and roar, adopted/held replicas, personal boars, host and teardown");
 // One failed watch must not duplicate or disable its successful neighbours.
 const int beforeWatches=authorityWatches;
 B::g_authorityWatched[2]=false;failAuthority=2;B::Install(&session);
 assert(authorityWatches==beforeWatches+1&&!B::g_authorityWatched[2]);
 failAuthority=-1;B::Install(&session);B::Install(&session);
 assert(authorityWatches==beforeWatches+2&&B::g_authorityWatched[2]);
 B::Install(&session);c.object=&body;lookups=hookAttempts=0;B::g_velocityRetryAt=0;
 tick();tick();assert(lookups==1&&hookAttempts==0); // Resolver retry is throttled.
 resolveVelocity=true;B::g_velocityRetryAt=0;tick();assert(hookAttempts==1&&!B::g_velocityHook);
 installVelocity=true;B::g_velocityRetryAt=0;tick();assert(hookAttempts==2&&B::g_velocityHook);
 tick();assert(hookAttempts==2);
 ue_wrap::FVector output{9,9,9};result=&output;presentation=true;
 auto read=[&]{velocityPost(&source,&anim,nullptr);};
 read();assert(output.X==100&&output.Y==25&&output.Z==0);
 visualVelocity={};read();assert(output.X==0&&output.Y==0&&output.Z==0);
 output={9,9,9};presentation=false;read();assert(output.X==9);presentation=true;
 animation.name.ComparisonIndex=3;read();assert(output.X==9);animation.name.ComparisonIndex=4;
 sourceClass=&other;read();assert(output.X==9);sourceClass=&boar;
 session.mode=coop::net::Role::Host;read();assert(output.X==9);session.mode=coop::net::Role::Client;
 result=nullptr;read();result=&output;assert(output.X==9);
 velocityPost(&source,nullptr,nullptr);assert(output.X==9);
 session.linked=false;read();assert(output.X==9);session.linked=true;
 session.active=false;read();assert(output.X==9);session.active=true;
 B::OnDisconnect();read();assert(output.X==9);
 puts("PASS native animation velocity override: scoped boar AnimBP, native fallthrough, stopped segment, hook retry and teardown");
}
