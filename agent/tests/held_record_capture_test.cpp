#include "support_2135.h"
#include "coop/items/held_record_capture.h"
#include "coop/props/prop_scale_converge.h"
#include <limits>
namespace test {
ue_wrap::save_record::SaveRecord captured;
bool captureOk=true;
ue_wrap::FVector scale{1,1,1};
int scaleWrites=0;
}
namespace ue_wrap::save_record {
bool CaptureRecord(void*,SaveRecord& out){out=test::captured;test::Callback();return test::captureOk;}
}
namespace ue_wrap::engine {
FVector GetActorScale3D(void*){return test::scale;}
bool SetActorScale3D(void*,const FVector& value){test::scale=value;++test::scaleWrites;return true;}
}
int main(){
 using namespace test;
 using ue_wrap::save_record::SaveRecord;
 // One native snapshot covers batteries, ordinary food, containers and subclasses.
 for(const wchar_t* cls:{L"prop_batts_C",L"prop_food_C",L"prop_foodBox_C",L"prop_cannedFood_C"}){
  SaveRecord old;old.className=cls;old.key=L"held";old.floats={{100}};old.ints={{5}};
  old.xform[4]=123;
  captured=old;captured.floats={{0}};captured.ints={{0}};
  captured.strings={{L"contents"}};captured.xform[4]=999;
  auto copy=old;CHECK(coop::items::CaptureHeldRecord(&actor,copy));
  CHECK(copy.floats[0][0]==0&&copy.ints[0][0]==0&&copy.strings==captured.strings);
  CHECK(copy.xform==old.xform&&copy.key==old.key&&copy.className==old.className);
  for(int cause=0;cause<6;++cause){
   captured=old;copy=old;captureOk=true;actor.live=true;
   if(cause==0)captured.key=L"different";
   if(cause==1)captured.className=L"different";
   if(cause==2)captureOk=false;
   if(cause==3)callback=[]{++actor.serial;};
   if(cause==4)callback=[]{++world;};
   if(cause==5)callback=[]{actor.live=false;};
   CHECK(!coop::items::CaptureHeldRecord(&actor,copy));
   CHECK(copy.floats==old.floats&&copy.ints==old.ints&&copy.key==old.key);
  }
  actor.live=true;captureOk=true;
 }
 SaveRecord empty;CHECK(coop::items::CaptureHeldRecord(nullptr,empty));
 CHECK(coop::props::ConvergePropScale(&actor,{2,3,4})&&scaleWrites==1);
 CHECK(coop::props::ConvergePropScale(&actor,{2,3,4})&&scaleWrites==1);
 CHECK(!coop::props::ConvergePropScale(&actor,{0,1,1}));
 CHECK(!coop::props::ConvergePropScale(&actor,{std::numeric_limits<float>::quiet_NaN(),1,1}));
 CHECK(scaleWrites==1);
 puts("PASS held battery/food/container live capture, empty/zero stock, identity/world/callback rejection and existing-prop scale convergence");
}
