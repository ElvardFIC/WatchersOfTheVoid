#include "../../src/votv-coop/src/ue_wrap/actors/food.cpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Object { int id; int serial = 1; bool live = true; uint8_t flags = 0xA0; int type = 2; int foods = 5; };
Object objects[3]{{0}, {1}, {2}};
bool classLoaded = true, correctClass = true, boolProperty = true;
uint8_t mask = 4;
int lookups = 0, refreshes = 0;
std::function<void()> callback;
}
namespace ue_wrap::reflection {
#ifndef CANNED_RECORD_TEST
std::wstring ClassNameOf(void*) { return L"prop_cannedFood_C"; }
#endif
void* FindClass(const wchar_t*) { return &test::objects[0]; }
void* ClassOf(void*) { return &test::objects[2]; }
bool IsDescendantOfAny(void*,void* const*,size_t,int) { return test::correctClass; }
bool IsLive(void* p) { return p && static_cast<test::Object*>(p)->live; }
int32_t InternalIndexOf(void* p) { return static_cast<test::Object*>(p)->id; }
int32_t AllocateSlotSerial(int32_t i) { return test::objects[i].serial; }
int32_t SlotSerial(int32_t i) { return test::objects[i].serial; }
bool IsLiveByIndex(void* p,int32_t i) { return p == &test::objects[i] && test::objects[i].live; }
int32_t FindPropertyOffset(void*,const wchar_t* name) {
    if (std::wcscmp(name, L"type") == 0) return offsetof(test::Object,type);
    if (std::wcscmp(name, L"foods") == 0) return offsetof(test::Object,foods);
    return -1;
}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& mask) {
    CHECK(std::wcscmp(name,L"opened") == 0); ++test::lookups;
    offset = offsetof(test::Object,flags); mask = test::mask; return test::boolProperty;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name) {
    CHECK(std::wcscmp(name,L"init") == 0); return &test::objects[2];
}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name) {
    CHECK(std::wcscmp(name,L"prop_cannedFood_C") == 0);
    return test::classLoaded ? &test::objects[0] : nullptr;
}
}
#ifndef CANNED_RECORD_TEST
namespace ue_wrap::prop { std::wstring GetInteractableKeyString(void*) { return L"can1"; } }
#endif
namespace ue_wrap::world_identity {
void* CurrentWorld() { return nullptr; }
void* WorldOf(void*) { return nullptr; }
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn) : fn_(fn) {}
bool Call(void*,ParamFrame&) {
    ++test::refreshes;
    auto cb = std::move(test::callback); test::callback = {};
    if (cb) cb(); return true;
}
}
int main() {
    using namespace test;
    namespace C = ue_wrap::canned_food;
    auto& can = objects[1]; C::State state;
    CHECK(C::Read(&can,state) && !state.opened && state.type == 2 && state.foods == 5);
    CHECK(C::Apply(&can,{true,3,0}));
    CHECK(can.flags == 0xA4 && can.type == 3 && can.foods == 0 && refreshes == 1);
    CHECK(C::Read(&can,state) && state.opened && state.foods == 0);
    CHECK(C::Apply(&can,{false,0,4095})); CHECK(can.flags == 0xA0 && lookups == 1);
    for (const C::State invalid : {C::State{true,-1,1}, {true,4,1}, {true,1,-1}, {true,1,4096}}) {
        const int before = refreshes; CHECK(!C::Apply(&can,invalid));
        CHECK(can.flags == 0xA0 && can.foods == 4095 && before == refreshes);
    }
    correctClass = false; CHECK(!C::Read(&can,state)); CHECK(!C::Apply(&can,{true,0,1})); correctClass = true;
    classLoaded = false; CHECK(!C::Read(&can,state)); classLoaded = true;
    ++objects[0].serial; mask = 0; CHECK(!C::Read(&can,state));
    mask = 8; boolProperty = false; CHECK(!C::Read(&can,state));
    boolProperty = true; CHECK(C::Apply(&can,{true,2,4})); CHECK(can.flags == 0xA8);
    callback = [&] { ++can.serial; };
    CHECK(!C::Apply(&can,{false,2,3})); // native init recycled the actor
    can.live = false; CHECK(!C::Read(&can,state));
    std::puts("PASS can reflection mask, lineage/ranges, class reload and native lifetime checks");
    return 0;
}
