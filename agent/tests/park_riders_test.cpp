#include "../../src/votv-coop/src/coop/props/prop_park.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
int objects[66]; bool simulates=true; int taken=0,given=0;
std::array<bool,66> held{};
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p!=nullptr;}
int32_t InternalIndexOf(void* p){return p?int(static_cast<int*>(p)-test::objects):-1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<66&&p==&test::objects[i];}
int32_t SlotSerial(int32_t){return 1;}
int32_t AllocateSlotSerial(int32_t){return 1;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return 1;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long long CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 1;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::engine {
int AttachedCharactersOf(void*,AttachedCharacter* out,int max) {
    const int count=std::min(max,32);
    for(int i=0;i<count;++i){out[i].updated=&test::objects[2+i];out[i].movement=&test::objects[34+i];}
    return count;
}
bool IsComponentSimulatingPhysics(void*){return test::simulates;}
bool SetComponentSimulatePhysics(void*,bool on){test::simulates=on;return true;}
bool SetActorSimulatePhysics(void*,bool on){test::simulates=on;return true;}
}
namespace coop::rider_hold {
Token Take(void* p){const auto id=ue_wrap::reflection::InternalIndexOf(p);CHECK(!test::held[id]);test::held[id]=true;++test::taken;return id+1;}
void Give(void* p,Token token){const auto id=ue_wrap::reflection::InternalIndexOf(p);CHECK(token==id+1&&test::held[id]);test::held[id]=false;++test::given;}
}
int main() {
    namespace P=coop::prop_park;
    P::Park(&test::objects[0],&test::objects[1]);
    CHECK(test::taken==32&&!test::simulates);
    P::Park(&test::objects[0],&test::objects[1]);CHECK(test::taken==32);
    P::Unpark(&test::objects[0],&test::objects[1],true);CHECK(test::given==32&&test::simulates);
    P::OnDisconnect();CHECK(test::given==32);
    P::Park(&test::objects[0],&test::objects[1]);P::OnDisconnect();CHECK(test::taken==64&&test::given==64);
    std::puts("PASS SI08: 32 rider leases acquired and returned once on release/disconnect");
}
