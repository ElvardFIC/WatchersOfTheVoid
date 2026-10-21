#define DRINK_SPILL_FIXTURE_ONLY
#include "drink_spill_test.cpp"
int main(){using namespace spill_test;using namespace test;using coop::net::Role;using ue_wrap::script_gate::Verdict;
    coop::net::Session session;ue_wrap::script_gate::Call call{};call.object=&actors[0];call.invocation=1;
    const auto ownerLaunch=[&]{spill_test::Reset(session,Role::Client);kind=3;handAxis=true;
        CHECK(D::BeforeGasBreak(call)==Verdict::Run && D::g_calls.size()==1);
        D::ObserveFinish(nullptr,&actors[0],&actors[1]);CHECK(freezes==1 && D::g_local.size()==1);
        activeInvocations.clear();D::Tick();CHECK(session.launches.size()==1 && destroys==1);
        return session.launches[0].payload;};
    const auto request=ownerLaunch();CHECK(request.kind==3 && !request.source.len && request.generation==7 && request.playerNo==11 && request.sx==1 && request.size==5);
    spill_test::Reset(session,Role::Host);kind=3;D::ReceiveLaunch(request,1);D::Tick();CHECK(authors==1 && D::g_flights.size()==1 && session.launches[0].payload.op==1);
    const auto accepted=session.launches[0].payload;D::ReceiveLaunch(request,1);D::Tick();CHECK(authors==1);
    D::ObserveFinish(nullptr,&actors[101],&actors[2]);D::Tick();CHECK(D::g_records.size()==1 && session.spills[0].payload.kind==3);
    ownerLaunch();auto wrong=accepted;wrong.kind=2;D::ReceiveLaunch(wrong,0);CHECK(D::g_local.size()==1);D::ReceiveLaunch(accepted,0);CHECK(D::g_local.empty());
    // Final-use source proof survives destruction; an unrelated retired class does not.
    spill_test::Reset(session,Role::Host);kind=3;mirror=nullptr;retiredProof=true;D::ReceiveLaunch(request,1);D::Tick();CHECK(authors==1);
    spill_test::Reset(session,Role::Host);kind=3;mirror=nullptr;retiredProof=true;proofClass=L"prop_snack_beer_C";D::ReceiveLaunch(request,1);D::Tick();CHECK(!authors);
    // No world source, remote hand factory, bad native shape or missing current generation.
    spill_test::Reset(session,Role::Client);kind=3;handAxis=true;isMirror=true;CHECK(D::BeforeGasBreak(call)==Verdict::Cancel && D::g_calls.empty());
    spill_test::Reset(session,Role::Client);kind=3;handAxis=true;hand=nullptr;CHECK(D::BeforeGasBreak(call)==Verdict::Cancel && D::g_calls.empty());
    spill_test::Reset(session,Role::Client);kind=3;flightReady=false;handAxis=true;CHECK(D::BeforeGasBreak(call)==Verdict::Cancel && D::g_calls.empty());
    for(int cause=0;cause<4;++cause){spill_test::Reset(session,Role::Host);kind=3;auto bad=request;
        if(cause==0)coop::net::WireKeyFromString(L"drink",bad.source);if(cause==1)bad.sx=.25f;if(cause==2)bad.size=1;if(cause==3)bad.generation=99;
        D::ReceiveLaunch(bad,1);D::Tick();CHECK(!authors);}
    spill_test::Reset(session,Role::Host);kind=3;handAxis=true;CHECK(D::BeforeGasBreak(call)==Verdict::Run && D::g_calls.size()==1);
    spill_test::Reset(session,Role::Host);kind=3;handAxis=true;isMirror=true;CHECK(D::BeforeGasBreak(call)==Verdict::Cancel && D::g_calls.empty());
    spill_test::Reset(session,Role::Client);kind=3;D::ObserveFinish(nullptr,&actors[0],&actors[1]);CHECK(destroys==1 && !actors[1].live && D::g_local.empty());
    puts("PASS owned gasoline break: immediate freeze, exact native hand launch, current/retired class proof, immutable ACK, host impact, world/mirror/shape refusal and unbound projectile cleanup");return 0;
}
