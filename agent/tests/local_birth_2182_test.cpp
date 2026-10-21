#include "coop/props/local_birth_protection.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {int actor,other,world,nextWorld,serial=1;bool live=true,awaiting=false;void* current=&world;}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void*){return 1;}
int32_t AllocateSlotSerial(int32_t){return test::serial;}
int32_t SlotSerial(int32_t){return test::serial;}
bool IsLiveByIndex(void* p,int32_t){return p==&test::actor&&test::live;}
}
namespace ue_wrap::world_identity {
void* WorldOf(void*){return &test::world;}
void* CurrentWorld(){return test::current;}
}
namespace coop::prop_drop_intent {
bool IsPendingHostPlacement(void*){return false;} // real pending queue tested by pending_birth regression
bool IsAwaitingHost(void* p){return test::awaiting&&p==&test::actor&&test::live;}
bool IsRefusedPlacement(void*){return false;}
}
int main(){
    using namespace test;using namespace coop::join_membership_sweep;
    uint64_t epoch=1;bool running=false;
    {
        ReconcilePass first(epoch,running);CHECK(first.Current()&&running);
        {ReconcilePass nested(epoch,running);CHECK(!nested.Current());}
        CHECK(first.Current()&&running);
        ++epoch;CHECK(!first.Current()); // a new bracket/reset cancels only old work
        {ReconcilePass nested(epoch,running);CHECK(!nested.Current());}
        CHECK(running);
    }
    CHECK(!running);
    {ReconcilePass next(epoch,running);CHECK(next.Current());}
    CHECK(!running);
    LocalBirths births;
    CHECK(!ProtectedLocalBirth(&actor,births));
    births[&actor].Set(&actor);CHECK(ProtectedLocalBirth(&actor,births));
    CHECK(!ProtectedLocalBirth(&other,births)&&!ProtectedLocalBirth(nullptr,births));
    ++serial;CHECK(!ProtectedLocalBirth(&actor,births));
    births[&actor].Set(&actor);current=&nextWorld;CHECK(!ProtectedLocalBirth(&actor,births));
    current=&world;live=false;CHECK(!ProtectedLocalBirth(&actor,births));live=true;
    births.clear();awaiting=true;CHECK(ProtectedLocalBirth(&actor,births));
    awaiting=false;CHECK(!ProtectedLocalBirth(&actor,births));
    std::unordered_set<void*> claimed;
    SweepCandidate candidate(&actor);
    CHECK(candidate.Resolve(claimed,births)==&actor);
    claimed.insert(&actor);CHECK(!candidate.Resolve(claimed,births));claimed.clear();
    births[&actor].Set(&actor);CHECK(!candidate.Resolve(claimed,births));births.clear();
    awaiting=true;CHECK(!candidate.Resolve(claimed,births));awaiting=false;
    ++serial;CHECK(!candidate.Resolve(claimed,births));
    candidate=SweepCandidate(&actor);current=&nextWorld;CHECK(!candidate.Resolve(claimed,births));
    current=&world;live=false;CHECK(!candidate.Resolve(claimed,births));
    std::puts("PASS sweep candidate revalidation: late claim/authorship/awaiting, serial reuse, world and death");
    std::puts("PASS local birth protection: authored/awaiting, unrelated actor, serial reuse, world, death and teardown");
}
