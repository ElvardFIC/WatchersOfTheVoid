#define main PlacedRecord2163Checks
#include "placed_record_2163_test.cpp"
#undef main
int main() {
    PlacedRecord2163Checks();
    namespace P=ue_wrap::placed_save;using R=ue_wrap::save_record::SaveRecord;
    R a,b;a.className=L"customWall_unfinished_C";a.key=L"mvpl_12345678_00000001";
    a.strings={{L""}};a.ints={{12,0,3,-1}};a.classes={{L"foreign_C"}};
    CHECK(P::Normalize(a,b)&&b.classes.empty()&&b.strings[0][0].empty());
    a.ints[0][0]=0;CHECK(!P::Normalize(a,b));a.ints[0][0]=12;
    a.ints[0][1]=-1;CHECK(!P::Normalize(a,b));a.ints[0][1]=5;
    a.ints[0][2]=4;CHECK(!P::Normalize(a,b));a.ints[0][2]=0;
    a.ints[0][3]=0;CHECK(!P::Normalize(a,b));a.ints[0][3]=-1;
    a.className=L"customWall_C";a.ints={{1,-1}};a.strings={{L"brick"}};a.floats={{17,200}};
    CHECK(P::Normalize(a,b)&&b.floats[0][0]==17);
    a.className=L"customWall_rad_C";CHECK(P::Normalize(a,b));
    a.className=L"customWall_wetConcrete_C";CHECK(P::Normalize(a,b));
    a.floats[0]={17,200,72,360};CHECK(P::Normalize(a,b)&&b.floats[0]==a.floats[0]);
    for(const auto& bad:std::vector<std::vector<float>>{{17,200,1},{17,200,-1,360},{17,200,1,0},{17,200,1,INFINITY}}){
        a.floats[0]=bad;CHECK(!P::Normalize(a,b));
    }
    a.className=L"customWall_C";a.floats[0]={17,200,72,360};CHECK(!P::Normalize(a,b));
    a.className=L"customWall_wetConcrete_C";a.floats[0]={17,200};
    a.floats[0][0]=std::numeric_limits<float>::quiet_NaN();CHECK(!P::Normalize(a,b));
    a.floats[0][0]=17;a.floats[0][1]=0;CHECK(!P::Normalize(a,b));
    a.className=L"customWall_unknown_C";CHECK(!P::Normalize(a,b));
    puts("PASS wall records: native schemas, progress/health, supported descendants, malformed geometry and fields");
}
