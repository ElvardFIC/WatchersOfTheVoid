#define TEST_CUSTOM_ACTION_FIELDS
#define PROP_ACTION_DELIVERY_NO_MAIN
#include "prop_action_delivery_test.cpp"
namespace test {
bool impactField=true,blockedField=true;int32_t blockedOffset=offsetof(Obj,flags),componentBytes=sizeof(Obj);uint8_t blockedMask=4;
std::function<void()> fieldCallback;
}
namespace ue_wrap::reflection {
int32_t FindPropertyOffset(void*,const wchar_t* name){if(!wcscmp(name,L"physicsImpact"))return test::impactField?offsetof(test::Obj,clipboard):-1;return -1;}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask){CHECK(!wcscmp(name,L"isNotSawable"));off=test::blockedOffset;mask=test::blockedMask;test::CallbackOnce(test::fieldCallback);return test::blockedField;}
int32_t StructSize(void* cls){return cls==&test::mesh?test::componentBytes:sizeof(test::Obj);}
}
void NativeStart(Role role=Role::Host){Start(role);test::actor.clipboard=&test::mesh;test::mesh.flags=0;
 test::impactField=test::blockedField=true;test::blockedOffset=offsetof(test::Obj,flags);test::componentBytes=sizeof(test::Obj);test::blockedMask=4;test::fieldCallback={};}
Call Hit(){auto c=Open();args.damage=20;args.impact={10,20,30};c.function=&test::damageFn;return c;}
uint8_t clean=0;
Call Slice(){auto c=Open();c.function=&test::sliceFn;c.locals=&clean;return c;}
int main(int argc,char** argv){using namespace test;const char* selected=argc>1?argv[1]:nullptr;int count=0;
 Case("native-damage",selected,[&]{NativeStart();A::OnReliable(Payload(1,20),1);CHECK(damageCalls==1 && appliedPlayer==&ui && appliedDamage==20);
  NativeStart();hitOk=false;A::OnReliable(Payload(1,20),1);CHECK(!damageCalls);
  NativeStart();finiteLocation=false;A::OnReliable(Payload(1,20),1);CHECK(!damageCalls);
 },count);
 Case("damage-preparation",selected,[&]{for(int stage=0;stage<4;++stage)for(int cause=0;cause<12;++cause){NativeStart();
  auto retire=[&,cause]{if(cause==0)++coop::net::session_serial::testSerial;if(cause==1)session.active=false;
   if(cause==2)++actor.serial;if(cause==3)actorType=&otherClass;if(cause==4)++damageFn.serial;if(cause==5)++mreOwner.serial;
   if(cause==6)++mesh.serial;if(cause==7)meshActor=&otherPlayer;if(cause==8)propKey=L"other";
   if(cause==9)held=L"empty";if(cause==10)++coop::roster_ledger::testRows[1].bornGeneration;if(cause==11)++ui.serial;};
  if(stage==0)frameCallback=retire;if(stage==1)locationCallback=retire;if(stage==2)bindCallback=retire;if(stage==3)hitCallback=retire;
  A::OnReliable(Payload(1,20),1);CHECK(damageCalls==int(stage==0 && (cause==6 || cause==7)));}
 },count);
 Case("damage-target-key",selected,[&]{NativeStart();coop::element::callback=[]{propKey=L"changed";};A::OnReliable(Payload(1,20),1);CHECK(!damageCalls);
  NativeStart();heldCallback=[]{++actor.serial;};A::OnReliable(Payload(1,20),1);CHECK(!damageCalls);
 },count);
 Case("client-input",selected,[&]{for(int cause=0;cause<7;++cause){NativeStart(Role::Client);auto c=Hit();
  argCallback=[&,cause]{if(cause==0)++coop::net::session_serial::testSerial;if(cause==1)++actor.serial;if(cause==2)actorType=&otherClass;
   if(cause==3)coop::players::Registry::Get().local=&otherPlayer;if(cause==4)++damageFn.serial;if(cause==5)++ui.serial;if(cause==6)++coop::roster_ledger::testRows[1].bornGeneration;};
  CHECK(A::OnDamage(c)==V::Cancel && session.sent.empty() && A::g_pending.empty());}
  NativeStart(Role::Client);heldCallback=[]{++actor.serial;};CHECK(A::OnDamage(Hit())==V::Cancel && session.sent.empty());
  NativeStart(Role::Client);auto c=Hit();c.fromOurCode=true;CHECK(A::OnDamage(c)==V::Cancel && session.sent.empty());
 },count);
 Case("saw-packed-bit",selected,[&]{NativeStart();mesh.flags=2;CHECK(A::Sawable(&actor));mesh.flags=6;CHECK(!A::Sawable(&actor));
  mesh.flags=0;for(int cause=0;cause<6;++cause){NativeStart();if(cause==0)impactField=false;if(cause==1)blockedField=false;
   if(cause==2)blockedMask=0;if(cause==3)blockedOffset=-1;if(cause==4)blockedOffset=sizeof(Obj);if(cause==5)componentBytes=1;
   CHECK(!A::Sawable(&actor));}
 },count);
 Case("saw-component-retirement",selected,[&]{for(int cause=0;cause<6;++cause){NativeStart();fieldCallback=[&,cause]{
  if(cause==0)++actor.serial;if(cause==1)actorType=&otherClass;if(cause==2)++mesh.serial;if(cause==3)actor.clipboard=&otherPlayer;
  if(cause==4)impactField=false;if(cause==5)++world;};CHECK(!A::Sawable(&actor));}
 },count);
 Case("native-slice",selected,[&]{NativeStart();sawTool=true;A::OnReliable(Payload(2),1);CHECK(sliceCalls==1);
  NativeStart();sawTool=true;mesh.flags=4;A::OnReliable(Payload(2),1);CHECK(!sliceCalls);
  NativeStart();sawTool=true;parametersOk=false;A::OnReliable(Payload(2),1);CHECK(!sliceCalls);
 },count);
 Case("slice-preparation",selected,[&]{for(int stage=0;stage<2;++stage)for(int cause=0;cause<10;++cause){NativeStart();sawTool=true;
  auto retire=[&,cause]{if(cause==0)++coop::net::session_serial::testSerial;if(cause==1)++actor.serial;if(cause==2)++sliceFn.serial;
   if(cause==3)++mreOwner.serial;if(cause==4)++mesh.serial;if(cause==5)actor.clipboard=&otherPlayer;
   if(cause==6)mesh.flags=4;if(cause==7)sawTool=false;if(cause==8)propKey=L"new";if(cause==9)++ui.serial;};
  if(stage==0)frameCallback=retire;else bindCallback=retire;A::OnReliable(Payload(2),1);
  // A newly bound component/serial is eligible if its current saw flag permits
  // the operation. Source/function/tool/key replacement never authorizes it.
  CHECK(sliceCalls==int(cause==4 || cause==5));}
 },count);
 Case("client-slice",selected,[&]{NativeStart(Role::Client);sawTool=true;CHECK(A::OnSlice(Slice())==V::Cancel && session.sent.size()==1 && Sent(0).action==2);
  NativeStart(Role::Client);sawTool=true;fieldCallback=[]{++actor.serial;};CHECK(A::OnSlice(Slice())==V::Cancel && session.sent.empty());
  NativeStart(Role::Client);sawTool=true;auto c=Slice();c.fromOurCode=true;CHECK(A::OnSlice(c)==V::Cancel && session.sent.empty());
 },count);
 CHECK(count>0);std::printf("PASS native prop actions: %d cases, guarded damage/slice/inputs, live tool and key, finite impact and checked packed saw eligibility\n",count);
}
