#define main PreviousPlacedRecordChecks
#include "placed_record_2163_test.cpp"
#undef main
int main() {
    PreviousPlacedRecordChecks();
    namespace P = ue_wrap::placed_save;
    ue_wrap::save_record::SaveRecord r, out;
    r.className = L"growingPlant_C"; r.key = L"native_plant_12";
    r.names = {{}, {L"deadbush", L"None"}};
    r.floats = {{}, {1.f, -2.f, -3.f, 0.f, 0.f, -1.f}};
    CHECK(P::Normalize(r, out) && out.floats == r.floats);
    r.floats[1][5] = -1.01f; CHECK(!P::Normalize(r, out)); r.floats[1][5] = -1.f;
    for (int index : {0, 3, 4}) {
        const auto old = r.floats[1][index]; r.floats[1][index] = -.01f;
        CHECK(!P::Normalize(r, out)); r.floats[1][index] = old;
    }
    for (int index = 0; index < 6; ++index) {
        const auto old = r.floats[1][index];
        r.floats[1][index] = std::numeric_limits<float>::quiet_NaN(); CHECK(!P::Normalize(r, out));
        r.floats[1][index] = std::numeric_limits<float>::infinity(); CHECK(!P::Normalize(r, out));
        r.floats[1][index] = 1'000'001.f; CHECK(!P::Normalize(r, out)); r.floats[1][index] = old;
    }
    puts("PASS native plant state: dead sentinel, water/fertilizer depletion across zero, malformed growth/lifetime, finite bounds");
}
