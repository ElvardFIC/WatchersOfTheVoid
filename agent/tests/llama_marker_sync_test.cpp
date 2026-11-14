#define main PreviousPlacedChecks
#include "placed_save_2163_test.cpp"
#undef main
namespace marker_test {
test::Rec Record(const wchar_t* key=L"mvpl_12345678_00000001"){test::Rec r;r.className=L"llamaSoul_C";r.key=key;return r;}
std::vector<net::BlobChunkPayload> Verb(Op op,const test::Rec& record){
    std::vector<uint8_t> bytes;W::AppU8(bytes,op);W::AppWStr(bytes,record.key);
    if(op==Harvest||op==Care||op==HoseRoute)W::AppU32(bytes,1);
    if(op==Harvest||op==Care)W::AppU32(bytes,test::playerNo);
    if(op==Care){W::AppU8(bytes,0);W::AppWStr(bytes,L"source_bucket");W::AppF32(bytes,10);}
    if(op==HoseRoute){W::AppU32(bytes,0);W::AppU32(bytes,0);}
    net::Session transport;static uint32_t seq=5000;CHECK(blob_chunks::SendBlob(&transport,net::ReliableKind::PlacedSave,++seq,bytes));return transport.sent;
}
void Group(int id){net::Session session;auto record=Record();switch(id){
    case 20:{Reset(session,net::Role::Client);auto* existing=test::New(record);Deliver(Packet(State,record),0);Pump();
        CHECK(test::spawned==0&&test::applied==1&&g_rows.at(record.key).ref.Get()==existing);
        Deliver(Packet(State,record),0);Pump();CHECK(test::spawned==0&&g_rows.at(record.key).ref.Get()==existing);break;}
    case 21:Reset(session,net::Role::Host);Deliver(Packet(Create,record),1);Pump();CHECK(g_rows.empty()&&!test::spawned);
        Reset(session,net::Role::Client);{auto* local=test::New(record);Finished(nullptr,nullptr,local);CHECK(g_rows.empty());}break;
    case 22:{Reset(session,net::Role::Host);auto* actor=test::New(record);ScanPlaced(nullptr,actor);EnrollPlacedActors();CHECK(g_rows.at(record.key).accepted);
        Deliver(Packet(Remove,record),1);Pump();CHECK(actor->live&&!g_rows.at(record.key).dead&&!test::destroyed);break;}
    case 23:{Reset(session,net::Role::Host);auto* actor=test::New(record);ScanPlaced(nullptr,actor);EnrollPlacedActors();
        for(Op op:{Harvest,Care,HoseRoute,Retired,State}){if(op==State)Deliver(Packet(op,record),1);else Deliver(Verb(op,record),1);}
        Pump();CHECK(actor->live&&!test::destroyed&&!test::harvested&&g_rows.at(record.key).accepted);break;}
    case 24:{Reset(session,net::Role::Host);CHECK(g_rows.empty());auto* actor=test::New(record);ScanPlaced(nullptr,actor);EnrollPlacedActors();CHECK(g_rows.size()==1&&g_rows.at(record.key).ref.Get()==actor);
        ScanPlaced(nullptr,actor);EnrollPlacedActors();CHECK(g_rows.size()==1);QueueConnectBroadcastForSlot(2);Pump();CHECK(!session.sent.empty()&&actor->live);break;}
    case 25:{Reset(session,net::Role::Client);auto* actor=test::New(record);Deliver(Packet(State,record),0);Pump();OnDisconnect();CHECK(actor->live&&!test::destroyed&&g_rows.empty());
        Install(&session);Deliver(Packet(State,record),0);Pump();CHECK(!test::spawned&&g_rows.at(record.key).ref.Get()==actor);break;}
    default:CHECK(false);}}
}
int main(){for(int id=20;id<=25;++id){marker_test::Group(id);std::printf("PASS llama lifecycle %d\n",id);}return 0;}
