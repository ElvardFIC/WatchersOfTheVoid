#define main PreviousBuildingWrappers
#include "building_wrapper_2153_test.cpp"
#undef main
int main(){PreviousBuildingWrappers();using namespace test;namespace B=ue_wrap::building_material;
 ue_wrap::save_record::SaveRecord base;base.key=key;base.className=L"prop_bucket_C";
 base.floats={{100,200},{38}};base.bools={{1,0,1},{0}};base.ints={{2}};base.names={{L"item"}};base.strings={{L"keep"}};
 const int originalPaints=paints,originalVisibility=visibilityWrites;
 objs[1].name.ComparisonIndex=1;objs[0].height=12.25f;objs[0].soap=3;objs[0].flags=0x82;
 auto rec=base;CHECK(B::RefreshHeldMaterialRecord(&objs[0],rec));
 CHECK(rec.floats[1][0]==12.25f&&rec.ints[0][0]==3&&rec.bools[1][0]==1);
 CHECK(rec.floats[0]==base.floats[0]&&rec.bools[0]==base.bools[0]&&rec.names==base.names&&rec.strings==base.strings);
 objs[0].height=-.5f;objs[0].soap=0;objs[0].flags=0x81;
 CHECK(B::RefreshHeldMaterialRecord(&objs[0],rec)&&rec.floats[1][0]==0&&rec.ints[0][0]==0&&rec.bools[1][0]==0);
 CHECK(objs[0].height==-.5f&&objs[0].flags==0x81); // pure snapshot, native field untouched
 for(int cause=0;cause<7;++cause){auto copy=rec;
  if(cause==0)key=L"another";if(cause==1)fields=false;if(cause==2)objs[0].height=std::numeric_limits<float>::quiet_NaN();
  if(cause==3)objs[0].soap=-1;if(cause==4)copy.floats.clear();if(cause==5)copy.bools.clear();if(cause==6)objs[0].live=false;
  const auto before=copy;CHECK(!B::RefreshHeldMaterialRecord(&objs[0],copy));
  CHECK(copy.floats==before.floats&&copy.ints==before.ints&&copy.bools==before.bools);
  key=base.key;fields=true;objs[0].height=0;objs[0].soap=0;objs[0].live=true;
 }
 for(int id=3;id<=5;++id){objs[1].name.ComparisonIndex=id;rec=base;rec.className=ue_wrap::reflection::ClassNameOf(&objs[0]);
  objs[0].cement=0;objs[0].height=0;objs[0].flags=0x82;CHECK(B::RefreshHeldMaterialRecord(&objs[0],rec));
  if(id==5)CHECK(rec.floats[1][0]==0&&rec.ints==base.ints);
  else CHECK(rec.ints[0][0]==0);
  if(id==3)CHECK(rec.bools[1][0]==1&&rec.bools[0]==base.bools[0]);
  CHECK(rec.floats[0]==base.floats[0]&&rec.names==base.names);
  objs[0].cement=-1;objs[0].height=-1;CHECK(!B::RefreshHeldMaterialRecord(&objs[0],rec));
 }
 objs[1].name.ComparisonIndex=4;rec=base;rec.className=L"prop_cementBag_C";objs[0].maxUnits=5;objs[0].cement=6;
 CHECK(!B::RefreshHeldMaterialRecord(&objs[0],rec));objs[0].cement=5;CHECK(B::RefreshHeldMaterialRecord(&objs[0],rec)&&rec.ints[0][0]==5);
 objs[1].name.ComparisonIndex=3;rec.className=L"prop_concreteBucket_C";objs[0].cement=21;CHECK(!B::RefreshHeldMaterialRecord(&objs[0],rec));
 CHECK(!B::RefreshHeldMaterialRecord(nullptr,rec));rec.className=L"unrelated_C";CHECK(B::RefreshHeldMaterialRecord(nullptr,rec));
 CHECK(paints==originalPaints&&visibilityWrites==originalVisibility);
 puts("PASS held material snapshots: live water/soap/alcohol, cement units/dry and sponge wetness, zero stock, base data, exact identity and no native mutation");
}
