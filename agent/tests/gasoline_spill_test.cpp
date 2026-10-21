#define DRINK_SPILL_FIXTURE_ONLY
#include "drink_spill_test.cpp"
int main(){using namespace spill_test;using namespace test;using coop::net::Role;using ue_wrap::script_gate::Verdict;
    coop::net::Session session;ue_wrap::script_gate::Call call{};call.object=&actors[0];call.invocation=1;
    spill_test::Reset(session,Role::Host);kind=3;
    CHECK(D::BeforeGasBreak(call)==Verdict::Run && D::g_calls.size()==1);
    D::ObserveFinish(nullptr,&actors[0],&actors[1]);D::ObserveFinish(nullptr,&actors[1],&actors[2]);D::Tick();
    CHECK(D::g_flights.size()==1 && D::g_records.size()==1 && session.spills.size()==3 && !authors);
    const auto birth=session.spills[0].payload;const auto key=D::KeyFor(&actors[2]);
    CHECK(birth.kind==3 && birth.op==1 && key.size()==28 && D::FindByKey(key)==&actors[2]);
    auto bad=key;bad[3]=L'z';CHECK(!D::FindByKey(bad));bad=key;bad[3]=key[3]==L'0'?L'1':L'0';CHECK(!D::FindByKey(bad));
    CHECK(!D::FindByKey(key+L"0") && !D::FindByKey(L"ds_0000000000000000_00000000"));
    actors[2].process=20;session.spills.clear();D::SendAllToSlot(2);D::Tick();CHECK(session.spills.size()==1 && session.spills[0].payload.op==2 && session.spills[0].payload.process==20);
    auto joined=session.spills[0].payload;
    D::OnRemoved(&actors[2]);CHECK(D::KeyFor(&actors[2]).empty() && !D::FindByKey(key));D::Tick();CHECK(session.spills.back().payload.op==3);
    spill_test::Reset(session,Role::Client);kind=3;CHECK(D::BeforeGasBreak(call)==Verdict::Cancel && D::g_calls.empty());
    handAxis=true;CHECK(D::BeforeGasBreak(call)==Verdict::Run);handAxis=false;
    call.object=&actors[1];CHECK(D::BeforeFlight(call)==Verdict::Cancel);call.object=&actors[0];
    auto request=Request();request.kind=3;request.sx=request.sy=request.sz=1;request.size=5;coop::net::WireKeyFromString(L"drink",request.source);
    D::ReceiveLaunch(request,1);CHECK(D::g_requests.empty());
    spill_test::Reset(session,Role::Host);kind=3;D::ReceiveLaunch(request,1);D::Tick();CHECK(!authors && D::g_requests.empty());
    spill_test::Reset(session,Role::Client);kind=3;D::Receive(birth,1);D::Tick();CHECK(!authors);
    D::Receive(birth,0);D::Tick();CHECK(authors==1 && sounds==1 && D::FindByKey(key));
    auto retired=birth;retired.op=3;D::Receive(retired,0);D::Tick();CHECK(!D::FindByKey(key) && destroys==1);
    // Saved gasoline adopts the existing canonical actor without a second spill or sound.
    spill_test::Reset(session,Role::Client);kind=3;D::g_baselineComplete=false;D::Receive(joined,0);
    actors[2].location={joined.x,joined.y,joined.z};D::ObserveExisting(&actors[2]);D::IndexComplete(world);D::Tick();
    CHECK(!authors && !sounds && D::FindByKey(key)==&actors[2]);
    ++actors[2].serial;CHECK(!D::FindByKey(key));D::OnDisconnect();CHECK(!D::FindByKey(key));
    puts("PASS gasoline spills: host break/flight/birth/retirement, no client launch authority, dynamic identity, quiet saved adoption and current cleaning joins");return 0;
}
