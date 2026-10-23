#define PROP_ACTION_DELIVERY_NO_MAIN
#include "prop_action_delivery_test.cpp"
void NativeStart(Role role=Role::Host){Start(role);}
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
 Case("saw-native-getter",selected,[&]{NativeStart();CHECK(A::Sawable(&actor) && getterCalls==1);getterBlocked=true;CHECK(!A::Sawable(&actor));
  for(int cause=0;cause<3;++cause){NativeStart();if(cause==0)haveFunction=false;if(cause==1)getterCallOk=false;if(cause==2)getterReadOk=false;
   CHECK(!A::Sawable(&actor));}
 },count);
 Case("saw-getter-retirement",selected,[&]{for(int cause=0;cause<5;++cause){NativeStart();getterCallback=[&,cause]{
  if(cause==0)++actor.serial;if(cause==1)actorType=&otherClass;if(cause==2)++sawFn.serial;
  if(cause==3)++mreOwner.serial;if(cause==4)++world;};CHECK(!A::Sawable(&actor));}
  NativeStart();getterCallback=[]{CHECK(!A::Sawable(&actor));};CHECK(A::Sawable(&actor) && getterCalls==1);
 },count);
 Case("native-slice",selected,[&]{NativeStart();sawTool=true;A::OnReliable(Payload(2),1);CHECK(sliceCalls==1);
  NativeStart();sawTool=true;getterBlocked=true;A::OnReliable(Payload(2),1);CHECK(!sliceCalls);
  NativeStart();sawTool=true;parametersOk=false;A::OnReliable(Payload(2),1);CHECK(!sliceCalls);
 },count);
 Case("slice-preparation",selected,[&]{for(int stage=0;stage<2;++stage)for(int cause=0;cause<10;++cause){NativeStart();sawTool=true;
  auto retire=[&,cause]{if(cause==0)++coop::net::session_serial::testSerial;if(cause==1)++actor.serial;if(cause==2)++sliceFn.serial;
  if(cause==3)++mreOwner.serial;if(cause==4)++sawFn.serial;if(cause==5)getterCallOk=false;
   if(cause==6)getterBlocked=true;if(cause==7)sawTool=false;if(cause==8)propKey=L"new";if(cause==9)++ui.serial;};
  frameTarget=&sliceFn;
  if(stage==0)frameCallback=retire;else bindCallback=retire;A::OnReliable(Payload(2),1);
  // A fresh valid getter may be read after metadata replacement; all source,
  // slice-function, tool, key and failed/blocked getter changes must refuse.
  CHECK(sliceCalls==int(cause==4));}
 },count);
 Case("client-slice",selected,[&]{NativeStart(Role::Client);sawTool=true;CHECK(A::OnSlice(Slice())==V::Cancel && session.sent.size()==1 && Sent(0).action==2);
  NativeStart(Role::Client);sawTool=true;getterCallback=[]{++actor.serial;};CHECK(A::OnSlice(Slice())==V::Cancel && session.sent.empty());
  NativeStart(Role::Client);sawTool=true;heldCallback=[]{sawTool=false;};CHECK(A::OnSlice(Slice())==V::Cancel && session.sent.empty());
  NativeStart(Role::Client);sawTool=true;auto c=Slice();c.fromOurCode=true;CHECK(A::OnSlice(c)==V::Cancel && session.sent.empty());
 },count);
 CHECK(count>0);std::printf("PASS native prop actions: %d cases, guarded damage/slice/inputs, live tool and key, finite impact and checked native saw getter\n",count);
}
