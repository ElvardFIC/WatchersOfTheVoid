#define main PreviousPlacedChecks
#include "placed_save_2163_test.cpp"
#undef main
int main(){CHECK(PreviousPlacedChecks()==0);net::Session s;
 auto wet=test::Make(L"saved_wet_wall",L"customWall_wetConcrete_C");wet.floats={{120,200,0,360}};
 wet.ints={{1,-1}};wet.strings={{L"concrete_0"}};
 Reset(s,net::Role::Host);s.linked=false;auto* a=test::New(wet);ScanPlaced(nullptr,a);Pump();
 CHECK(g_rows.at(wet.key).accepted&&test::spawned==0);
 a->record.floats[0][2]=72;g_rows.at(wet.key).pollAt=0;PollGarden(Now());
 CHECK(g_rows.at(wet.key).record.floats[0][2]==72&&g_rows.at(wet.key).dirty);
 s.linked=true;s.sendBudget=0;Pump();CHECK(g_rows.at(wet.key).dirty);
 s.sendBudget=-1;QueueConnectBroadcastForSlot(1);Pump();CHECK(!s.sent.empty());
 const auto snapshot=s.sent;
 Reset(s,net::Role::Client);a=test::New(wet);
 Deliver(snapshot,0);Pump();
 CHECK(test::spawned==0&&test::applied==1&&a->record.floats[0][2]==72);
 Deliver(Packet(Retired,wet),0);Pump();CHECK(!a->live);
 auto solid=wet;solid.className=L"customWall_C";solid.key=L"finished_wall";solid.floats={{120,200}};
 Deliver(Packet(State,solid),0);Pump();Pump();CHECK(test::spawned==1);
 puts("PASS wet-wall wire: pre-session host enrollment, solo elapsed poll, retained send refusal, chunked join/adoption and canonical replacement retirement");
}
