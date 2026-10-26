#include "../../src/votv-coop/src/ue_wrap/actors/placed_save_record.cpp"
#include "llama_marker_boundary_stubs.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    namespace P=ue_wrap::placed_save;using R=ue_wrap::save_record::SaveRecord;
    R a,b;a.className=L"faucet_C";a.key=L"mvpl_12345678_00000001";a.names={{L"None"}};a.bools={{1}};
    a.classes={{L"ArbitrarySpawn_C"}};a.strings={{L"unrelated"}};
    CHECK(P::Normalize(a,b)&&b.classes.empty()&&b.strings.empty()&&b.bools[0][0]==1);
    a.xform[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!P::Normalize(a,b));a.xform[0]=0;
    a.xform[3]=0;CHECK(!P::Normalize(a,b));a.xform[3]=1;
    a.xform[7]=-1;CHECK(!P::Normalize(a,b));a.xform[7]=1;
    a.className=L"actor_save_C";CHECK(!P::Normalize(a,b));a.className=L"groundHose_C";
    a.names={{L"tap",L"sprinkler"}};a.vectors={{{0,0,0},{1,2,3}}};CHECK(P::Normalize(a,b)&&b.bools.empty()&&b.vectors[0].size()==2);
    a.vectors[0].resize(4097);CHECK(!P::Normalize(a,b));a.vectors[0].resize(1);CHECK(!P::Normalize(a,b));
    a.className=L"growingPlant_shroom_0_C";a.names={{},{L"shroom",L"pot"}};a.floats={{},{.4f,.2f,.8f,0,0,.1f}};
    CHECK(P::Normalize(a,b)&&b.names[1][1]==L"pot"&&b.floats[1][2]==.8f&&b.vectors.empty());
    a.floats[1].pop_back();CHECK(!P::Normalize(a,b));
    puts("PASS placed record: class/geometry bounds, family schemas, hose points, plant/pot state, unrelated fields removed");
    return 0;
}
