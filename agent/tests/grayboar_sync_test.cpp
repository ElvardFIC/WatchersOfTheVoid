#include "../../src/votv-coop/src/coop/creatures/grayboar_sync.cpp"
#include <cassert>
#include <cwchar>
#include <cstdio>
#include <limits>
#include <cstddef>
namespace test {int body=1,claims=0,watches=0;uint32_t eid=0;bool watchOk=true;
int victim=2,local=3,relays=0;float lastDamage=0;int lastSlot=-1;
struct Object {void* pawn=nullptr;void* owner=nullptr;void* controller=nullptr;} source,helper,controller;
bool navigationReady=true,statusOk=true,readStatus=true,stopOk=true;
uint8_t moveStatus=3;int statusReads=0,stops=0,reenter=0;uint32_t world=1;
int serials[]={1,1,1};
int anim=5,lookups=0,hookAttempts=0;bool resolveVelocity=false,installVelocity=false,presentation=false;
ue_wrap::FVector visualVelocity{100,25,0};void* result=nullptr;
ue_wrap::ufunction_hook::PostNativeCallback velocityPost=nullptr;
bool sourceLive=true,mirror=false,parked=false;
struct Class {ue_wrap::reflection::FName name{};Class* parent=nullptr;};
Class boar{{1,0}},child{{2,0},&boar},other{{3,0}},animation{{4,0}};Class* sourceClass=&boar;
Class helperClass{{5,0}},controllerClass{{6,0}};
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
 if(!wcscmp(fn,L"ExecuteUbergraph_grayboar")){
  assert(!wcscmp(cls,L"grayboar_C")&&pre==&coop::grayboar_sync::OnNavigationResume&&!post);return test::watchOk;
 }
 for(int i=0;i<coop::grayboar_sync::kAuthorityEntryCount;++i) if(!wcscmp(fn,coop::grayboar_sync::kAuthorityEntries[i].name)&&
     !wcscmp(cls,coop::grayboar_sync::kAuthorityEntries[i].owner)) {
  assert(post!=nullptr);
  test::authorityPre[i]=pre;++test::authorityWatches;
  return test::watchOk && test::failAuthority!=i;
 }
 assert(!post);
 if(!wcscmp(cls,L"grayboar_C")){assert(!wcscmp(fn,L"ReceiveTick"));test::pre=pre;}
 else {assert(!wcscmp(cls,L"mainPlayer_C")&&!wcscmp(fn,L"Add Player Damage"));test::damagePre=pre;}
 ++test::watches;return test::watchOk;
}
}
namespace coop::prop_element_tracker {
uint32_t GetPropElementIdForActor(void* p){assert(p==&test::body);return test::eid;}
}
namespace coop::prop_drive_host {
void Claim(void* p,const char*){assert(p==&test::body);++test::claims;}
}
namespace coop::prop_drive_stream {
bool IsParked(void*){return test::parked;}
bool TryGetPresentationVelocity(void*,ue_wrap::FVector& out){out=test::visualVelocity;return test::presentation;}
}
namespace ue_wrap::ufunction_hook {
}
namespace coop::prop_echo_suppress {bool IsMirrorSpawn(void*){return test::mirror;}}
namespace ue_wrap::ufunction_hook {
bool InstallPostHook(void*,PostNativeCallback callback,bool){++test::hookAttempts;test::velocityPost=callback;return test::installVelocity;}
void* CurrentResult(){return test::result;}
}
namespace coop::player_damage {void SendPlayerDamage(int slot,float damage){++test::relays;test::lastSlot=slot;test::lastDamage=damage;}}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p==&test::source?0:p==&test::helper?1:p==&test::controller?2:-1;}
bool IsLive(void* p){return test::sourceLive&&InternalIndexOf(p)>=0;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&InternalIndexOf(p)==i&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<3?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p==&test::anim?&test::animation:p==&test::helper?&test::helperClass:
 p==&test::controller?&test::controllerClass:test::sourceClass;}
void* SuperStructOf(void* cls){return static_cast<test::Class*>(cls)->parent;}
const FName& NameOf(void* cls){return static_cast<test::Class*>(cls)->name;}
bool NameEquals(const FName& name,const wchar_t* text){return (name.ComparisonIndex==1&&!wcscmp(text,L"grayboar_C"))||
 (name.ComparisonIndex==4&&!wcscmp(text,L"grayboar_Skeleton_AnimBlueprint_C"))||
 (name.ComparisonIndex==5&&!wcscmp(text,L"grayboarPawn_C"))||
 (name.ComparisonIndex==6&&!wcscmp(text,L"AIController"));}
int32_t FindPropertyOffset(void*,const wchar_t* name){
 if(!wcscmp(name,L"pawn")||!wcscmp(name,L"Pawn"))return offsetof(test::Object,pawn);
 if(!wcscmp(name,L"Owner"))return offsetof(test::Object,owner);
 assert(!wcscmp(name,L"Controller"));return offsetof(test::Object,controller);
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){
 if(wcscmp(name,L"GetVelocity")){assert(!wcscmp(name,L"GetMoveStatus")||!wcscmp(name,L"StopMovement"));
  return test::navigationReady?const_cast<wchar_t*>(name):nullptr;}
 ++test::lookups;return test::resolveVelocity?&test::body:nullptr;}
int32_t FindParamOffset(void*,const wchar_t* name){return !wcscmp(name,L"EntryPoint")?0:!wcscmp(name,L"source")?test::sourceOffset:test::damageOffset;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return &test::world;}
void* WorldOf(void*){return &test::world;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(1){}
bool ParamFrame::GetRaw(const wchar_t* name,void* out,int32_t size)const {
 assert(!wcscmp(name,L"ReturnValue")&&size==1);*static_cast<uint8_t*>(out)=test::moveStatus;return test::readStatus;
}
bool Call(void* object,ParamFrame& frame){
 assert(object==&test::controller);
 if(!wcscmp(static_cast<const wchar_t*>(frame.function()),L"GetMoveStatus")){
  ++test::statusReads;
  switch(test::reenter){
   case 1: ++test::serials[0];break;
   case 2: ++test::world;break;
   case 3: coop::grayboar_sync::OnDisconnect();break;
   case 4: test::controller.pawn=nullptr;break;
   case 5: coop::grayboar_sync::StopReplicaNavigation(&test::source);break;
   case 6: coop::element::Registry::Get().row.mirror=false;break;
  }
  return test::statusOk;
 }
 assert(!wcscmp(static_cast<const wchar_t*>(frame.function()),L"StopMovement"));++test::stops;
 // Aborting the task may synchronously dispatch its completion and retry.
 script_gate::Call call{};call.object=&test::source;
 assert(coop::grayboar_sync::OnNativeAuthority(call)==script_gate::Verdict::Cancel);
 coop::grayboar_sync::StopReplicaNavigation(&test::source);
 if(test::stopOk)test::moveStatus=0;
 return test::stopOk;
}
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
 // A child override has a separate declaring-class registration and retry.
 const int chargedIndex=B::kAuthorityEntryCount-1,chargedBefore=authorityWatches;
 assert(!wcscmp(B::kAuthorityEntries[chargedIndex].owner,L"grayboar_overcharged_C"));
 B::g_authorityWatched[chargedIndex]=false;failAuthority=chargedIndex;B::Install(&session);
 assert(authorityWatches==chargedBefore+1&&!B::g_authorityWatched[chargedIndex]);
 failAuthority=-1;B::Install(&session);B::Install(&session);
 assert(authorityWatches==chargedBefore+2&&B::g_authorityWatched[chargedIndex]);
 puts("PASS collision/lightning/charged authority: declaring-class watches, adopted/held mirrors, personal/host fallthrough and child-watch retry");
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
 B::Install(&session);entities.id=42;entities.row.actor=&source;entities.row.mirror=true;
 int32_t entry=15;c.object=&source;c.function=&body;c.locals=reinterpret_cast<uint8_t*>(&entry);frameSize=sizeof(entry);
 auto resume=[&](SG::Verdict expected){assert(B::OnNavigationResume(c)==expected);};
 for(int32_t aiEntry:{15,543,558,966,1016,1381}){entry=aiEntry;resume(SG::Verdict::Cancel);}
 // Target expiry and dest only clear/unbind state; teardown still retires the helper.
 for(int32_t otherEntry:{0,531,563,891,896,981,1004,16409,17479,21194,23477,23553,23558,2921,-1}){
  entry=otherEntry;resume(SG::Verdict::Run);
 }
 entry=15;frameSize=3;resume(SG::Verdict::Run);frameSize=sizeof(entry);
 entities.row.mirror=false;resume(SG::Verdict::Run);entities.row.mirror=true;
 session.mode=coop::net::Role::Host;resume(SG::Verdict::Run);session.mode=coop::net::Role::Client;
 session.linked=false;resume(SG::Verdict::Run);session.linked=true;
 B::OnDisconnect();resume(SG::Verdict::Run);
 puts("PASS AI continuations: move/retry/detected/stuck/path resumes refused; target/delegate cleanup, cosmetics, construction and teardown stay native");
 B::Install(&session);source.pawn=&helper;helper.owner=&source;helper.controller=&controller;controller.pawn=&helper;
 c.object=&source;
 auto stop=[&]{assert(B::OnBodyTick(c)==SG::Verdict::Run);};
 for(uint8_t state:{1,2,3}){moveStatus=state;const int before=stops;stop();assert(stops==before+1&&moveStatus==0);
  stop();assert(stops==before+1);}
 int before=stops;moveStatus=255;stop();assert(stops==before);moveStatus=3;
 navigationReady=false;stop();assert(stops==before);navigationReady=true;
 statusOk=false;stop();assert(stops==before);statusOk=true;
 readStatus=false;stop();assert(stops==before);readStatus=true;
 stopOk=false;stop();assert(stops==++before&&moveStatus==3);stopOk=true;
 stop();assert(stops==++before&&moveStatus==0);moveStatus=3;
 const int beforeReads=statusReads;
 source.pawn=nullptr;stop();source.pawn=&helper;
 helper.owner=nullptr;stop();helper.owner=&source;
 helper.controller=nullptr;stop();helper.controller=&controller;
 controller.pawn=&source;stop();controller.pawn=&helper;
 helperClass.name.ComparisonIndex=3;stop();helperClass.name.ComparisonIndex=5;
 controllerClass.name.ComparisonIndex=3;stop();controllerClass.name.ComparisonIndex=6;
 entities.row.mirror=false;stop();entities.row.mirror=true;
 entities.id=coop::element::kHostRangeSize;stop();entities.id=42;
 sourceLive=false;stop();sourceLive=true;
 session.linked=false;stop();session.linked=true;
 session.active=false;stop();session.active=true;
 session.mode=coop::net::Role::Host;B::StopReplicaNavigation(&source);session.mode=coop::net::Role::Client;
 B::OnDisconnect();stop();B::Install(&session);
 assert(stops==before&&statusReads==beforeReads);
 for(int edge:{1,2,3,4,6}){
  reenter=edge;stop();assert(stops==before);
  B::Install(&session);controller.pawn=&helper;entities.row.mirror=true;
 }
 reenter=5;stop();assert(stops==++before&&moveStatus==0);reenter=0;
 moveStatus=3;sourceClass=&child;stop();assert(stops==++before);sourceClass=&boar;
 // Authority persists while the body hands off from the drive lane to a hand.
 moveStatus=3;parked=true;stop();assert(stops==++before);parked=false;
 moveStatus=3;stop();assert(stops==++before);
 puts("PASS adopted boar navigation: active path abort, idle no-op, late/replaced possession, role/identity guards, retry and reentrant teardown");
}
