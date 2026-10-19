#define main PreviousPlacementChecks
#include "placed_save_2163_test.cpp"
#undef main

test::Rec Hose(const wchar_t* key=L"saved_hose_5") {
    test::Rec r; r.key=key; r.className=L"groundHose_C";
    r.names={{L"saved_tap_4",L"None"}};
    r.vectors={{{0,0,0},{100,0,0},{200,0,0}}}; return r;
}
std::vector<net::BlobChunkPayload> Wire(const std::vector<uint8_t>& bytes, uint32_t blobSeq=900000) {
    net::Session s;CHECK(blob_chunks::SendBlob(&s,net::ReliableKind::PlacedSave,blobSeq,bytes));return s.sent;
}
std::vector<net::BlobChunkPayload> Route(const test::Rec& r,uint64_t baseline,uint32_t seq=1) {
    std::vector<uint8_t> b;W::AppU8(b,HoseRoute);W::SerSave(b,r);W::AppU32(b,seq);
    W::AppU32(b,uint32_t(baseline));W::AppU32(b,uint32_t(baseline>>32));return Wire(b);
}
std::vector<net::BlobChunkPayload> Answer(const test::Rec& r,uint32_t seq,uint8_t status=1,uint32_t blobSeq=900001) {
    std::vector<uint8_t> b;W::AppU8(b,HoseReply);W::AppU32(b,seq);W::AppU8(b,status);W::AppWStr(b,r.key);
    if(status==1)W::SerSave(b,r);return Wire(b,blobSeq);
}
test::Actor* Adopt(net::Session& s,net::Role role,const test::Rec& r) {
    Reset(s,role);auto* a=test::New(r);
    if(role==net::Role::Host){ue_wrap::CachedObjRef ref;ref.Set(a);CHECK(EnrollPlaced(ref));}
    else {Deliver(Packet(State,r),0);Pump();}
    test::applied=test::linked=0;s.sent.clear();return a;
}
ue_wrap::script_gate::Call Resume(test::Actor* a) {
    ue_wrap::script_gate::Call c{};c.object=a;c.function=test::caller;
    CHECK(BeforeHoseResume(c)==ue_wrap::script_gate::Verdict::Run);HoseResumed(c);return c;
}
void TravelDuringCapture(){++test::world;World();}
void LeaveDuringCapture(){OnPeerLeftSlot(1);}
int main() {
    net::Session s;const auto original=Hose();auto changed=original;changed.vectors[0].back()={450,150,0};
    changed.names[0][1]=L"sprinkler_B";
    auto* a=Adopt(s,net::Role::Client,original);auto c=Resume(a);
    CHECK(g_hoseEdits.size()==1);a->record=changed;a->record.xform[3]=0.99999994f;
    auto hostChanged=original;hostChanged.vectors[0].back()={250,0,0};
    Deliver(Packet(State,hostChanged),0);Pump();
    CHECK(test::applied==0&&a->record.vectors==changed.vectors&&g_hoseEdits.size()==1);
    HoseFinished(c);Pump();CHECK(g_hoseEdits.at(original.key).captured&&!s.sent.empty());
    const auto pending=g_hoseEdits.at(original.key);const auto request=s.sent;
    CHECK(pending.proposal.vectors==changed.vectors&&pending.baseline==HoseFingerprint(original));
    CHECK(BeforeHoseResume(c)==ue_wrap::script_gate::Verdict::Cancel);
    uint8_t action[16]{4};c.locals=action;CHECK(BeforeAction(c)==ue_wrap::script_gate::Verdict::Cancel);
    Deliver(Answer(original,pending.seq+1),0);CHECK(g_hoseEdits.size()==1);
    Deliver(Answer(original,pending.seq),1);CHECK(g_hoseEdits.size()==1); // no client-authored answer
    Deliver(Packet(State,original),0);Pump();CHECK(g_hoseEdits.size()==1&&test::applied==0);

    // Same transaction uses real record/chunk parsing on host and client.
    a=Adopt(s,net::Role::Host,original);g_blobSeq=1000000;
    Deliver(request,1);CHECK(test::applied==1&&g_rows.at(original.key).link&&s.sent.empty());
    c.object=a;CHECK(BeforeAction(c)==ue_wrap::script_gate::Verdict::Cancel&&BeforeHoseResume(c)==ue_wrap::script_gate::Verdict::Cancel);
    test::linkOK=false;Pump();CHECK(g_rows.at(original.key).link&&s.sent.empty());
    test::linkOK=true;Pump();CHECK(!g_rows.at(original.key).link&&!s.sent.empty());const auto response=s.sent;
    bool targetedAck=false;
    for(size_t i=0;i<s.sent.size();++i)if(s.sent[i].chunkIdx==0&&s.sent[i].data[0]==HoseReply){CHECK(s.recipients[i]==1);targetedAck=true;}
    CHECK(targetedAck);
    CHECK(a->record.vectors==changed.vectors&&a->record.names==changed.names);
    Deliver(request,1);Pump();CHECK(test::applied==1); // retry never reloads stale route
    a->record=hostChanged;Deliver(request,1);Pump();CHECK(test::applied==1&&a->record.vectors==hostChanged.vectors);
    a=Adopt(s,net::Role::Client,original);g_hoseEdits.emplace(original.key,pending);
    g_hoseEdits.at(original.key).ref.Set(a);a->record=changed;
    Deliver(response,0);Pump();CHECK(g_hoseEdits.empty()&&a->record.vectors==changed.vectors&&a->record.names==changed.names);
    // An older canonical blob completing after the acknowledgement cannot undo it.
    Deliver(Packet(State,original),0);Pump();CHECK(a->record.vectors==changed.vectors);

    // Conflict, root/faucet mutation, reach and endpoint occupancy return the
    // actual host route. No case mutates or creates a host actor.
    for(int mode=0;mode<6;++mode){
        a=Adopt(s,net::Role::Host,original);auto proposal=changed;
        if(mode==0)a->record=hostChanged;
        if(mode==1)proposal.xform[4]=123;
        if(mode==2)proposal.names[0][0]=L"other_tap";
        if(mode==3)proposal.vectors[0].front()={1,0,0};
        if(mode==4)test::reach=false;
        if(mode==5)test::linkOK=false;
        Deliver(Route(proposal,HoseFingerprint(original)),1);Pump();
        CHECK(test::applied==0&&test::spawned==0&&!s.sent.empty()&&g_hoseAnswers[1].empty());
        test::reach=test::linkOK=true;Deliver(Route(proposal,HoseFingerprint(original)),1);Pump();CHECK(test::applied==0);
    }
    // Native host edit wins; its eventual finish determines the reply. Rejected
    // operation IDs stay rejected even once the earlier condition disappears.
    a=Adopt(s,net::Role::Host,original);test::ready=false;
    Deliver(Route(changed,HoseFingerprint(original)),1);Pump();CHECK(test::applied==0&&!g_hoseAnswers[1].empty());
    a->record=hostChanged;test::ready=true;Pump();CHECK(g_hoseAnswers[1].empty());
    Deliver(Route(changed,HoseFingerprint(original)),1);Pump();CHECK(test::applied==0);
    // Two peers start with one baseline: first accepted completion wins.
    a=Adopt(s,net::Role::Host,original);
    Deliver(Route(changed,HoseFingerprint(original),1),1);
    Deliver(Route(hostChanged,HoseFingerprint(original),1),2);Pump();
    CHECK(test::applied==1&&a->record.vectors==changed.vectors&&g_hoseAnswers[1].empty()&&g_hoseAnswers[2].empty());
    // Two different hoses cannot both reserve an unoccupied sprinkler before
    // either native Link has run. The losing route receives its original state.
    a=Adopt(s,net::Role::Host,original);auto second=Hose(L"second_hose");second.names[0][0]=L"second_tap";
    auto* secondActor=test::New(second);ue_wrap::CachedObjRef secondRef;secondRef.Set(secondActor);CHECK(EnrollPlaced(secondRef));
    auto secondProposal=second;secondProposal.names[0][1]=changed.names[0][1];
    Deliver(Route(changed,HoseFingerprint(original),1),1);
    Deliver(Route(secondProposal,HoseFingerprint(second),1),2);Pump();
    CHECK(test::applied==1&&secondActor->record.names==second.names&&g_hoseAnswers[2].empty());
    // Missing identities cannot become remote creations. Wrong families are
    // answered too, rather than leaving a permanently blocked reply head.
    Reset(s,net::Role::Host);Deliver(Route(changed,HoseFingerprint(original)),1);Pump();
    CHECK(test::spawned==0&&g_rows.empty()&&g_hoseAnswers[1].empty()&&!s.sent.empty());
    auto wrong=test::Make(original.key.c_str());Adopt(s,net::Role::Host,wrong);
    Deliver(Route(changed,HoseFingerprint(original)),1);Pump();CHECK(test::applied==0&&g_hoseAnswers[1].empty());
    // Backpressure retains the exact proposal/nonce on client and response on
    // host. Partial chunks are retried, not treated as a commit or an ACK.
    a=Adopt(s,net::Role::Client,original);c=Resume(a);a->record=changed;HoseFinished(c);
    s.sendBudget=1;Pump();CHECK(g_hoseEdits.size()==1&&s.sent.size()==1);
    const auto seq=g_hoseEdits.at(original.key).seq;g_hoseEdits.at(original.key).next=0;s.sent.clear();s.sendBudget=-1;
    Pump();CHECK(g_hoseEdits.at(original.key).seq==seq&&s.sent.size()>1);
    a=Adopt(s,net::Role::Host,original);Deliver(Route(changed,HoseFingerprint(original)),1);
    s.sendBudget=0;Pump();CHECK(!g_hoseAnswers[1].empty()&&test::applied==1);
    s.sendBudget=-1;Pump();CHECK(g_hoseAnswers[1].empty()&&test::applied==1);
    // A newer canonical state can precede an older explicit answer. It releases
    // the gesture, but the older reply must not replace that newer state.
    a=Adopt(s,net::Role::Client,original);c=Resume(a);a->record=changed;HoseFinished(c);Pump();
    const auto currentSeq=g_hoseEdits.at(original.key).seq;
    std::vector<uint8_t> state;W::AppU8(state,State);W::SerSave(state,hostChanged);
    Deliver(Wire(state,900010),0);Deliver(Answer(changed,currentSeq,1,900009),0);Pump();
    CHECK(g_hoseEdits.empty()&&a->record.vectors==hostChanged.vectors);
    // Retirement, cancelled native resume, slot recycling and world travel.
    a=Adopt(s,net::Role::Client,original);c=Resume(a);Deliver(Packet(Retired,original),0);Pump();
    CHECK(g_hoseEdits.empty()&&!a->live);
    a=Adopt(s,net::Role::Client,original);c={};c.object=a;c.function=test::caller;
    CHECK(BeforeHoseResume(c)==ue_wrap::script_gate::Verdict::Run);Pump();CHECK(g_hoseEdits.empty());
    a=Adopt(s,net::Role::Host,original);Deliver(Route(changed,HoseFingerprint(original)),1);
    OnPeerLeftSlot(1);CHECK(g_hoseAnswers[1].empty()&&g_hoseSeen[1].seq==0);
    a=Adopt(s,net::Role::Host,original);test::captureCallback=&TravelDuringCapture;
    Deliver(Route(changed,HoseFingerprint(original)),1);CHECK(g_rows.empty()&&g_hoseAnswers[1].empty()&&test::applied==0);
    a=Adopt(s,net::Role::Host,original);test::captureCallback=&LeaveDuringCapture;
    Deliver(Route(changed,HoseFingerprint(original)),1);CHECK(g_hoseAnswers[1].empty()&&test::applied==0);
    a=Adopt(s,net::Role::Host,original);test::applyCallback=&LeaveDuringCapture;
    Deliver(Route(changed,HoseFingerprint(original)),1);CHECK(g_hoseAnswers[1].empty()&&g_rows.at(original.key).link);
    test::applyCallback=nullptr;Pump();CHECK(!g_rows.at(original.key).link&&a->record.vectors==changed.vectors);
    a=Adopt(s,net::Role::Host,original);test::applyOK=false;
    Deliver(Route(changed,HoseFingerprint(original)),1);CHECK(test::applied==2&&a->record.vectors==original.vectors);
    test::applyOK=true;Pump();CHECK(!g_rows.at(original.key).link&&g_hoseAnswers[1].empty());
    a=Adopt(s,net::Role::Host,original);Deliver(Route(changed,HoseFingerprint(original)),1);
    test::captureCallback=&LeaveDuringCapture;Pump();CHECK(g_hoseAnswers[1].empty());
    OnDisconnect();CHECK(g_hoseEdits.empty());
    puts("PASS hose routes: client gesture/host commit, real chunked request/targeted ACK, old-state ordering, conflicts/replay, root/reach/endpoints, backpressure, slot/world lifetime");
}
