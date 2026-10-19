#define CANNED_PROFILE_TEST
#define main PriorProfileTest
#include "profile_receive_2169_test.cpp"
#undef main
namespace can_profile { bool ready = true; int foods = 0; }
namespace ue_wrap::canned_food {
bool RefreshHeldRecord(void*,save_record::SaveRecord& record) {
    if (!can_profile::ready) return false;
    record.ints = {{2,can_profile::foods}}; record.bools = {{1,0,1,0},{1}};
    return true;
}
}
int main() {
    using namespace test;
    Reset(); session.mode = coop::net::Role::Client; P::g_profileApplied = true;
    localItems.hold.resize(1);
    localItems.hold[0].data.className = L"prop_cannedFood_C";
    localItems.hold[0].data.key = L"can1";
    localItems.hold[0].data.ints = {{2,5}};
    localItems.hold[0].data.bools = {{1,0,1,0},{0}};
    auto poll = [] { P::g_lastPoll = {}; P::ClientStreamTick(&session); };
    auto receive = [] {
        coop::blob_chunks::Assembler assembler; std::vector<uint8_t> blob;
        for (const auto& part : session.sent) assembler.OnChunk(part,0,blob);
        coop::player_profile::Profile out;
        CHECK(coop::inventory_wire::Deserialize(blob,out)); return out.items.hold[0].data;
    };
    can_profile::ready = false; poll(); CHECK(session.sent.empty());
    can_profile::ready = true; poll(); CHECK(!session.sent.empty());
    auto saved = receive(); CHECK(saved.ints[0][1] == 0 && saved.bools[1][0] == 1);
    CHECK(localItems.hold[0].data.ints[0][1] == 5); // copied snapshot only
    session.sent.clear(); poll(); CHECK(session.sent.empty()); // hash includes refreshed state
    can_profile::foods = 1; session.sendOk = false; poll(); CHECK(session.sent.empty());
    session.sendOk = true; poll(); CHECK(!session.sent.empty());
    CHECK(receive().ints[0][1] == 1);
    std::puts("PASS real profile stream refreshes can state before hash, defers failed read and retries refused send");
}
