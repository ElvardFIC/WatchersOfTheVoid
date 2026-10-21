#define main PreviousPlacementMain
#include "placed_save_2163_test.cpp"
#undef main
test::Rec Plant(const wchar_t* key=L"native_garden_17"){
    auto r=test::Make(key,L"growingPlant_C");r.bools.clear();r.names={{},{L"tomato",L"None"}};
    r.floats={{},{.2f,100.f,50.f,0.f,0.f,0.f}};return r;
}
namespace care_test {
net::Session session;test::Actor* plant=nullptr;test::Actor* tool=nullptr;void* player=nullptr;SG::Call call{};
void Setup(){Reset(session,net::Role::Client);auto record=Plant();plant=test::New(record);Deliver(Packet(State,record),0);Pump();
    players::Registry::Get().peer=1;
    tool=test::New(test::Make(L"bucket_key",L"prop_bucket_C"));test::hand=tool;test::careReadable=test::careSpent=true;
    test::careUse={plant,L"bucket_key",300.f,P::CareKind::Water};player=players::Registry::Get().Local();call={};call.object=tool;
    call.function=test::caller;call.locals=reinterpret_cast<uint8_t*>(&player);call.invocation=1;test::careInvocations={1};session.sent.clear();
}
void Begin(){CHECK(BeforeCare(call)==SG::Verdict::Run);}
void Paid(){Begin();AfterCare(call);CHECK(g_uses.size()==1);}
}
int main(){using namespace care_test;
    Setup();Begin();auto unrelated=call;unrelated.invocation=2;test::careReadable=false;
    CHECK(BeforeCare(unrelated)==SG::Verdict::Run);AfterCare(unrelated);CHECK(g_careFrames.size()==1 && g_uses.empty());
    AfterCare(call);CHECK(g_careFrames.empty() && g_uses.size()==1 && g_uses.front().care.amount==300);
    Setup();Begin();auto other=Plant(L"native_garden_18");auto* second=test::New(other);Deliver(Packet(State,other),0);Pump();
    auto nested=call;nested.invocation=2;test::careInvocations.insert(2);test::careUse={second,L"bucket_key",600.f,P::CareKind::Water};
    CHECK(BeforeCare(nested)==SG::Verdict::Run && g_careFrames.size()==2);AfterCare(call);CHECK(g_uses.size()==1 && g_uses.front().key==plant->record.key);
    AfterCare(nested);CHECK(g_careFrames.empty() && g_uses.size()==2 && g_uses.back().key==other.key);
    Setup();Begin();test::careInvocations={2};nested=call;nested.invocation=2;
    CHECK(BeforeCare(nested)==SG::Verdict::Run && g_careFrames.size()==1);AfterCare(call);CHECK(g_careFrames.size()==1 && g_uses.empty());AfterCare(nested);CHECK(g_uses.size()==1);
    Setup();call.invocation=0;CHECK(BeforeCare(call)==SG::Verdict::Cancel && g_careFrames.empty());
    for(int cause=0;cause<7;++cause){Setup();test::onCareRead=[cause]{if(cause==0)++test::world;if(cause==1)OnDisconnect();if(cause==2)++test::playerNo;
        if(cause==3)++tool->serial;if(cause==4)++g_hosePeerEpoch[1];if(cause==5)players::Registry::Get().peer=2;if(cause==6)session.mode=net::Role::Host;};
        CHECK(BeforeCare(call)==SG::Verdict::Cancel && g_careFrames.empty());}
    for(int cause=0;cause<6;++cause){Setup();Begin();if(cause==0)++test::world;if(cause==1)OnDisconnect();if(cause==2)++test::playerNo;
        if(cause==3)OnPeerLeftSlot(1);if(cause==4)players::Registry::Get().peer=2;if(cause==5)session.mode=net::Role::Host;
        AfterCare(call);CHECK(g_uses.empty());}
    for(int cause=0;cause<4;++cause){Setup();Begin();test::onCareSpent=[cause]{if(cause==0)++test::world;if(cause==1)++test::playerNo;
        if(cause==2)++tool->serial;if(cause==3)OnDisconnect();};AfterCare(call);CHECK(g_uses.empty());}
    // Paid care keeps its old key if the local crop vanishes; the host validates it.
    Setup();Begin();plant->live=false;AfterCare(call);CHECK(g_uses.size()==1);FlushUses(Now());CHECK(!session.sent.empty());
    Setup();tool->record.className=L"prop_fertilizer_C";test::careUse.kind=P::CareKind::Fertilizer;test::careUse.amount=600;Begin();tool->live=false;
    ++test::world;AfterCare(call);CHECK(g_uses.empty());
    for(int cause=0;cause<3;++cause){Setup();Paid();if(cause==0)++test::playerNo;if(cause==1)++g_hosePeerEpoch[1];if(cause==2)players::Registry::Get().peer=2;
        FlushUses(Now());CHECK(g_uses.empty() && session.sent.empty());}
    Setup();Paid();session.onSend=[]{++test::playerNo;};FlushUses(Now());CHECK(g_uses.size()==1);FlushUses(Now());CHECK(g_uses.empty());
    Setup();Paid();session.onSend=[]{auto next=g_uses.front();g_uses.clear();++next.seq;g_uses.push_back(next);};FlushUses(Now());
    CHECK(g_uses.empty() && session.sent.size()==2);
    Setup();Begin();test::careInvocations.clear();g_tickAt=0;Tick();CHECK(g_careFrames.empty());
    puts("PASS actual paid garden use: invocation-bound nested reservations, skipped POST, owner/world/slot epochs, final cost after crop loss, native/read/send interruption and reentrant queue replacement");return 0;
}
