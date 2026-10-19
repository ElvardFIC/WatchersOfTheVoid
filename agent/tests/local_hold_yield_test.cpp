#include "support_2135.h"
#include "coop/player/local_prop_hold.h"
namespace test {
ue_wrap::engine::MainPlayerGrabState state;
void* force=nullptr;bool readOk=true,releaseOk=true;
int handles=0,weapons=0,gravity=0;
}
namespace ue_wrap::engine {
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& out){out=test::state;return test::readOk;}
bool ReleaseMainPlayerGrabIfHolding(void*,void* a){
 ++test::handles;if(test::state.grabbingActor==a)test::state.grabbingActor=nullptr;return true;
}
}
namespace ue_wrap::physics_gun {
bool ReleaseHeldProp(void*,void* a,void*){
 ++test::weapons;if(test::releaseOk&&test::state.physicsActor==a)test::state.physicsActor=nullptr;
 return test::releaseOk;
}
}
namespace coop::gravity_motion {
void* Target(){return test::force;}
void YieldTarget(void* a){++test::gravity;if(test::state.gravityActor==a)test::state.gravityActor=nullptr;
 if(test::force==a)test::force=nullptr;}
}
int main(){
 using namespace test;namespace L=coop::local_prop_hold;
 state.grabbingActor=&actor;state.physicsActor=&actor;state.holdingActor=&ui;
 state.gravityActor=&actor;force=&actor;
 CHECK(L::IsHeld(&actor)&&L::Yield(&actor)&&!L::IsHeld(&actor));
 CHECK(handles==1&&weapons==1&&gravity==1);
 state.physicsActor=&actor;releaseOk=false;CHECK(!L::Yield(&actor)&&L::IsHeld(&actor));
 releaseOk=true;CHECK(L::Yield(&actor));
 state.grabbingActor=&ui;force=&ui;CHECK(L::Yield(&actor));
 CHECK(state.grabbingActor==&ui&&force==&ui); // a competing drive cannot release a different item
 readOk=false;CHECK(!L::Yield(&actor));CHECK(!L::Yield(nullptr));
 puts("PASS actual local yield: native handle, physgun and gravity cleanup, failed release and unrelated-item protection");
}
