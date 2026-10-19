#define main PreviousPropActions
#include "prop_action_2143_test.cpp"
#undef main

int main() {
 PreviousPropActions();
 using namespace test;using V=ue_wrap::script_gate::Verdict;using K=ue_wrap::placed_save::Kind;
 Reset();wallKind=K::Wall;session.mode=coop::net::Role::Client;
 Params args{coop::players::Registry::Get().Local(),20.f,{100,0,0}};
 ue_wrap::script_gate::Call c{};c.object=&actor;c.function=&damageFn;c.locals=reinterpret_cast<uint8_t*>(&args);
 CHECK(A::OnDamage(c)==V::Cancel&&session.sent.size()==1&&!wallHits&&!damageCalls);
 coop::net::PropActionIntentPayload p{};std::memcpy(&p,session.sent[0].data.data(),sizeof(p));
 CHECK(p.action==A::kWallDamage&&coop::net::StringFromWireKey(p.key)==wallKey);
 session.mode=coop::net::Role::Host;A::OnReliable(p,1);CHECK(wallHits==1&&appliedDamage==20.f&&!damageCalls);
 for(int reason=0;reason<11;++reason) {
  Reset();wallKind=K::Wall;auto bad=p;
  if(reason==0)coop::element::reach=false;
  if(reason==1)bad.damage=1000;
  if(reason==2)bad.damage=0;
  if(reason==3)bad.clean=1;
  if(reason==4)bad._pad[0]=1;
  if(reason==5)bad.key.len=255;
  if(reason==6)bad.impactY=std::numeric_limits<float>::infinity();
  if(reason==7)wallKind=K::WallOutline;
  if(reason==8)wallKind=K::None;
  if(reason==9)wallResolveCallback=[]{++world;};
  if(reason==10)wallResolveCallback=[]{A::OnDisconnect();};
  A::OnReliable(bad,1);CHECK(!wallHits&&!damageCalls);
 }
 Reset();wallKind=K::Wall;
 A::OnReliable(p,0);A::OnReliable(p,4);CHECK(!wallHits);
 // Non-damage verbs cannot operate on a native-key wall through this envelope.
 for(uint8_t action=1;action<=4;++action){auto q=p;q.action=action;A::OnReliable(q,1);}
 CHECK(!wallHits&&!damageCalls&&!sliceCalls&&!mreCalls);
 Reset();wallKind=K::Wall;session.mode=coop::net::Role::Client;
 args.actor=&ui;CHECK(A::OnDamage(c)==V::Run&&session.sent.empty());args.actor=coop::players::Registry::Get().Local();
 announced=false;CHECK(A::OnDamage(c)==V::Cancel&&session.sent.empty());announced=true;
 c.fromOurCode=true;CHECK(A::OnDamage(c)==V::Cancel&&session.sent.empty());c.fromOurCode=false;
 // Backpressure retries once; a key change/recycled actor discards stale work.
 session.sendOk=false;CHECK(A::OnDamage(c)==V::Cancel&&A::g_pending.size()==1);
 session.sendOk=true;A::Tick();A::Tick();CHECK(session.sent.size()==1&&A::g_pending.empty());
 session.sendOk=false;CHECK(A::OnDamage(c)==V::Cancel);wallKey=L"replacement";
 session.sendOk=true;A::Tick();CHECK(session.sent.size()==1&&A::g_pending.empty());
 session.sendOk=false;CHECK(A::OnDamage(c)==V::Cancel);++actor.serial;
 session.sendOk=true;A::Tick();CHECK(session.sent.size()==1&&A::g_pending.empty());
 // Never truncate a native key into another wall's identity.
 Reset();wallKind=K::Wall;session.mode=coop::net::Role::Client;wallKey.assign(64,L'x');
 CHECK(A::OnDamage(c)==V::Cancel&&session.sent.empty()&&A::g_pending.empty());
 puts("PASS wall hits: native-key routing, host-only damage, weapon/reach/role/class guards, retry, stale identity and world/session reentry");
}
