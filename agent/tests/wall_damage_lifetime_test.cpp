#define main PriorWallWrapperMain
#include "wall_wrapper_2166_test.cpp"
#undef main
#include <limits>
const char* wallDamageCase="setup";
#undef CHECK
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s line %d: %s\n",wallDamageCase,__LINE__,#x);std::exit(1);}}while(0)
void ResetDamage(){using namespace test;for(int i=0;i<400;++i){objs[i].id=i;++objs[i].serial;objs[i].live=true;objs[i].cls=nullptr;}
 solid=true;locationOK=damageFields=callOk=true;kill=false;damages=0;metadata={};locCallback=nullptr;locationValue={1,2,3};++world;
 objs[0].cls=&objs[7];
}
template<class Fn>void Case(const char* name,const char* selected,Fn fn,int& count){if(selected && std::strcmp(selected,name))return;wallDamageCase=name;fn();++count;}
int main(int argc,char** argv){using namespace test;namespace W=ue_wrap::wall_build;const char* selected=argc>1?argv[1]:nullptr;int count=0;
 Case("wall-damage-native",selected,[&]{ResetDamage();CHECK(W::Damage(&objs[0],20) && damages==1);ResetDamage();kill=true;
  CHECK(W::DamageWhile(&objs[0],20,[]{return true;}) && damages==1 && !objs[0].live);
 },count);
 Case("wall-damage-preparation",selected,[&]{for(int stage=0;stage<2;++stage)for(int cause=0;cause<6;++cause){ResetDamage();bool caller=true;
  auto retire=[&,cause]{if(cause==0)caller=false;if(cause==1)++world;if(cause==2)++objs[0].serial;if(cause==3)objs[0].cls=&objs[8];
   if(cause==4)++objs[6].serial;if(cause==5)solid=false;};
  if(stage==0)metadata=[&](const char* at){if(!std::strcmp(at,"feed-params")){metadata={};retire();}};
  else locCallback=nullptr;
  std::function<bool()> current=[&]{return caller;};
  if(stage==1)metadata=[&](const char* at){if(!std::strcmp(at,"find-feed") && damages==0){metadata={};retire();}};
  const bool result=W::DamageWhile(&objs[0],20,current);
  CHECK(damages==int(stage==1 && cause==4));CHECK(result==bool(damages));}
 },count);
 Case("wall-location-retirement",selected,[&]{for(int cause=0;cause<4;++cause){ResetDamage();bool caller=true;
  // TryGetActorLocation is a native callback boundary in the real wrapper.
  static std::function<void()> callback;
  callback=[&,cause]{if(cause==0)caller=false;if(cause==1)++world;if(cause==2)++objs[6].serial;if(cause==3)objs[0].cls=&objs[8];};
  locCallback=[]{callback();};CHECK(!W::DamageWhile(&objs[0],20,[&]{return caller;}) && !damages);}
 },count);
 Case("wall-damage-refusal",selected,[&]{for(int cause=0;cause<4;++cause){ResetDamage();if(cause==0)locationOK=false;if(cause==1)damageFields=false;
  if(cause==2)locationValue.X=std::numeric_limits<float>::quiet_NaN();if(cause==3)solid=false;
  CHECK(!W::DamageWhile(&objs[0],20,[]{return true;}) && !damages);}
 },count);
 CHECK(count>0);std::printf("PASS wall damage lifetime: %d cases, real metadata/ParamFrame, source/actor/class/function retirement, finite location and native destruction\n",count);
}
