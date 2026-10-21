// Actual production transaction handlers, with the same engine/network boundaries as the birth tests.
#define main LegacyBirthTests
#include "pending_birth_2130_test.cpp"
#undef main
#include <limits>
using coop::net::PropPlacementCode;
using coop::net::ReliableKind;
coop::net::PropDropIntentPayload Request() {
    test::Reset();test::Enqueue();D::Tick(&test::session);
    CHECK(test::session.drops.size()==1);
    return test::session.drops.back();
}
void Host() {D::Reset();test::session.mode=coop::net::Role::Host;}
int main() {
    CHECK(LegacyBirthTests()==0);
    using namespace test;
    auto p=Request();auto reply=D::PlacementReply(p,PropPlacementCode::Accepted);
    D::OnPlacementResult(reply,1);CHECK(D::IsAwaitingHost(&actor)); // peer cannot acknowledge
    reply=D::PlacementReply(p,PropPlacementCode::Refused);++reply.requestId;
    D::OnPlacementResult(reply,0);CHECK(D::IsAwaitingHost(&actor)); // stale result
    --reply.requestId;D::OnPlacementResult(reply,0);
    CHECK(!D::IsAwaitingHost(&actor)&&D::IsRefusedPlacement(&actor)&&destroys==0&&notices==1);
    coop::join_membership_sweep::LocalBirths none;
    CHECK(coop::join_membership_sweep::ProtectedLocalBirth(&actor,none));
    Enqueue();D::Tick(&session);CHECK(session.drops.size()==1); // same refused object cannot bypass result
    ++serial;CHECK(!D::IsRefusedPlacement(&actor));
    p=Request();reply=D::PlacementReply(p,PropPlacementCode::Accepted);D::OnPlacementResult(reply,0);
    CHECK(D::IsAwaitingHost(&actor)&&D::FindAwaitingHost(L"item",L"prop_test_C")==&actor);
    D::g_awaitingHost[0].next=0;D::Tick(&session);
    CHECK(session.drops.size()==2&&session.drops[1].requestId==p.requestId);
    tracked=true;D::Tick(&session);CHECK(!D::IsAwaitingHost(&actor));
    p=Request();Host();D::OnPropDropIntent(session,p,1);CHECK(begins==1);
    session.sendOK=false;D::Tick(&session);CHECK(session.replies.empty()&&D::PlacementRow(1,p.requestId)->owed);
    session.sendOK=true;D::Tick(&session);CHECK(session.replies.size()==1&&session.replies[0].code==0);
    D::OnPropDropIntent(session,p,1);D::Tick(&session);
    CHECK(begins==1&&expresses==1&&session.replies.size()==2&&session.replies.back().code==0);
    auto collision=p;collision.locX+=1;D::OnPropDropIntent(session,collision,1);
    CHECK(begins==1&&session.replies.back().code==1&&D::PlacementRow(1,p.requestId)->code==PropPlacementCode::Accepted);
    p=Request();Host();duringFinish=[&]{D::OnPropDropIntent(session,p,1);};
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(begins==1&&session.replies.size()==1);
    p=Request();Host();keySetOK=false;D::OnPropDropIntent(session,p,1);D::Tick(&session);
    CHECK(begins==1&&!finished&&destroys==1&&session.replies.back().code==1);
    D::OnPropDropIntent(session,p,1);CHECK(begins==1); // refusal is terminal for this request
    p=Request();Host();finishOK=false;D::OnPropDropIntent(session,p,1);D::Tick(&session);
    CHECK(destroys==1&&session.replies.back().code==1);
    for(bool parityFailure:{false,true}) {
        p=Request();Host();if(parityFailure)parityOK=false;else scaleOK=false;
        D::OnPropDropIntent(session,p,1);D::Tick(&session);
        CHECK(destroys==1&&session.replies.back().code==1);
    }
    p=Request();Host();p.locX=std::numeric_limits<float>::quiet_NaN();
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(begins==0&&session.replies.back().code==1);
    p=Request();Host();D::OnReelEjectIntent(session,p,1);D::Tick(&session);
    CHECK(begins==0&&session.replies.back().code==1); // fresh-birth whitelist still enforced
    p=Request();Host();indexRead=coop::prop_key_index::Read::Pending;
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(begins==0&&session.replies.back().code==2);
    indexRead=coop::prop_key_index::Read::Miss;
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(begins==1&&session.replies.back().code==0);
    p=Request();Host();indexed=&actor;indexRead=coop::prop_key_index::Read::Hit;
    const auto original=p;
    for(int n=0;n<300;++n){p.requestId=original.requestId+n;D::OnPropDropIntent(session,p,1);D::Tick(&session);}
    CHECK(D::g_placementPeers[1].records.size()==256&&begins==0);
    indexed=nullptr;indexRead=coop::prop_key_index::Read::Miss;
    D::OnPropDropIntent(session,original,1);CHECK(begins==0&&session.replies.back().code==1);
    D::OnPeerLeft(1);D::OnPropDropIntent(session,original,1);CHECK(begins==1); // next occupant has a fresh ledger
    p=Request();Host();duringFinish=[]{++roster[1].bornGeneration;D::OnPeerLeft(1);};
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(session.replies.empty());
    p=Request();Host();duringBegin=[]{D::Reset();};
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(session.replies.empty()&&!finished);
    p=Request();Host();duringApplyParked=[]{D::Reset();};
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(session.replies.empty()&&expectedRecords==0);
    p=Request();Host();foodActor=true;p.hasFoodCondition=1;p.foodTemperature=300;p.foodRipeness=.5f;p.foodUses=2;
    duringFoodWrite=[]{foodWritable=false;D::Reset();};
    D::OnPropDropIntent(session,p,1);D::Tick(&session);CHECK(session.replies.empty()&&destroys==0&&invalidates==0);
    p=Request();Host();roster[1].playerNo=0;
    D::OnPropDropIntent(session,p,1);CHECK(begins==0&&D::g_placementPeers[1].records.empty());
    p=Request();const auto oldReply=D::PlacementReply(p,PropPlacementCode::Refused);
    const auto newer=Request();CHECK(newer.requestId!=p.requestId);
    D::OnPlacementResult(oldReply,0);CHECK(D::IsAwaitingHost(&actor)&&!D::IsRefusedPlacement(&actor));
    p=Request();Host();D::OnPropDropIntent(session,p,1);
    session.duringSend=[]{D::Reset();};D::Tick(&session);CHECK(D::g_placementPeers[1].records.empty());
    p=Request();currentWorld=&nextWorld;D::Tick(&session);CHECK(!D::IsAwaitingHost(&actor));
    reply=D::PlacementReply(p,PropPlacementCode::Refused);D::OnPlacementResult(reply,0);CHECK(notices==0);
    puts("PASS placement: accepted/refused/retry, retained local recovery, canonical adoption, immutable retry, dedup/reentrancy, backpressure, replay eviction, whitelist, native failure, recycled peer/world and stale reply");
}
