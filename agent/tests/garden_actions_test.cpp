#define main PreviousPlacementChecks
#include "placed_save_2163_test.cpp"
#undef main
namespace G=coop::garden_actions;
std::vector<net::BlobChunkPayload> GardenPacket(uint8_t kind,uint32_t seq,const test::Rec& record,const std::wstring& endpoint={}) {
    std::vector<uint8_t> b;W::AppU8(b,9);W::AppU8(b,kind);W::AppU32(b,seq);W::AppU32(b,test::playerNo);W::AppWStr(b,record.key);W::AppWStr(b,endpoint);
    net::Session s;static uint32_t blobSeq=40000;CHECK(blob_chunks::SendBlob(&s,net::ReliableKind::PlacedSave,++blobSeq,b));return s.sent;
}
int main(){
    net::Session s;auto tap=test::Make(L"saved_tap");auto hose=test::Make(L"saved_hose",L"groundHose_C");
    hose.names={{tap.key,L"saved_sprinkler"}};hose.vectors={{{0,0,0},{8000,0,0}}};
    Reset(s,net::Role::Host);auto* faucet=test::New(tap);
    ue_wrap::CachedObjRef ref;ref.Set(faucet);CHECK(EnrollPlaced(ref));
    Deliver(GardenPacket(1,1,tap),1);CHECK(test::toggles==1&&!faucet->record.bools[0][0]);
    Deliver(GardenPacket(1,1,tap),1);CHECK(test::toggles==1);
    Deliver(GardenPacket(1,2,tap),1);CHECK(test::toggles==2&&faucet->record.bools[0][0]);
    Deliver(GardenPacket(1,3,tap),0);CHECK(test::toggles==2);
    test::reach=false;Deliver(GardenPacket(1,3,tap),1);CHECK(test::toggles==2);
    test::reach=true;Deliver(GardenPacket(1,3,tap),1);CHECK(test::toggles==2); // terminal rejected sequence
    auto* a=test::New(hose);ref.Set(a);CHECK(EnrollPlaced(ref));
    Deliver(GardenPacket(2,4,hose,L"other_sprinkler"),1);CHECK(test::detaches==0);
    Deliver(GardenPacket(2,5,hose,L"saved_sprinkler"),1);CHECK(test::detaches==1&&a->record.names[0][1]==L"None");
    Deliver(GardenPacket(2,5,hose,L"saved_sprinkler"),1);CHECK(test::detaches==1);
    Deliver(GardenPacket(2,6,tap,L"saved_sprinkler"),1);CHECK(test::detaches==1);
    G::OnPeerLeftSlot(1);Deliver(GardenPacket(1,1,tap),1);CHECK(test::toggles==3);
    Reset(s,net::Role::Client);faucet=test::New(tap);
    test::paramPlayerOff=8;uint8_t arguments[16]{5};void* local=&test::actors[19];std::memcpy(arguments+8,&local,8);
    ue_wrap::script_gate::Call faucetCall{};faucetCall.object=faucet;faucetCall.function=test::caller;faucetCall.locals=arguments;
    CHECK(G::FaucetAction(faucetCall)==ue_wrap::script_gate::Verdict::Cancel&&G::g_pending.size()==1);
    faucetCall.fromOurCode=true;CHECK(G::FaucetAction(faucetCall)==ue_wrap::script_gate::Verdict::Run&&G::g_pending.size()==1);
    faucetCall.fromOurCode=false;CHECK(G::FaucetAction(faucetCall)==ue_wrap::script_gate::Verdict::Cancel&&G::g_pending.size()==2);
    s.sendBudget=0;G::Tick();CHECK(G::g_pending.size()==2&&s.sent.empty());
    s.sendBudget=-1;G::Tick();CHECK(G::g_pending.empty()&&s.sent.size()==2);
    const auto clicks=s.sent;
    Reset(s,net::Role::Host);faucet=test::New(tap);Deliver(clicks,1);CHECK(test::toggles==2&&faucet->record.bools[0][0]);
    Reset(s,net::Role::Client);a=test::New(hose);
    ue_wrap::script_gate::Call call{};call.object=a;
    CHECK(G::HoseDetach(call)==ue_wrap::script_gate::Verdict::Cancel&&G::g_pending.size()==1&&test::detaches==0);
    CHECK(G::HoseDetach(call)==ue_wrap::script_gate::Verdict::Cancel&&G::g_pending.size()==1);
    a->record.names[0][1]=L"new_sprinkler";G::Tick();const auto detach=s.sent;
    Reset(s,net::Role::Host);a=test::New(hose);a->record.names[0][1]=L"new_sprinkler";
    Deliver(detach,1);CHECK(test::detaches==0); // old grab cannot detach the new owner
    Reset(s,net::Role::Host);a=test::New(hose);call.object=a;
    CHECK(G::HoseDetach(call)==ue_wrap::script_gate::Verdict::Cancel&&test::detaches==1);
    Reset(s,net::Role::Client);faucet=test::New(tap);CHECK(G::Enqueue(faucet,G::kToggle));
    ++test::world;G::Tick();CHECK(G::g_pending.empty());
    CHECK(G::Enqueue(faucet,G::kToggle));G::OnDisconnect();CHECK(G::g_pending.empty());
    // The request can precede native placement or its reflected component readiness.
    Reset(s,net::Role::Host);Deliver(GardenPacket(1,1,tap),1);CHECK(!test::toggles && G::g_host.size()==1);
    faucet=test::New(tap);test::ready=false;G::Tick();CHECK(!test::toggles && G::g_host.size()==1);
    Deliver(GardenPacket(1,2,tap),1);CHECK(G::g_host.size()==2);
    auto other=test::Make(L"another_tap");auto* second=test::New(other);test::ready=true;test::notReady=faucet;
    Deliver(GardenPacket(1,3,other),1);CHECK(test::toggles==1 && !second->record.bools[0][0] && G::g_host.size()==2);
    test::notReady=nullptr;G::Tick();CHECK(test::toggles==3 && faucet->record.bools[0][0] && G::g_host.empty());
    Deliver(GardenPacket(1,2,tap),1);CHECK(test::toggles==3);
    Reset(s,net::Role::Host);faucet=test::New(tap);test::ready=false;Deliver(GardenPacket(1,1,tap),1);
    test::ready=true;test::gardenReadyAnswers={false,true};Deliver(GardenPacket(1,2,tap),1);
    CHECK(!test::toggles && G::g_host.size()==2);G::Tick();CHECK(test::toggles==2);
    Reset(s,net::Role::Host);faucet=test::New(tap);test::ready=false;Deliver(GardenPacket(1,1,tap),1);
    Deliver(GardenPacket(1,2,tap),1);
    ++faucet->serial;test::ready=true;G::Tick();CHECK(!test::toggles && G::g_host.empty());
    Reset(s,net::Role::Host);faucet=test::New(tap);test::ready=false;Deliver(GardenPacket(1,1,tap),1);
    faucet->record.key=L"different_key";test::ready=true;G::Tick();CHECK(!test::toggles && G::g_host.empty());
    Reset(s,net::Role::Host);faucet=test::New(tap);test::ready=false;Deliver(GardenPacket(1,1,tap),1);
    ++test::playerNo;test::ready=true;G::Tick();CHECK(!test::toggles && G::g_host.empty());
    Reset(s,net::Role::Host);auto stale=GardenPacket(1,1,tap);++test::playerNo;faucet=test::New(tap);
    Deliver(stale,1);CHECK(!test::toggles && G::g_host.empty());
    Reset(s,net::Role::Host);Deliver(GardenPacket(1,1,tap),1);G::g_host.front().expires=0;faucet=test::New(tap);G::Tick();CHECK(!test::toggles);
    Reset(s,net::Role::Host);faucet=test::New(tap);s.worldReady=false;Deliver(GardenPacket(1,1,tap),1);CHECK(!test::toggles && G::g_host.empty());
    Reset(s,net::Role::Host);faucet=test::New(tap);test::ready=false;Deliver(GardenPacket(1,1,tap),1);
    G::OnPeerLeftSlot(1);test::ready=true;G::Tick();CHECK(!test::toggles && G::g_host.empty());
    for(int stage=0;stage<4;++stage)for(int cause=0;cause<4;++cause){
        if(stage==0 && cause==3)continue; // identity is first sampled from the lookup result
        Reset(s,net::Role::Host);faucet=test::New(tap);
        auto change=[&,cause]{if(cause==0)G::OnDisconnect();if(cause==1)++test::world;
            if(cause==2)++test::playerNo;if(cause==3)++faucet->serial;};
        if(stage==0)test::gardenResolve=change;if(stage==1)test::gardenReady=change;
        if(stage==2)test::gardenKey=change;if(stage==3)test::gardenReach=change;
        Deliver(GardenPacket(1,1,tap),1);CHECK(!test::toggles);
    }
    Reset(s,net::Role::Host);faucet=test::New(tap);test::gardenAction=[&]{Deliver(GardenPacket(1,2,tap),1);};
    Deliver(GardenPacket(1,1,tap),1);CHECK(test::toggles==1 && G::g_host.size()==1);
    G::Tick();CHECK(test::toggles==2 && G::g_host.empty()); // no recursive native toggle
    Reset(s,net::Role::Host);faucet=test::New(tap);test::gardenAction=[]{G::OnDisconnect();};
    Deliver(GardenPacket(1,1,tap),1);CHECK(test::toggles==1 && !G::g_session && G::g_host.empty());
    Reset(s,net::Role::Host);
    for(uint32_t seq=1;seq<=33;++seq)Deliver(GardenPacket(1,seq,tap),1);
    CHECK(G::g_host.size()==32 && G::g_seen[1].count==32);
    Reset(s,net::Role::Client);faucet=test::New(tap);CHECK(G::Enqueue(faucet,G::kToggle));++test::playerNo;G::Tick();CHECK(s.sent.empty() && G::g_pending.empty());
    Reset(s,net::Role::Client);faucet=test::New(tap);CHECK(G::Enqueue(faucet,G::kToggle));
    G::g_pending.front().key=std::wstring(128,L'a');G::g_pending.front().endpoint=std::wstring(128,L'b');
    s.onSend=[]{++test::world;};G::Tick();CHECK(!s.sent.empty() && s.sent.size()==s.sent.front().chunks); // admitted batch stays owned after source retirement
    Reset(s,net::Role::Client);faucet=test::New(tap);CHECK(G::Enqueue(faucet,G::kToggle));
    s.onSend=[]{G::OnDisconnect();};G::Tick();CHECK(!G::g_session && G::g_pending.empty());
    puts("PASS garden actions: ordered host faucet clicks, duplicate/rejected sequence, role/reach/family, detach endpoint identity, native host cleanup, retained client queue, shared blob sequence and world/peer cleanup");
}
