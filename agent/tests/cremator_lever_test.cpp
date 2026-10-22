#include "../../src/votv-coop/src/ue_wrap/devices/cremator.cpp"
#include <cassert>
#include <cwchar>
#include <cstdio>

namespace test {
struct Obj {
    int id, serial = 1;
    bool live = true;
    void* cls = nullptr;
    bool anim = false, info = true, looking = true, active = false;
};
Obj machine{0}, cls{1}, wrong{2};
Obj* objects[] = {&machine, &cls, &wrong};
bool missingField = false, missingVerb = false, refuseCall = false;
int calls = 0, scans = 0;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p) { return static_cast<test::Obj*>(p)->id; }
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
bool IsLiveByIndex(void* p, int32_t i) { return i >= 0 && i < 3 && p == test::objects[i] && IsLive(p); }
int32_t SlotSerial(int32_t i) { return i >= 0 && i < 3 ? test::objects[i]->serial : 0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
void* ClassOf(void* p) { return static_cast<test::Obj*>(p)->cls; }
bool IsDescendantOfAny(void* p, void* const* bases, size_t n, int) {
    for (size_t i = 0; i < n; ++i) if (p == bases[i]) return true;
    return false;
}
int32_t FindPropertyOffset(void*, const wchar_t* name) {
    if (test::missingField) return -1;
    if (!wcscmp(name, L"anim")) return offsetof(test::Obj, anim);
    if (!wcscmp(name, L"lookingAt_info")) return offsetof(test::Obj, info);
    if (!wcscmp(name, L"lookingAt_handle")) return offsetof(test::Obj, looking);
    if (!wcscmp(name, L"isActive")) return offsetof(test::Obj, active);
    return -1;
}
void* FindFunction(void*, const wchar_t* name) {
    assert(!wcscmp(name, L"useLever"));
    return test::missingVerb ? nullptr : const_cast<wchar_t*>(name);
}
const FName& NameOf(void*) { static FName name{1, 0}; return name; }
bool NameStartsWith(const FName&, const wchar_t*) { return false; }
int32_t NumObjects() { ++test::scans; return 1; }
void* ObjectAt(int32_t) { return &test::machine; }
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name) { return !wcscmp(name, L"cremator_C") ? &test::cls : nullptr; }
}
namespace coop::element { std::wstring PortableWireKey(void*) { return L"cremator"; } }
namespace ue_wrap::world_identity {
void* CurrentWorld() { return nullptr; }
void* WorldOf(void*) { return nullptr; }
uint32_t Generation() { return 1; }
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn) : fn_(fn), buf_(128) {}
bool Call(void* target, ParamFrame& frame) {
    assert(target == &test::machine && !wcscmp(static_cast<wchar_t*>(frame.function()), L"useLever"));
    ++test::calls;
    return !test::refuseCall;
}
}
int main() {
    using namespace test;
    namespace C = ue_wrap::cremator;
    machine.cls = &cls;
    assert(C::IsCremator(&machine)); // An unloaded door class must not disable the machine.
    assert(C::CanUseLever(&machine));
    machine.anim = true; assert(!C::CanReplayLever(&machine));
    machine.anim = false; machine.info = false; assert(!C::CanUseLever(&machine));
    machine.info = true; machine.looking = false; assert(!C::CanUseLever(&machine));
    machine.looking = true;
    bool active = true;
    assert(C::TryReadLeverActive(&machine, active) && !active);
    machine.active = true; assert(C::TryReadLeverActive(&machine, active) && active);
    missingField = true;
    assert(!C::CanReplayLever(&machine) && !C::TryReadLeverActive(&machine, active) && active);
    missingField = false;
    assert(C::CallUseLever(&machine) && calls == 1);
    missingVerb = true; assert(!C::CallUseLever(&machine) && calls == 1);
    missingVerb = false; refuseCall = true; assert(!C::CallUseLever(&machine) && calls == 2);
    assert(!C::CallUseLever(nullptr) && !C::CallUseLever(&wrong) && calls == 2);
    C::Instances(); const int first = scans; C::Instances(); assert(scans == first);
    puts("PASS active cremator lever: native dispatch/refusal, action guards, missing fields and cached census");
}
