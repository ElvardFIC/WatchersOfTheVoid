// Real controller, with native food reads/calls and reliable sends as boundaries.
#define main PriorFoodPublisherMain
#include "food_publish_2142_test.cpp"
#undef main
#include <cstring>
const char* foodCase="setup";
#undef CHECK
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %s line %d: %s\n",foodCase,__LINE__,#x);std::exit(1);}}while(0)
using Role=coop::net::Role;
using Verdict=ue_wrap::script_gate::Verdict;
using FoodCall=ue_wrap::script_gate::Call;
uint8_t eatAction=4;
FoodCall Eat(){FoodCall c{};c.object=c.function=&actor;c.locals=&eatAction;c.invocation=1;return c;}
FoodCall Output(){auto c=Eat();c.function=&foodFn;c.invocation=2;foodInvocations.insert(2);return c;}
void Reset(coop::net::Session& session,Role role=Role::Client){
 S::OnDisconnect();session={};session.mode=role;S::g_session=&session;
 ++foodWorld;++coop::net::session_serial::testSerial;
 foodAlive=foodReadable=foodWritable=foodResolvable=true;foodHand=foodMirror=false;foodNative=true;
 foodLocalActor=foodRemoteActor=nullptr;foodLocalSlot=1;foodElementId=1;foodKey=L"meal";eatAction=4;
 foodActorType=foodFunctionOwner=&foodOwner;foodOutputClass=L"prop_foodBox_C";foodOutputs=foodDestroys=0;
 foodKeyCallback=foodFrameCallback=foodBindCallback=foodReadCallback=foodWriteCallback=foodResolveCallback={};
 foodAuthorize=foodOutputCallback={};foodInvocations={1};meal={300,.5f,2,0};
 for(int i=0;i<4;++i)foodRoster[i].playerNo=uint16_t(10+i);
 S::EnsureWorld();
}
template<class Fn>void Case(const char* name,const char* selected,Fn fn,int& count){
 if(selected && std::strcmp(selected,name))return;foodCase=name;fn();++count;
}
int main(int argc,char** argv){const char* selected=argc>1?argv[1]:nullptr;int count=0;coop::net::Session session;
 Case("untracked-eat",selected,[&]{Reset(session);auto c=Eat();c.invocation=0;
  CHECK(S::OnAction(c)==Verdict::Cancel);eatAction=0;CHECK(S::OnAction(c)==Verdict::Run);
  eatAction=4;c.invocation=1;CHECK(S::OnAction(c)==Verdict::Run);auto b=S::g_bite.front();b.actor=&foodOtherType;
  S::g_bite.assign(64,b);CHECK(S::OnAction(c)==Verdict::Cancel);CHECK(session.bites.empty());
 },count);
 Case("invalid-condition",selected,[&]{
  for(int cause=0;cause<6;++cause){Reset(session);if(cause==0)foodReadable=false;if(cause==1)meal.uses=0;
   if(cause==2)meal.uses=-1;if(cause==3)meal.temperature=NAN;if(cause==4)meal.ripeness=INFINITY;if(cause==5)meal.cookProgress=-.5f;
   CHECK(S::OnAction(Eat())==Verdict::Cancel && S::g_bite.empty() && session.bites.empty());}
 },count);
 Case("missing-address",selected,[&]{Reset(session);foodKey.clear();foodElementId=coop::element::kInvalidId;
  CHECK(S::OnAction(Eat())==Verdict::Cancel && S::g_bite.empty());
  Reset(session);foodKey.clear();CHECK(S::OnAction(Eat())==Verdict::Run);meal.uses=1;S::OnActionPost(Eat());
  CHECK(session.bites.size()==1 && session.bites[0].elementId==1 && !session.bites[0].key.len);
 },count);
 Case("nested-eat",selected,[&]{Reset(session);auto inner=Eat();inner.invocation=2;foodInvocations.insert(2);
  foodReadCallback=[&]{CHECK(S::OnAction(inner)==Verdict::Cancel);};
  CHECK(S::OnAction(Eat())==Verdict::Run);meal.uses=1;S::OnActionPost(inner);CHECK(S::g_bite.size()==1);
  S::OnActionPost(Eat());CHECK(session.bites.size()==1 && S::g_bite.empty());
 },count);
 Case("key-retirement",selected,[&]{Reset(session);foodReadCallback=[]{foodKey=L"replacement";};
  CHECK(S::OnAction(Eat())==Verdict::Cancel && S::g_bite.empty());
  Reset(session);CHECK(S::OnAction(Eat())==Verdict::Run);foodKey=L"replacement";meal.uses=1;S::OnActionPost(Eat());CHECK(session.bites.empty());
 },count);
 Case("source-retirement",selected,[&]{
  for(int cause=0;cause<8;++cause){Reset(session);foodReadCallback=[&,cause]{
   if(cause==0)++coop::net::session_serial::testSerial;if(cause==1)session.active=false;if(cause==2)session.mode=Role::Host;
   if(cause==3)++foodWorld;if(cause==4)++foodSerial;if(cause==5)foodActorType=&foodOtherType;
   if(cause==6)S::OnPeerLeft(1);if(cause==7)S::OnDisconnect();};
   CHECK(S::OnAction(Eat())==Verdict::Cancel && session.bites.empty() && S::g_bite.empty());}
 },count);
 Case("session-restart-post",selected,[&]{Reset(session);CHECK(S::OnAction(Eat())==Verdict::Run);meal.uses=1;
  ++coop::net::session_serial::testSerial;S::OnActionPost(Eat());CHECK(session.bites.empty() && S::g_bite.empty());
 },count);
 Case("post-owned-frame",selected,[&]{Reset(session);CHECK(S::OnAction(Eat())==Verdict::Run);meal.uses=1;auto unrelated=Eat();unrelated.invocation=9;
  S::OnActionPost(unrelated);CHECK(S::g_bite.size()==1);auto final=Eat();final.locals=nullptr;
  S::OnActionPost(final);CHECK(session.bites.size()==1 && S::g_bite.empty());
 },count);
 Case("final-dead-delivery",selected,[&]{Reset(session);meal.uses=1;CHECK(S::OnAction(Eat())==Verdict::Run);
  session.sendOk=false;CHECK(S::ConsumesClientDestroy(&actor));foodAlive=false;foodKey=L"dead-must-not-be-read";
  S::OnActionPost(Eat());CHECK(S::g_clientBiteRetry.size()==1);session.sendOk=true;S::RetryBiteRequests(session);
  CHECK(session.bites.size()==1 && coop::net::StringFromWireKey(session.bites[0].key)==L"meal");
 },count);
 Case("output-scope",selected,[&]{Reset(session);foodLocalActor=&actor;auto c=Output();
  CHECK(!S::IsLocalConsumedOutput(&actor,&foodGraph));CHECK(S::OnConsumedOutput(c)==Verdict::Run);
  CHECK(S::IsLocalConsumedOutput(&actor,&foodGraph));CHECK(!S::IsLocalConsumedOutput(&foodOtherType,&foodGraph));
  S::OnConsumedOutputPost(Eat());CHECK(S::IsLocalConsumedOutput(&actor,&foodGraph));
  S::OnConsumedOutputPost(c);CHECK(!S::IsLocalConsumedOutput(&actor,&foodGraph));
 },count);
 Case("hand-cleared-output",selected,[&]{Reset(session);foodLocalActor=&actor;meal.uses=1;CHECK(S::OnAction(Eat())==Verdict::Run);
  foodLocalActor=nullptr;auto c=Output();CHECK(S::OnConsumedOutput(c)==Verdict::Run);
  CHECK(S::IsLocalConsumedOutput(&actor,&foodGraph));S::OnConsumedOutputPost(c);foodAlive=false;S::OnActionPost(Eat());
  CHECK(session.bites.size()==1 && S::g_outputs.empty());
  Reset(session);CHECK(S::OnAction(Eat())==Verdict::Run);CHECK(S::OnConsumedOutput(Output())==Verdict::Cancel);
 },count);
 Case("output-retirement",selected,[&]{
  for(int cause=0;cause<9;++cause){Reset(session);foodLocalActor=&actor;auto c=Output();CHECK(S::OnConsumedOutput(c)==Verdict::Run);
   if(cause==0)foodInvocations.erase(2);if(cause==1)++foodSerial;if(cause==2)++foodFnSerial;if(cause==3)++foodOwnerSerial;
   if(cause==4)++coop::net::session_serial::testSerial;if(cause==5)session.active=false;if(cause==6)foodActorType=&foodOtherType;
   if(cause==7)foodFunctionOwner=&foodOtherType;if(cause==8)++foodRoster[1].playerNo;
   CHECK(!S::IsLocalConsumedOutput(&actor,&foodGraph));}
 },count);
 Case("output-capacity",selected,[&]{Reset(session);foodLocalActor=&actor;auto c=Output();CHECK(S::OnConsumedOutput(c)==Verdict::Run);
  auto inner=c;inner.invocation=3;foodInvocations.insert(3);CHECK(S::OnConsumedOutput(inner)==Verdict::Cancel);
  auto row=S::g_outputs.front();row.source.actor=&foodOtherType;row.source.identity.Set(&foodOtherType);row.source.serial=1;S::g_outputs.assign(64,row);
  CHECK(S::OnConsumedOutput(c)==Verdict::Cancel && S::g_outputs.size()==64);foodInvocations.erase(2);
  CHECK(!S::IsLocalConsumedOutput(&actor,&foodGraph) && S::g_outputs.empty());
 },count);
 Case("output-allowlist",selected,[&]{
  for(const auto* name:{L"prop_foodBox_C",L"prop_food_banana_C",L"prop_food_coffeeCup_C",L"prop_food_poison_C",L"prop_food_snuskLoaf_C"}){
   Reset(session);foodLocalActor=&actor;foodOutputClass=name;const bool verified=name==std::wstring(L"prop_foodBox_C") || name==std::wstring(L"prop_food_banana_C") || name==std::wstring(L"prop_food_coffeeCup_C");
   CHECK((S::OnConsumedOutput(Output())==Verdict::Run)==verified);}
 },count);
 Case("host-final-output",selected,[&]{
  for(const auto* name:{L"prop_foodBox_C",L"prop_food_banana_C",L"prop_food_coffeeCup_C",L"prop_food_poison_C",L"prop_food_funnyshroom_C",L"prop_food_snuskLoaf_C"}){
   Reset(session,Role::Host);meal.uses=1;foodOutputClass=name;coop::net::FoodBitePayload p{};coop::net::WireKeyFromString(L"meal",p.key);
   S::OnBite(p,1);const bool output=name==std::wstring(L"prop_foodBox_C") || name==std::wstring(L"prop_food_banana_C") || name==std::wstring(L"prop_food_coffeeCup_C");
   CHECK(foodOutputs==int(output) && foodDestroys==1 && session.sent.size()==1 && session.sent[0].state.uses==0);}
 },count);
 Case("host-refilled-final",selected,[&]{Reset(session,Role::Host);meal.uses=1;session.beforeSend=[]{meal.uses=3;};
  coop::net::FoodBitePayload p{};coop::net::WireKeyFromString(L"meal",p.key);S::OnBite(p,1);
  CHECK(!foodOutputs && !foodDestroys && meal.uses==3);
 },count);
 Case("host-address-retirement",selected,[&]{
  for(int cause=0;cause<4;++cause){Reset(session,Role::Host);meal.uses=1;foodWriteCallback=[&,cause]{
   if(cause==0)foodKey=L"new-food";if(cause==1)foodActorType=&foodOtherType;if(cause==2)++coop::net::session_serial::testSerial;if(cause==3)session.active=false;};
   const auto writes=foodWrites;coop::net::FoodBitePayload p{};coop::net::WireKeyFromString(L"meal",p.key);S::OnBite(p,1);
   CHECK(foodWrites==writes && !foodOutputs && !foodDestroys && meal.uses==1);}
 },count);
 Case("metadata-retirement",selected,[&]{
  for(int cause=0;cause<4;++cause){Reset(session,Role::Host);meal.uses=1;
   auto retire=[&,cause]{if(cause==0 || cause==2)++foodFnSerial;else ++foodOwnerSerial;};
   if(cause<2)foodFrameCallback=retire;else foodBindCallback=retire;
   coop::net::FoodBitePayload p{};coop::net::WireKeyFromString(L"meal",p.key);S::OnBite(p,1);
   CHECK(!foodOutputs && !foodDestroys);}
 },count);
 CHECK(count>0);std::printf("PASS food callbacks: %d cases, exact eat/output invocations, bounded admission, source/key/class retirement, dead final delivery and canonical packaging\n",count);
}
