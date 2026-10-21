#define main LegacyBirthTests
#include "pending_birth_2130_test.cpp"
#undef main
int main(){using namespace test;CHECK(LegacyBirthTests()==0);
 coop::join_membership_sweep::LocalBirths noAuthorship;
 for(int failure=0;failure<3;++failure){
  Reset();className=L"prop_rifle_C";authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();
  if(failure==0)session.sendOK=false;
  if(failure==1)locationOk=false;
  if(failure==2){foodActor=true;foodReadable=false;}
  for(int n=0;n<=D::kMaxKeyTries;++n)D::Tick(&session);
  CHECK(D::g_pending.empty()&&session.drops.empty()&&publishes==0&&destroys==0);
  CHECK(D::IsRefusedPlacement(&actor)&&coop::join_membership_sweep::ProtectedLocalBirth(&actor,noAuthorship));
  session.sendOK=true;locationOk=true;foodReadable=true;Enqueue();D::Tick(&session);
  CHECK(session.drops.empty()); // a failed item cannot enter the generic author again
  ++serial;CHECK(!D::IsRefusedPlacement(&actor));
 }
 Reset();authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();locationOk=false;
 ue_wrap::CachedObjRef other;other.Set(&nextWorld);D::g_refusedPlacements.assign(256,other);
 for(int n=0;n<=D::kMaxKeyTries;++n)D::Tick(&session);
 CHECK(D::g_pending.size()==1&&D::IsPendingHostPlacement(&actor)&&destroys==0);
 D::g_refusedPlacements.clear();D::Tick(&session);CHECK(D::IsRefusedPlacement(&actor)&&D::g_pending.empty());
 puts("PASS admitted rifle placement: exhausted send/capture/food retries retain local recovery and sweep/fallback protection; bounded recovery pool preserves pending ownership");
}
