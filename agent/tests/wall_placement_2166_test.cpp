#define main Placed2163Checks
#include "placed_save_2163_test.cpp"
#undef main

int main() {
    Placed2163Checks();
    net::Session s;
    auto outline=test::Make(L"mvpl_12345678_00000001",L"customWall_unfinished_C");
    outline.ints={{12,0,0,-1}};outline.strings={{L"brick"}};
    Reset(s,net::Role::Client);
    auto* a=test::New(outline);Finished(nullptr,nullptr,a);Pump();
    const auto key=g_rows.begin()->first;
    auto host=g_rows.at(key).record;host.ints[0][1]=5;
    Deliver(Packet(State,host),0);Pump();
    CHECK(test::spawned==0&&test::applied==1&&a->record.ints[0][1]==5);
    host.ints[0][1]=8;Deliver(Packet(State,host),0);Pump();
    CHECK(a->record.ints[0][1]==8&&test::spawned==0);
    // Successor plus retirement, in either order, never resurrect the outline.
    auto solid=host;solid.key=L"mvpl_12345678_00000002";solid.className=L"customWall_C";
    solid.ints={{0,-1}};solid.floats={{100,100}};
    Deliver(Packet(State,solid),0);Deliver(Packet(Retired,host),0);Pump();
    CHECK(!a->live&&g_rows.at(key).dead&&test::spawned==1);
    Deliver(Packet(State,host),0);Pump();CHECK(test::spawned==1&&!a->live);
    Reset(s,net::Role::Host);Deliver(Packet(Create,outline),1);Pump();
    a=static_cast<test::Actor*>(g_rows.at(outline.key).ref.Get());
    CHECK(OwnsClientWall(outline.key,a,1,test::playerNo));
    CHECK(!OwnsClientWall(outline.key,a,2,test::playerNo));CHECK(!OwnsClientWall(outline.key,a,1,test::playerNo+1));
    g_rows.at(outline.key).spawnedForClient=false;CHECK(!OwnsClientWall(outline.key,a,1,test::playerNo));g_rows.at(outline.key).spawnedForClient=true;
    a->record.ints[0][1]=7;MarkChanged(a);s.sendBudget=0;Pump();
    CHECK(g_rows.at(outline.key).dirty&&g_rows.at(outline.key).record.ints[0][1]==7);
    s.sendBudget=-1;Pump();CHECK(!g_rows.at(outline.key).dirty);
    Deliver(Packet(Create,outline),1);Pump();CHECK(a->record.ints[0][1]==7&&test::spawned==1);
    QueueConnectBroadcastForSlot(2);Pump();CHECK(g_rows.at(outline.key).record.ints[0][1]==7);
    a->live=false;Pump();CHECK(g_rows.at(outline.key).dead);
    Reset(s,net::Role::Host);a=test::New(outline);Deliver(Packet(Create,outline),1);Pump();
    CHECK(!OwnsClientWall(outline.key,a,1,test::playerNo)); // a colliding existing host actor was never created for this client
    puts("PASS wall placement: author receives progress, conversion/tombstone, late join, stale create and send retry");
}
