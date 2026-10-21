#define DRINK_SPILL_FIXTURE_ONLY
#include "drink_spill_test.cpp"
int main(){using namespace spill_test;using namespace test;using coop::net::Role;using V=ue_wrap::script_gate::Verdict;
    coop::net::Session session;ue_wrap::script_gate::Call call{};call.object=&actors[0];call.invocation=1;
    const auto setup=[&](Role role=Role::Host){spill_test::Reset(session,role);kind=N::kExplosionScorch;scorchSource=true;};
    setup();CHECK(D::BeforeScorch(call)==V::Run && D::g_scorchCalls.size()==1);
    D::ObserveFinish(nullptr,&actors[0],&actors[2]);D::ObserveFinish(nullptr,&actors[0],&actors[2]);D::Tick();
    CHECK(D::g_records.size()==1 && session.spills.size()==3 && !authors && !sounds && D::g_flights.empty());
    const auto birth=session.spills[0].payload;const auto key=D::KeyFor(&actors[2]);
    CHECK(birth.kind==N::kExplosionScorch && birth.op==1 && !birth.source.len && !birth.slot && key.size()==28);
    CHECK(D::FindByKey(key)==&actors[2]);
    // A blocked recipient still gets the current cleaning floor and retirement.
    session.spills.clear();session.allowSpill=[](int slot,const auto&){return slot!=2;};
    actors[2].process=25;D::SendAllToSlot(2);D::Tick();CHECK(session.spills.empty());
    session.allowSpill={};D::Tick();CHECK(session.spills.size()==1 && session.spills[0].payload.op==2 && session.spills[0].payload.process==25);
    const auto join=session.spills[0].payload;session.spills.clear();session.allowSpill=[](int,const auto&){return false;};D::OnRemoved(&actors[2]);D::Tick();
    CHECK(session.spills.empty() && !D::FindByKey(key));session.allowSpill={};D::Tick();CHECK(session.spills.size()==3 && session.spills[0].payload.op==3);
    setup(Role::Client);CHECK(D::BeforeScorch(call)==V::Cancel && D::g_scorchCalls.empty());
    call.fromOurCode=true;CHECK(D::BeforeScorch(call)==V::Cancel);call.fromOurCode=false;
    scorchSource=false;CHECK(D::BeforeScorch(call)==V::Run);
    D::Receive(birth,1);D::Tick();CHECK(!authors);D::Receive(birth,0);D::Tick();
    CHECK(authors==1 && !sounds && D::FindByKey(key));D::Receive(birth,0);D::Tick();CHECK(authors==1 && !sounds);
    auto retired=birth;retired.op=3;D::Receive(retired,0);D::Receive(birth,0);D::Tick();CHECK(!D::FindByKey(key) && destroys==1);
    setup(Role::Client);D::g_baselineComplete=false;D::Receive(join,0);actors[2].location={join.x,join.y,join.z};
    D::ObserveExisting(&actors[2]);D::IndexComplete(world);existingReady=false;D::Tick();CHECK(!authors && !sounds);
    existingReady=true;D::Tick();CHECK(!authors && !sounds && D::FindByKey(key)==&actors[2] && actors[2].process==25);
    // A stale or unrelated invocation must not claim another factory's decal.
    for(int cause=0;cause<5;++cause){setup();CHECK(D::BeforeScorch(call)==V::Run);
        if(cause==0)activeInvocations.clear();if(cause==1)++actors[0].serial;if(cause==2)++world;
        if(cause==3)session.mode=Role::Client;
        D::ObserveFinish(nullptr,cause==4?&actors[3]:&actors[0],&actors[2]);CHECK(D::g_candidates.empty() && D::g_records.empty());}
    setup();D::ObserveFinish(nullptr,&actors[0],&actors[2]);D::Tick();CHECK(D::g_records.empty());
    setup();call.invocation=0;CHECK(D::BeforeScorch(call)==V::Run && D::g_scorchCalls.empty());call.invocation=1;
    setup();onScorchSource=[]{++world;};CHECK(D::BeforeScorch(call)==V::Cancel && D::g_scorchCalls.empty());
    setup();onScorchSource=[]{++actors[0].serial;};CHECK(D::BeforeScorch(call)==V::Cancel && D::g_scorchCalls.empty());
    setup();CHECK(D::BeforeScorch(call)==V::Run);D::ObserveExisting(&actors[2]);CHECK(D::g_candidates.size()==1 && D::g_candidates[0].baseline);
    D::ObserveFinish(nullptr,&actors[0],&actors[2]);CHECK(D::g_candidates.size()==1 && !D::g_candidates[0].baseline);
    D::Tick();CHECK(D::g_records.size()==1 && session.spills[0].payload.op==1);
    setup();CHECK(D::BeforeScorch(call)==V::Run);activeInvocations.clear();D::Tick();CHECK(D::g_scorchCalls.empty());
    setup();CHECK(D::BeforeScorch(call)==V::Run);D::OnDisconnect();CHECK(D::g_scorchCalls.empty());
    // Computer-game explosions execute the same inherited method, on either peer.
    for(auto role:{Role::Host,Role::Client}){setup(role);scorchSource=false;privateScorchSource=true;
        CHECK(D::BeforeScorch(call)==V::Run);D::ObserveExisting(&actors[2]);
        CHECK(D::PrivateScorch(&actors[2]) && D::ExcludeFromStaticIndex(&actors[2]));
        D::ObserveFinish(nullptr,&actors[0],&actors[2]);D::Tick();
        CHECK(D::g_candidates.empty() && D::g_records.empty() && session.spills.empty() && !destroys);
        activeInvocations.clear();D::Tick();CHECK(D::PrivateScorch(&actors[2]));
        ++actors[2].serial;CHECK(!D::PrivateScorch(&actors[2]));}
    // Unknown provenance stays in the old static cleaning lane; it is not a new birth.
    setup();D::ObserveExisting(&actors[2]);D::Tick();
    CHECK(D::g_candidates.empty() && D::g_records.empty() && !D::ExcludeFromStaticIndex(&actors[2]));
    setup(Role::Client);D::ObserveExisting(&actors[2]);CHECK(!D::ExcludeFromStaticIndex(&actors[2]));
    // Native metadata can arrive late without deleting the host's real scorch.
    setup();CHECK(D::BeforeScorch(call)==V::Run);D::ObserveFinish(nullptr,&actors[0],&actors[2]);
    readReady=false;D::Tick();CHECK(D::g_candidates.size()==1);D::g_candidates[0].expires=1;D::Tick();
    CHECK(!destroys && D::g_candidates.size()==1 && D::ExcludeFromStaticIndex(&actors[2]));
    activeInvocations.clear();readReady=true;D::Tick();
    CHECK(D::g_records.size()==1 && session.spills[0].payload.op==1);
    D::OnDisconnect();D::Install(&session);D::ObserveExisting(&actors[2]);D::Tick();CHECK(D::g_records.size()==1);
    D::OnDisconnect();++world;D::Install(&session);D::ObserveExisting(&actors[2]);D::Tick();CHECK(D::g_records.empty());
    setup();auto launch=Request();launch.kind=N::kExplosionScorch;D::ReceiveLaunch(launch,1);D::Tick();CHECK(D::g_requests.empty() && !authors);
    // All six surfaces keep independent identities even in the same static cell.
    setup();CHECK(D::BeforeScorch(call)==V::Run);std::set<std::wstring> surfaceKeys;
    for(int i=100;i<106;++i){actors[i].location={};actors[i].process=100;
        S::Floor()[S::PosKey(&actors[i])]=0;D::ObserveFinish(nullptr,&actors[0],&actors[i]);
        D::ObserveFinish(nullptr,&actors[0],&actors[i]);S::HubMatch(nullptr,&actors[i]);CHECK(actors[i].process==100);}
    D::Tick();CHECK(D::g_records.size()==6 && session.spills.size()==18);
    for(int i=100;i<106;++i)surfaceKeys.insert(D::KeyFor(&actors[i]));CHECK(surfaceKeys.size()==6 && !surfaceKeys.contains(L""));
    session.spills.clear();actors[100].process=0;D::SendAllToSlot(1);D::Tick();
    CHECK(session.spills.size()==6);int clean=0;for(const auto& sent:session.spills){CHECK(sent.payload.op==2);clean+=sent.payload.process==0;}
    CHECK(clean==1);const auto surfaces=session.spills;
    setup(Role::Client);for(const auto& sent:surfaces){D::Receive(sent.payload,0);D::Receive(sent.payload,0);}D::Tick();
    CHECK(authors==6 && !sounds && D::g_records.size()==6);
    puts("PASS explosion scorch: exact native factory invocation, host-only surfaces, canonical birth/cleaning/join/retirement, quiet saved adoption, retries and no liquid launch authority");return 0;
}

