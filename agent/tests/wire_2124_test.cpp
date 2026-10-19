#include "coop/world/service_panel_rules.h"
#include "coop/interactables/kitchen_wire.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    using namespace coop;
    net::ServicePanelPayload p{};p.family=2;
    p.state[0]=6;p.state[1]=25;p.state[2]=1;p.state[13]=1; // 25th light
    CHECK(service_panel_rules::Valid(p));
    p.state[13]=2;CHECK(!service_panel_rules::Valid(p));
    p.state[13]=0;p.state[4]=3;CHECK(!service_panel_rules::Valid(p));
    p.state[4]=0;p.state[1]=17;CHECK(!service_panel_rules::Valid(p));
    for (unsigned before=0;before<16;++before) for (unsigned after=0;after<16;++after)
    for (unsigned current=0;current<16;++current) {
        p={};p.family=1;p.op=1;p.seq=1;p.before[0]=p.state[0]=15;
        p.before[1]=uint8_t(before);p.state[1]=uint8_t(after);p.action=uint8_t((before^after)<<4);
        CHECK(service_panel_rules::Valid(p)==(before!=after));
        uint8_t state[16]{};state[0]=7;state[1]=uint8_t(current);
        service_panel_rules::MergeDesk(p,state);
        unsigned expected=current;
        for (unsigned i=0;i<4;++i) if (((before^after)&(1u<<i)) && !((before^current)&(1u<<i)))
            expected=(expected&~(1u<<i))|(after&(1u<<i));
        CHECK(state[0]==7 && state[1]==expected); // independent bits, untouched covers
    }
    p={};p.family=1;p.op=1;p.seq=1;p.action=1;p.state[0]=1;
    CHECK(!service_panel_rules::Valid(p)); // client cannot fabricate a returned cover
    net::KitchenStatePayload k{};k.kind=1;
    ue_wrap::kitchen_machine::State s{};s.hasDoorPose=true;s.doorPose.RotZ=.6f;s.doorPose.RotW=.8f;s.doorPose.TX=18;
    kitchen_wire::EncodePose(s,k);CHECK(kitchen_wire::ValidPose(k));
    auto t=kitchen_wire::DecodePose(k);CHECK(t.RotZ==.6f && t.TX==18 && t.SZ==1);
    auto bad=k;bad.doorPose[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!kitchen_wire::ValidPose(bad));
    bad=k;bad.doorPose[3]=0;CHECK(!kitchen_wire::ValidPose(bad));
    bad=k;bad.doorPose[4]=501;CHECK(!kitchen_wire::ValidPose(bad));
    bad=k;bad.kind=2;CHECK(!kitchen_wire::ValidPose(bad));
    bad=k;bad.doorPose[7]=0;CHECK(!kitchen_wire::ValidPose(bad));
    bad=k;CHECK(kitchen_wire::SamePose(bad,k));bad.doorPose[2]+=.02f;CHECK(!kitchen_wire::SamePose(bad,k));
    std::puts("PASS: 4096 independent screw merges, 25-cell masks, bad operations and hinge pose validation");
}
