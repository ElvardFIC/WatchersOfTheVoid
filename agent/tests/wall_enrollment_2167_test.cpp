#define main PreviousPlacementChecks
#include "placed_save_2163_test.cpp"
#undef main

int main() {
    PreviousPlacementChecks();
    net::Session s;
    auto wall = test::Make(L"native_wall_17", L"customWall_C");
    wall.ints={{0,-1}}; wall.strings={{L"brick"}}; wall.floats={{70,100}};
    Reset(s,net::Role::Host); s.linked=false;
    auto* a=test::New(wall);
    ScanWall(nullptr,a); ScanWall(nullptr,a);
    CHECK(g_enrollWalls.size()==1);
    Pump();
    CHECK(g_rows.at(wall.key).accepted && g_rows.at(wall.key).dirty);
    CHECK(a->record.key==wall.key && s.sent.empty() && test::spawned==0);
    // Host changes and deaths while alone must survive the first connection.
    a->record.floats[0][0]=50; MarkChanged(a); Pump();
    s.linked=true; QueueConnectBroadcastForSlot(1); Pump();
    CHECK(g_rows.at(wall.key).record.floats[0][0]==50 && !s.sent.empty());
    s.linked=false; a->live=false; Pump();
    CHECK(g_rows.at(wall.key).dead && g_rows.at(wall.key).dirty);
    s.linked=true; Pump(); CHECK(!g_rows.at(wall.key).dirty);

    Reset(s,net::Role::Client); a=test::New(wall);
    ScanWall(nullptr,a); Pump(); CHECK(g_rows.empty() && s.sent.empty());
    Deliver(Packet(State,wall),0); Pump();
    CHECK(test::spawned==0 && test::applied==1 && g_rows.at(wall.key).ref.Get()==a);
    Deliver(Packet(Retired,wall),0); Pump(); CHECK(!a->live);
    Deliver(Packet(State,wall),0); Pump(); CHECK(test::spawned==0);
    // A retired legacy wall in the older transferred save is removed too.
    Reset(s,net::Role::Client); a=test::New(wall);
    Deliver(Packet(Retired,wall),0); Pump(); CHECK(!a->live);
    // Never let a bare legacy key remove a different actor family.
    Reset(s,net::Role::Client); auto tap=wall; tap.className=L"faucet_C";
    a=test::New(tap); Deliver(Packet(Retired,wall),0); Pump(); CHECK(a->live);
    Reset(s,net::Role::Client); Deliver(Packet(State,tap),0); Pump(); CHECK(g_rows.empty());
    Reset(s,net::Role::Host); a=test::New(wall);
    Deliver(Packet(Create,wall),1); Deliver(Packet(Remove,wall),1); Pump();
    CHECK(g_rows.empty() && a->live);

    // Registry/initialization failures are retried with bounded work, no spawn.
    Reset(s,net::Role::Host); a=test::New(wall); test::registry=false;
    ScanWall(nullptr,a); Pump(); CHECK(g_rows.empty() && g_enrollWalls.size()==1);
    test::registry=true; test::ready=false; Pump(); CHECK(g_rows.empty());
    test::ready=true; Pump(); CHECK(g_rows.at(wall.key).ref.Get()==a);
    ScanWall(nullptr,a); Pump(); CHECK(g_rows.size()==1 && test::spawned==0);
    Reset(s,net::Role::Host); auto keyless=wall; keyless.key=L"None";
    a=test::New(keyless); ScanWall(nullptr,a); Pump();
    CHECK(g_rows.empty() && a->record.key==L"None");
    ue_wrap::placed_save::SetKey(a,wall.key); Pump(); CHECK(g_rows.size()==1);
    // Native key collision: keep both actors untouched, never hijack the map.
    Reset(s,net::Role::Host); a=test::New(wall); auto* other=test::New(wall);
    ScanWall(nullptr,a); Pump(); CHECK(g_rows.empty() && test::keys[wall.key]==other);
    // A slot reused before the pump cannot enroll its new tenant by old evidence.
    Reset(s,net::Role::Host); a=test::New(wall); ScanWall(nullptr,a); ++a->serial;
    Pump(); CHECK(g_rows.empty()); ScanWall(nullptr,a); Pump(); CHECK(g_rows.size()==1);
    // Four native captures maximum per tick, with a fair queue.
    Reset(s,net::Role::Host);
    for(int i=0;i<7;++i){auto r=wall;r.key+=std::to_wstring(i);ScanWall(nullptr,test::New(r));}
    Pump(1); CHECK(g_rows.size()==4 && g_enrollWalls.size()==3);
    Pump(1); CHECK(g_rows.size()==7 && g_enrollWalls.empty());
    Reset(s,net::Role::Host); a=test::New(wall); ScanWall(nullptr,a);
    ++test::world; World(); Pump(); CHECK(g_rows.empty() && g_enrollWalls.empty());
    // Destruction before the census still leaves a retirement for the old save.
    Reset(s,net::Role::Host); a=test::New(wall); TrackWall(a);
    CHECK(g_rows.at(wall.key).ref.Get()==a);
    a->live=false; Pump(); CHECK(g_rows.at(wall.key).dead);
    Reset(s,net::Role::Host); a=test::New(wall); ScanWall(nullptr,a);
    test::captureCallback=[] { ++test::world; World(); };
    Pump(); CHECK(g_rows.empty() && g_enrollWalls.empty());
    Reset(s,net::Role::Host); a=test::New(wall); ScanWall(nullptr,a);
    test::captureCallback=[] { test::actors[1].live=false; };
    Pump(); CHECK(g_rows.empty());
    // Native placements made by a solo host are tracked before anyone joins.
    Reset(s,net::Role::Host); s.linked=false; a=test::New(wall);
    Finished(nullptr,nullptr,a); Pump(); CHECK(g_rows.size()==1 && s.sent.empty());
    s.linked=true; Pump(); CHECK(!s.sent.empty());
    puts("PASS wall enrollment: native identity, solo host, save twin, tombstones, role/class guards, retry, budget, slot/world lifetime");
}
