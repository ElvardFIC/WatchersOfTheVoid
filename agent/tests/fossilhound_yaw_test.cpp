#include "../../src/votv-coop/src/ue_wrap/actors/fossilhound.cpp"
#include <cassert>
#include <cstdio>
namespace test {
    alignas(void*) uint8_t actor[ue_wrap::profile::off::ACharacter_Mesh+sizeof(void*)]{};
    int child,base,other,mesh;
    bool libraryReady=false,nameReady=false,meshLive=true;
    int lookups=0,writes=0;
    ue_wrap::FRotator rotation{};
    ue_wrap::reflection::FName houndName{17,0},otherName{18,0};
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&(p!=&test::mesh||test::meshLive);}
void* ClassOf(void* p){return p==test::actor?&test::child:&test::other;}
void* SuperStructOf(void* p){return p==&test::child?&test::base:nullptr;}
const FName& NameOf(void* p){return p==&test::base?test::houndName:test::otherName;}
}
namespace ue_wrap::fname_utils {
bool Resolved(){return test::libraryReady;}
reflection::FName StringToFName(const std::wstring&){++test::lookups;return test::nameReady?test::houndName:reflection::FName{};}
}
namespace ue_wrap::engine {
FRotator GetComponentWorldRotation(void*){return {0,42,0};}
bool SetComponentWorldRotation(void*,const FRotator& r){++test::writes;test::rotation=r;return true;}
}
int main(){
    using namespace ue_wrap::fossilhound;
    *reinterpret_cast<void**>(test::actor+ue_wrap::profile::off::ACharacter_Mesh)=&test::mesh;
    float yaw=0;
    assert(!ReadBodyYaw(test::actor,yaw)&&test::lookups==0);
    test::libraryReady=true;
    assert(!ReadBodyYaw(test::actor,yaw)&&test::lookups==1);
    test::nameReady=true;
    assert(ReadBodyYaw(test::actor,yaw)&&yaw==42&&test::lookups==2);
    assert(ReadBodyYaw(test::actor,yaw)&&test::lookups==2);
    DriveBodyYaw(test::actor,123);assert(test::writes==1&&test::rotation.Yaw==123);
    DriveBodyYaw(&test::other,55);assert(test::writes==1);
    test::meshLive=false;assert(!ReadBodyYaw(test::actor,yaw));
    DriveBodyYaw(test::actor,55);assert(test::writes==1);
    puts("PASS production fossil hound: delayed FName readiness, subclasses, non-hounds and absent mesh");
}
