#define main PreviousGardenWrapperChecks
#include "garden_wrapper_test.cpp"
#undef main

namespace {
namespace P=ue_wrap::placed_save;
using Rec=ue_wrap::save_record::SaveRecord;
test::Obj *hose, *tap, *oldEnd, *newEnd, *otherHose;
Rec ResetLinks() {
    using namespace test;
    for(int i=0;i<20;++i){objs[i]={};objs[i].id=i;objs[i].name.ComparisonIndex=i;}
    keys.clear();fields=callOK=processOK=true;captureOK=false;processCallback=nullptr;nameCallback=nullptr;
    hose=&objs[9];tap=&objs[10];oldEnd=&objs[11];newEnd=&objs[12];otherHose=&objs[13];
    hose->classId=otherHose->classId=7;tap->classId=6;oldEnd->classId=newEnd->classId=8;
    hose->key=L"saved_hose";hose->faucet=tap;hose->sprinkler=oldEnd;hose->sprinklerKey=L"old_end";
    tap->hose=hose;tap->hoseKey=hose->key;oldEnd->hose=hose;oldEnd->hoseKey=hose->key;
    keys[L"tap"]=tap;keys[L"old_end"]=oldEnd;keys[L"new_end"]=newEnd;
    Rec r;r.key=hose->key;r.className=L"groundHose_C";r.names={{L"tap",L"new_end"}};
    r.vectors={{{0,0,0},{100,0,0}}};return r;
}
void VerifyBeforeFinish(void* actor) {
    CHECK(actor==hose&&hose->faucet==tap&&hose->sprinkler==newEnd);
    CHECK(tap->hose==hose&&tap->hoseKey==hose->key);
    CHECK(newEnd->hose==hose&&newEnd->hoseKey==hose->key);
    CHECK(!oldEnd->hose&&oldEnd->hoseKey==L"None"&&oldEnd->updates==1);
}
void TravelOnFinish(void*){++test::world;}
void RecycleOnFinish(void*){++hose->serial;}
void TravelOnName(){++test::world;}
}
int main() {
    auto r=ResetLinks();CHECK(P::HoseLinksAvailable(hose,r));
    test::processCallback=&VerifyBeforeFinish;
    CHECK(P::Link(hose,r)&&hose->generations==1&&newEnd->updates==1);
    CHECK(P::Link(hose,r)&&hose->generations==2&&oldEnd->updates==1); // repeat rebuild, no repeated detach
    test::processCallback=nullptr;r.names[0][1]=L"None";
    CHECK(P::Link(hose,r)&&!hose->sprinkler&&!newEnd->hose&&newEnd->hoseKey==L"None"&&newEnd->updates==3);
    CHECK(tap->hose==hose&&hose->generations==3);
    r=ResetLinks();newEnd->hose=otherHose;newEnd->hoseKey=L"other_hose";
    CHECK(!P::HoseLinksAvailable(hose,r)&&!P::Link(hose,r));
    CHECK(hose->sprinkler==oldEnd&&oldEnd->hose==hose&&oldEnd->hoseKey==hose->key&&hose->generations==0);
    r=ResetLinks();tap->hose=otherHose;CHECK(!P::Link(hose,r)&&newEnd->hose==nullptr);
    r=ResetLinks();test::keys.erase(L"new_end");CHECK(!P::Link(hose,r)&&oldEnd->hose==hose);
    r=ResetLinks();test::keys.erase(L"tap");CHECK(!P::Link(hose,r));
    r=ResetLinks();test::fields=false;CHECK(!P::Link(hose,r));
    r=ResetLinks();oldEnd->hose=otherHose;oldEnd->hoseKey=L"other_hose";
    CHECK(P::Link(hose,r)&&oldEnd->hose==otherHose&&oldEnd->hoseKey==L"other_hose"); // never clear another owner
    r=ResetLinks();test::processOK=false;CHECK(!P::Link(hose,r)&&hose->generations==0&&newEnd->updates==0);
    r=ResetLinks();test::processCallback=&TravelOnFinish;
    CHECK(!P::Link(hose,r)&&newEnd->updates==0); // no post-callback write into a different world
    r=ResetLinks();test::processCallback=&RecycleOnFinish;
    CHECK(!P::Link(hose,r)&&newEnd->updates==0);
    r=ResetLinks();test::nameCallback=&TravelOnName;
    CHECK(!P::Link(hose,r)&&newEnd->hose==nullptr&&hose->generations==0);
    r=ResetLinks();test::captureOK=true;test::captured=r;hose->sprinkler=nullptr;
    test::captured.names[0][1]=L"old_end";Rec captured;
    CHECK(P::Capture(hose,captured)&&captured.names[0][1]==L"None"&&hose->sprinklerKey==L"None");
    tap->hose=nullptr;tap->hoseKey=L"old_hose";test::captured.className=L"faucet_C";test::captured.names={{L"old_hose"}};
    CHECK(P::Capture(tap,captured)&&captured.names[0][0]==L"None"&&tap->hoseKey==L"None");
    test::nameCallback=&TravelOnName;CHECK(!P::Capture(tap,captured));
    r=ResetLinks();oldEnd->key=L"old_end";
    CHECK(!P::DetachSprinkler(hose,L"new_end")&&test::detaches==0&&hose->sprinkler==oldEnd);
    CHECK(P::DetachSprinkler(hose,L"old_end")&&test::detaches==1&&!hose->sprinkler&&!oldEnd->hose);
    CHECK(P::DetachSprinkler(hose,L"old_end")&&test::detaches==1);
    CHECK(P::ToggleFaucet(tap)&&test::toggles==1&&!P::ToggleFaucet(hose));
    puts("PASS native hose wrapper: endpoint ownership, nullable/bidirectional saved links, old-owner cleanup, repeated geometry dispatch, missing metadata/dependencies, callback world/serial guards, stale optional-key capture");
}
