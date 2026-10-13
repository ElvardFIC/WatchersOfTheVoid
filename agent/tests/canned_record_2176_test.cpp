#define main PriorCanWrapperTest
#define CANNED_RECORD_TEST
#include "canned_wrapper_2175_test.cpp"
#undef main
namespace record_test {
std::wstring cls = L"prop_cannedFood_C", key = L"can1";
}
namespace ue_wrap::reflection {std::wstring ClassNameOf(void*) { return record_test::cls; }}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*) { return record_test::key; }}
int main() {
    using namespace test;
    namespace C = ue_wrap::canned_food;
    ue_wrap::save_record::SaveRecord base;
    base.className = record_test::cls; base.key = record_test::key;
    base.ints = {{2,5,99},{88}}; base.bools = {{1,0,1,0},{0,1},{1}};
    base.names = {{L"closed",L"nametag"}}; base.floats = {{12}};
    objects[1].flags = 0xA4; objects[1].type = 3; objects[1].foods = 0;
    auto rec = base;
    CHECK(C::RefreshHeldRecord(&objects[1],rec));
    CHECK(rec.ints[0][0] == 3 && rec.ints[0][1] == 0 && rec.bools[1][0] == 1);
    CHECK(rec.ints[0][2] == 99 && rec.ints[1] == base.ints[1]);
    CHECK(rec.bools[0] == base.bools[0] && rec.bools[1][1] == 1 && rec.bools[2] == base.bools[2]);
    CHECK(rec.key == base.key && rec.className == base.className && rec.names == base.names && rec.floats == base.floats);
    CHECK(refreshes == 0 && objects[1].flags == 0xA4 && objects[1].foods == 0);
    for (int i = 0; i < 4; ++i) {
        record_test::cls = L"prop_cannedFood_" + std::to_wstring(i) + L"_C";
        rec = base; rec.className = record_test::cls; rec.ints.clear(); rec.bools.resize(1);
        CHECK(C::RefreshHeldRecord(&objects[1],rec) && rec.ints[0][1] == 0 && rec.bools[1][0] == 1);
    }
    record_test::cls = base.className;
    for (int reason = 0; reason < 7; ++reason) {
        rec = base;
        if (reason == 0) record_test::key = L"different";
        if (reason == 1) record_test::cls = L"prop_cannedFood_0_C";
        if (reason == 2) objects[1].live = false;
        if (reason == 3) objects[1].foods = -1;
        if (reason == 4) rec.bools.clear();
        if (reason == 5) classLoaded = false;
        if (reason == 6) rec.key.clear();
        const auto before = rec;
        CHECK(!C::RefreshHeldRecord(&objects[1],rec));
        CHECK(rec.ints == before.ints && rec.bools == before.bools && rec.names == before.names);
        record_test::key = base.key; record_test::cls = base.className;
        objects[1].live = true; objects[1].foods = 0; classLoaded = true;
    }
    CHECK(!C::RefreshHeldRecord(nullptr,rec));
    rec.className = L"unrelated_C"; CHECK(C::RefreshHeldRecord(nullptr,rec));
    std::puts("PASS held can record: zero portions/open flag, exact identity, preserved base state, no live mutation");
}
