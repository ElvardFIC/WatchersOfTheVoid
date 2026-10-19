#include "../../src/votv-coop/src/ue_wrap/actors/safe.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {struct Obj{uint8_t bits=0xA4;bool live=true;void* cls=nullptr;}actor,cls;int calls=0;bool fields=true,family=true;}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void* a){return static_cast<test::Obj*>(a)->cls;}void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void*){static FName n{};return n;}bool NameEquals(const FName&,const wchar_t*){return test::family;}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,bits);mask=4;return test::fields;}
void* FindDispatchFunctionCached(void*,const wchar_t*){return &test::cls;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f){}
bool Call(void* a,ParamFrame&){CHECK(a==&test::actor);++test::calls;test::actor.bits&=~4;return true;}
}
int main(){using namespace test;namespace S=ue_wrap::safe;actor.cls=&cls;
 bool locked=false;CHECK(S::ReadLocked(&actor,locked)&&locked);CHECK(S::Unlock(&actor)&&calls==1&&actor.bits==0xA0);
 CHECK(S::Unlock(&actor)&&calls==1); // repeat cannot spawn loot
 ue_wrap::save_record::SaveRecord record;record.bools={{1,0,1},{1,1}};record.strings={{L"01234567"}};
 CHECK(S::PreserveUnlocked(&actor,record)&&record.bools[1][0]==0&&record.bools[1][1]==1&&record.bools[0][0]==1);
 CHECK(record.strings[0][0]==L"01234567"&&!S::PreserveUnlocked(&actor,record));
 record.bools.clear();CHECK(S::PreserveUnlocked(&actor,record)&&record.bools[1][0]==0);
 actor.bits|=4;record.bools[1][0]=1;CHECK(!S::PreserveUnlocked(&actor,record)&&record.bools[1][0]==1);
 record.bools[1][0]=0;CHECK(S::PreserveHostLock(&actor,record)&&record.bools[1][0]==1);
 fields=false;CHECK(!S::Unlock(&actor));fields=true;family=false;CHECK(!S::IsSafe(&actor)&&!S::Unlock(&actor));
 family=true;actor.live=false;CHECK(!S::Unlock(&actor));
 puts("PASS safe wrapper: native single unlock, packed bool, stale saved lock merge and unrelated data preservation, missing metadata/lifetime");
}
