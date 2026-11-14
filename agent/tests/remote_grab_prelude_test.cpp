#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/actors/prop_events.h"
#include "coop/props/prop_stick_sync.h"
#include "../../src/votv-coop/src/coop/props/remote_prop_drive.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Obj {int id,serial=1;bool live=true;} objects[]={{0},{1}};
uint32_t world=1;
int calls=0,unsticks=0;
using Result=ue_wrap::prop::GrabPreludeResult;
Result result=Result::Applied;
std::function<void()> callback;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<2&&p==&test::objects[i]&&test::objects[i].live;}
int32_t SlotSerial(int32_t i){return i>=0&&i<2?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::prop {
GrabPreludeResult ReplayRemoteGrabPrelude(void*){
    ++test::calls;auto cb=std::move(test::callback);test::callback={};if(cb)cb();return test::result;
}
}
namespace coop::prop_stick_sync {bool ReplayUnstick(void*){++test::unsticks;return true;}}
namespace coop::remote_prop {
#include "grab_prelude.inc"
}
int main(){
    using namespace test;using namespace coop::remote_prop;
    HoldGate hold;void* actor=&objects[0];
    auto reset=[&]{hold={};calls=unsticks=0;result=Result::Applied;callback={};++world;};
    auto run=[&](uint16_t gen=7){return PrepareGrabPrelude(hold,actor,gen,false);};
    reset();CHECK(run()&&run()&&calls==1);CHECK(run(8)&&calls==2);
    CHECK(PrepareGrabPrelude(hold,&objects[1],8,false)&&calls==3);
    ++objects[1].serial;CHECK(PrepareGrabPrelude(hold,&objects[1],8,false)&&calls==4);
    reset();result=Result::NotReady;CHECK(!run()&&!hold.preludeAttempted);
    result=Result::Applied;CHECK(run()&&run()&&calls==2);
    reset();result=Result::Failed;CHECK(!run()&&hold.preludeAttempted);
    result=Result::Applied;CHECK(!run()&&calls==1);CHECK(run(8)&&calls==2);
    reset();callback=[&]{CHECK(!run()&&calls==1);};CHECK(run()&&calls==1);
    reset();callback=[&]{hold={};};CHECK(!run()&&!hold.preludeAttempted);
    reset();callback=[&]{++objects[0].serial;};CHECK(!run());
    reset();callback=[&]{++world;};CHECK(!run());CHECK(run()&&calls==2);
    reset();callback=[&]{CHECK(run(8));};CHECK(!run()&&hold.preludeGen==8&&hold.preludeSucceeded&&calls==2);
    reset();CHECK(PrepareGrabPrelude(hold,actor,7,true)&&unsticks==1&&calls==0);
    CHECK(PrepareGrabPrelude(hold,actor,7,true)&&unsticks==1);
    reset();CHECK(run(65535)&&run(1)&&calls==2);
    reset();callback=[&]{CHECK(!run(0));};CHECK(run(0)&&calls==1);
    CHECK(run(0)&&calls==2); // legacy unversioned restarts retain native semantics
    reset();result=Result::Failed;CHECK(!run(0));result=Result::Applied;
    CHECK(!run(0)&&calls==1); // unversioned packets cannot retry an uncertain dispatch either
    puts("PASS remote prelude: once per actor/hold, readiness retry, uncertain dispatch, reentry, slot reset, serial/world replacement, successor hold and wall-attach routing");
}
