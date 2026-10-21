#define GARDEN_SYNC_FIXTURE_ONLY
#include "garden_sync_test.cpp"
namespace owner_test {
net::Session session;test::Actor* plant=nullptr;
void Setup(){Reset(session,net::Role::Host);test::playerNo=11;plant=test::New(Plant());ScanPlaced(nullptr,plant);Pump();session.sent.clear();}
std::vector<net::BlobChunkPayload> Wire(Op op,uint32_t person,bool include=true,uint32_t sequence=1){
    std::vector<uint8_t> bytes;W::AppU8(bytes,op);W::AppWStr(bytes,plant->record.key);W::AppU32(bytes,sequence);
    if(include)W::AppU32(bytes,person);
    if(op==Care){W::AppU8(bytes,uint8_t(P::CareKind::Water));W::AppWStr(bytes,L"bucket_key");W::AppF32(bytes,300.f);}
    net::Session temporary;static uint32_t blob=90000;CHECK(blob_chunks::SendBlob(&temporary,net::ReliableKind::PlacedSave,++blob,bytes));return temporary.sent;
}
}
int main(){using namespace owner_test;
    for(Op op:{Care,Harvest}){
        Setup();auto old=Wire(op,11);++test::playerNo;OnPeerLeftSlot(1);Deliver(old,1);
        CHECK(!test::harvested && plant->record.floats[1][2]==50 && !g_used[1].seq);
        Deliver(Wire(op,12),1);
        CHECK(g_used[1].playerNo==12 && g_used[1].seq==1);
        if(op==Care)CHECK(plant->record.floats[1][2]==300);else CHECK(test::harvested==1);
        if(op==Care)plant->record.floats[1][2]=100;
        Deliver(old,1);CHECK(g_used[1].playerNo==12);if(op==Care)CHECK(plant->record.floats[1][2]==100);else CHECK(test::harvested==1);
        // Malformed identities and the old layout cannot reach native gameplay.
        for(uint32_t bad:{0u,12u,65536u}){Setup();Deliver(Wire(op,bad),1);CHECK(!g_used[1].seq && !test::harvested && plant->record.floats[1][2]==50);}
        Setup();Deliver(Wire(op,11,false),1);CHECK(!g_used[1].seq && !test::harvested && plant->record.floats[1][2]==50);
        // Native authority probes may reenter without changing the player number.
        for(int cause=0;cause<6;++cause){Setup();test::gardenReach=[cause]{if(cause==0)OnPeerLeftSlot(1);if(cause==1)++test::world;
            if(cause==2)++test::playerNo;if(cause==3)OnDisconnect();if(cause==4)session.mode=net::Role::Client;if(cause==5)session.worldReady=false;};
            Deliver(Wire(op,11),1);CHECK(!test::harvested && plant->record.floats[1][2]==50);}
        Setup();session.worldReady=false;Deliver(Wire(op,11),1);CHECK(!test::harvested && plant->record.floats[1][2]==50);
    }
    // The sender retains the captured person in real chunk framing.
    Reset(session,net::Role::Client);auto record=Plant();plant=test::New(record);Deliver(Packet(State,record),0);Pump();session.sent.clear();
    uint8_t action=4;SG::Call call{};call.object=plant;call.function=test::caller;call.locals=&action;
    CHECK(BeforeAction(call)==SG::Verdict::Cancel && g_uses.size()==1);const auto person=g_uses.front().person;
    FlushUses(Now());CHECK(!session.sent.empty());blob_chunks::Assembler assembler;std::vector<uint8_t> bytes;
    for(const auto& part:session.sent)assembler.OnChunk(part,1,bytes);
    size_t off=0;uint8_t op=0;std::wstring key;uint32_t sequence=0,encoded=0;
    CHECK(W::RdU8(bytes,off,op) && op==Harvest && W::RdWStr(bytes,off,key) && W::RdU32(bytes,off,sequence) && W::RdU32(bytes,off,encoded));
    CHECK(encoded==person && off==bytes.size());
    puts("PASS garden wire owner: captured sender identity, stale slot packets, malformed/legacy framing, same-person connection reentry, world/role/readiness and real encoded request");return 0;
}
