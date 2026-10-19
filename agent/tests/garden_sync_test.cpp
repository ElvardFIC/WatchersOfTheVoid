#define main PreviousPlacementChecks
#include "placed_save_2163_test.cpp"
#undef main

test::Rec Plant(const wchar_t* key = L"native_garden_17") {
    auto r = test::Make(key, L"growingPlant_C");
    r.bools.clear(); r.names = {{}, {L"tomato", L"None"}};
    r.floats = {{}, {.2f, 100.f, 50.f, 0.f, 0.f, 0.f}};
    return r;
}
void PollNow() { for (auto& [key, row] : g_rows) row.pollAt = 0; PollGarden(Now()); }
std::vector<net::BlobChunkPayload> CarePacket(const test::Rec& r,uint32_t seq,P::CareKind kind,const wchar_t* source,float amount) {
    net::Session s;std::vector<uint8_t> bytes;W::AppU8(bytes,Care);W::AppWStr(bytes,r.key);W::AppU32(bytes,seq);
    W::AppU8(bytes,uint8_t(kind));W::AppWStr(bytes,source);W::AppF32(bytes,amount);
    static uint32_t blob=7000;CHECK(blob_chunks::SendBlob(&s,net::ReliableKind::PlacedSave,++blob,bytes));return s.sent;
}
int main() {
    PreviousPlacementChecks();
    net::Session s;
    auto plant = Plant();
    Reset(s, net::Role::Host); s.linked = false;
    auto* a = test::New(plant); ScanPlaced(nullptr, a); Pump();
    CHECK(g_rows.at(plant.key).accepted && a->record.key == plant.key && test::spawned == 0 && s.sent.empty());
    // Water, fertilizer and growth changed by native gameplay while the host is alone.
    a->record.floats[1] = {1.f, -2.f, -3.f, 1.f, 0.f, -1.f};
    PollNow(); CHECK(g_rows.at(plant.key).record.floats[1] == a->record.floats[1]);
    s.linked = true; QueueConnectBroadcastForSlot(1); Pump();
    CHECK(!s.sent.empty() && !g_rows.at(plant.key).dirty);
    s.sent.clear(); PollNow(); Pump(); CHECK(s.sent.empty()); // no unchanged-state stream
    // A failed transport retains the newest state and retries, including host death.
    a->record.floats[1][2] = 120.f; s.sendBudget = 0; PollNow(); Pump();
    CHECK(g_rows.at(plant.key).dirty && s.sent.empty());
    s.sendBudget = -1; Pump(); CHECK(!s.sent.empty() && !g_rows.at(plant.key).dirty);
    a->live = false; Pump(); CHECK(g_rows.at(plant.key).dead);

    // A pre-session save twin is updated and retired without a second native birth.
    Reset(s, net::Role::Client); a = test::New(plant);
    ScanPlaced(nullptr, a); Pump(); CHECK(g_rows.empty());
    auto canonical = plant; canonical.floats[1][0] = 1.f; canonical.floats[1][3] = 1.f;
    Deliver(Packet(State, canonical), 0); Pump();
    CHECK(test::spawned == 0 && test::applied == 1 && a->record.floats[1][0] == 1.f);
    ue_wrap::script_gate::Call call{}; call.object = a;
    CHECK(BeforePlantSimulation(call) == ue_wrap::script_gate::Verdict::Cancel);
    Deliver(Packet(Retired, plant), 0); Pump(); CHECK(!a->live);
    Reset(s, net::Role::Client); a = test::New(plant);
    Deliver(Packet(Retired, plant), 0); Pump(); CHECK(!a->live);

    // A planting peer keeps the initial acknowledgement, then follows host maturity.
    Reset(s, net::Role::Client); a = test::New(plant); Finished(nullptr, nullptr, a); Pump();
    const auto key = g_rows.begin()->first; canonical = a->record;
    call.object = a;
    CHECK(BeforePlantSimulation(call) == ue_wrap::script_gate::Verdict::Run);
    Deliver(Packet(State, canonical), 0); Pump(); CHECK(test::applied == 0);
    canonical.floats[1][0] = 1.f; Deliver(Packet(State, canonical), 0); Pump();
    CHECK(test::applied == 1 && a->record.floats[1][0] == 1.f && g_rows.at(key).accepted);
    CHECK(BeforePlantSimulation(call) == ue_wrap::script_gate::Verdict::Cancel);
    OnDisconnect(); CHECK(BeforePlantSimulation(call) == ue_wrap::script_gate::Verdict::Run);

    // Taps and completed hoses use the same host-authored legacy path.
    for (auto kind : {L"faucet_C", L"groundHose_C"}) {
        auto r = test::Make(L"native_water_5", kind);
        Reset(s, net::Role::Host); a = test::New(r); ScanPlaced(nullptr, a); Pump();
        CHECK(g_rows.at(r.key).accepted);
        Reset(s, net::Role::Client); a = test::New(r); Deliver(Packet(State, r), 0); Pump();
        CHECK(test::spawned == 0 && test::applied == 1);
    }
    // Refreshes never enroll or overwrite a legacy placement. Removing a known
    // garden actor still requires host reach; unknown legacy removals do nothing.
    Reset(s, net::Role::Host); a = test::New(plant); ScanPlaced(nullptr, a); Pump();
    test::reach=false;
    Deliver(Packet(Create, canonical), 1); Deliver(Packet(Remove, plant), 1);
    Deliver(Packet(State, canonical), 1); Pump(); CHECK(a->live && test::applied == 0);
    CHECK(g_rows.at(plant.key).record.floats[1][0] == plant.floats[1][0]);
    test::reach=true;Deliver(Packet(Refresh,plant),1);CHECK(g_rows.at(plant.key).dirty);
    auto unknown=Plant(L"unknown_saved_plant");Deliver(Packet(Refresh,unknown),1);Deliver(Packet(Remove,unknown),1);
    CHECK(g_rows.size()==1);Deliver(Packet(Remove,plant),1);Pump();CHECK(!a->live && g_rows.at(plant.key).dead);
    Reset(s,net::Role::Client);a=test::New(plant);Deliver(Packet(State,plant),0);Pump();
    a->live=false;Pump();CHECK(!g_rows.at(plant.key).accepted && !g_rows.at(plant.key).remove);
    CHECK(!s.sent.empty());Deliver(Packet(State,plant),0);Pump();CHECK(g_rows.at(plant.key).ref.Get());
    // Unknown schemas stay outside legacy identity. Saved specialized plants
    // enroll too; seeing unkeyed vegetation must not call getData or mint a key.
    Reset(s, net::Role::Client); auto foreign = plant; foreign.className = L"prop_C";
    a = test::New(foreign); Deliver(Packet(Retired, foreign), 0); Pump(); CHECK(a->live);
    Deliver(Packet(State, foreign), 0); CHECK(g_rows.at(foreign.key).dead);
    Reset(s, net::Role::Host); foreign.className = L"growingPlant_pine_C";
    a=test::New(foreign); ScanPlaced(nullptr, a); Pump(); CHECK(g_rows.at(foreign.key).accepted);
    Reset(s, net::Role::Host); foreign.key=L"None";
    a=test::New(foreign); ScanPlaced(nullptr, a); Pump(); CHECK(g_rows.empty() && test::captures==0 && a->record.key==L"None");
    Reset(s, net::Role::Client); foreign=Plant();foreign.className=L"growingPlant_stolasHerb_C";
    a=test::New(foreign);call.object=a;
    { Applying applying;CHECK(BeforePlantSimulation(call)==ue_wrap::script_gate::Verdict::Cancel); }

    // Polling is bounded and fair, including captures that cannot be normalized.
    Reset(s, net::Role::Host); s.linked = false;
    for (int i = 0; i < 6; ++i) { auto r = Plant(); r.key += std::to_wstring(i); ScanPlaced(nullptr, test::New(r)); }
    Pump(); CHECK(g_rows.size() == 6); test::captures = 0;
    for (auto& [k, row] : g_rows) row.pollAt = 0;
    PollGarden(Now()); CHECK(test::captures == 2);
    PollGarden(Now()); PollGarden(Now()); CHECK(test::captures == 6);
    test::captures = 0; PollGarden(Now()); CHECK(test::captures == 0);
    // Reentrant travel/disconnect invalidates the captured result before mutation.
    for (auto& [k, row] : g_rows) row.pollAt = 0;
    test::captureCallback = [] { ++test::world; World(); };
    PollGarden(Now()); CHECK(g_rows.empty());
    Reset(s, net::Role::Host); a = test::New(plant); ScanPlaced(nullptr, a); Pump();
    a->record.floats[1][0] = 1.f;
    test::captureCallback = [] { OnDisconnect(); };
    PollNow(); CHECK(g_rows.empty());
    // A guest's native harvest is refused; its ordered request retries without
    // locally creating loot or destroying the crop.
    Reset(s,net::Role::Client);a=test::New(plant);Deliver(Packet(State,plant),0);Pump();
    uint8_t action=4;call.object=a;call.locals=&action;call.function=test::caller;
    CHECK(BeforeAction(call)==ue_wrap::script_gate::Verdict::Cancel && g_uses.size()==1 && test::harvested==0);
    CHECK(BeforeAction(call)==ue_wrap::script_gate::Verdict::Cancel && g_uses.size()==1);
    s.sendBudget=0;Pump();CHECK(g_uses.size()==1);
    s.sendBudget=-1;Pump();CHECK(g_uses.empty() && a->live);
    a->discard=true;CHECK(BeforeAction(call)==ue_wrap::script_gate::Verdict::Run);
    // Host identity/reach/nonce checks precede the single native outcome.
    Reset(s,net::Role::Host);a=test::New(plant);ScanPlaced(nullptr,a);Pump();
    test::reach=false;Deliver(Packet(Harvest,plant),1);CHECK(test::harvested==0);
    test::reach=true;Deliver(Packet(Harvest,plant),1);CHECK(test::harvested==1 && g_rows.at(plant.key).dirty);
    Deliver(Packet(Harvest,plant),1);CHECK(test::harvested==1); // duplicate reliable/request
    Deliver(Packet(Harvest,plant,false,2),1);CHECK(test::harvested==1); // burst refusal
    g_used[1].at=0;Deliver(Packet(Harvest,plant,false,2),1);CHECK(test::harvested==2);
    Deliver(Packet(Harvest,unknown,false,3),1);CHECK(test::harvested==2);
    a->discard=true;g_used[1].at=0;Deliver(Packet(Harvest,plant,false,3),1);CHECK(test::harvested==2);
    a->discard=false;test::harvestCallback=[] { test::actors[1].live=false; };
    Deliver(Packet(Harvest,plant,false,3),1);CHECK(test::harvested==3 && g_rows.at(plant.key).dead);
    Reset(s,net::Role::Host);a=test::New(plant);ScanPlaced(nullptr,a);Pump();
    test::harvestCallback=[] {
        uint8_t result=1;ue_wrap::script_gate::Call pocket{};
        pocket.object=&test::actors[19];pocket.function=test::caller;pocket.locals=&result;
        CHECK(BeforeHarvestPocket(pocket)==ue_wrap::script_gate::Verdict::Cancel && result==0);
        OnDisconnect();result=1;
        test::actors[19].live=false;
        CHECK(BeforeHarvestPocket(pocket)==ue_wrap::script_gate::Verdict::Cancel && result==0);
    };
    Deliver(Packet(Harvest,plant),1);CHECK(g_rows.empty());
    CHECK(!g_harvestCollector.Raw());
    // Owner-paid care is queued only after the actual native source cost.
    Reset(s,net::Role::Client);a=test::New(plant);Deliver(Packet(State,plant),0);Pump();
    auto* bucket=test::New(test::Make(L"bucket_key",L"prop_bucket_C"));test::hand=bucket;
    test::careReadable=true;test::careUse={a,L"bucket_key",300.f,P::CareKind::Water};
    void* player=&test::actors[19];call.object=bucket;call.function=test::caller;call.locals=reinterpret_cast<uint8_t*>(&player);
    CHECK(BeforeCare(call)==ue_wrap::script_gate::Verdict::Run&&g_careFrames.size()==1);
    AfterCare(call);CHECK(g_careFrames.empty()&&g_uses.empty()); // native refusal did not empty bucket
    BeforeCare(call);test::careSpent=true;AfterCare(call);CHECK(g_uses.size()==1&&g_uses.front().op==Care&&g_uses.front().care.amount==300.f);
    s.sendBudget=0;g_uses.front().expires=1;Pump();CHECK(g_uses.size()==1);s.sendBudget=-1;Pump();CHECK(g_uses.empty());
    auto* fertilizer=test::New(test::Make(L"bag_key",L"prop_fertilizer_C"));test::hand=fertilizer;call.object=fertilizer;
    test::careUse={a,L"bag_key",600.f,P::CareKind::Fertilizer};BeforeCare(call);AfterCare(call);CHECK(g_uses.empty());
    BeforeCare(call);fertilizer->live=false;AfterCare(call);CHECK(g_uses.size()==1&&g_uses.front().care.sourceKey==L"bag_key");
    fertilizer->live=true;BeforeCare(call);CHECK(!g_careFrames.empty());Pump();CHECK(g_careFrames.empty()); // another gate cancelled; no post
    // Reserving a full queue refuses before native stock can be consumed.
    fertilizer->live=true;g_uses.resize(32);CHECK(BeforeCare(call)==ue_wrap::script_gate::Verdict::Cancel);
    // Host modifies only the tracked plant; duplicates, stale sequences, foreign
    // tools, malformed amounts and spent fertilizer identities cannot refill it.
    Reset(s,net::Role::Host);a=test::New(plant);ScanPlaced(nullptr,a);Pump();
    Deliver(CarePacket(plant,1,P::CareKind::Water,L"bucket_key",300),1);CHECK(a->record.floats[1][2]==300);
    a->record.floats[1][2]=200;Deliver(CarePacket(plant,1,P::CareKind::Water,L"bucket_key",300),1);CHECK(a->record.floats[1][2]==200);
    test::reach=false;Deliver(CarePacket(plant,2,P::CareKind::Water,L"bucket_key",400),1);CHECK(a->record.floats[1][2]==200);
    test::reach=true;test::lastHeld=L"prop_fertilizer_C";
    Deliver(CarePacket(plant,2,P::CareKind::Water,L"bucket_key",400),1);CHECK(a->record.floats[1][2]==200);
    a->record.floats[1][1]=-1;Deliver(CarePacket(plant,2,P::CareKind::Fertilizer,L"bag_key",600),1);CHECK(a->record.floats[1][1]==600);
    a->record.floats[1][1]=0;Deliver(CarePacket(plant,3,P::CareKind::Fertilizer,L"bag_key",600),1);CHECK(a->record.floats[1][1]==0);
    Deliver(CarePacket(plant,3,P::CareKind::Fertilizer,L"bag_other",300),1);CHECK(a->record.floats[1][1]==0);
    Deliver(CarePacket(plant,3,P::CareKind::Fertilizer,L"bag_other",600),1);CHECK(a->record.floats[1][1]==600);
    test::lastHeld=L"prop_bucket_C";
    Deliver(CarePacket(plant,4,P::CareKind::Water,L"bucket_key",601),1);CHECK(a->record.floats[1][2]==200);
    Deliver(CarePacket(unknown,4,P::CareKind::Water,L"bucket_key",300),1);CHECK(g_rows.size()==1);
    // Slot departure drops the sequence watermark; consumed fertilizer history
    // remains scoped to its player number until world reset.
    OnPeerLeftSlot(1);Deliver(CarePacket(plant,1,P::CareKind::Water,L"bucket_key",100),1);CHECK(a->record.floats[1][2]==100);
    puts("PASS client care: native-cost proof, reserved retry queue, no-use refusal, dead fertilizer, duplicate/source/amount/reach/tool guards, canonical host state");
    puts("PASS garden sync: saved placements, canonical growth/water/fertilizer/ripeness/death, creator updates, backpressure, fair capture budget, lifetime guards");
}
