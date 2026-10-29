#define main ExistingHandFireMain
#include "hand_fire_2138_test.cpp"
#undef main
int main(){std::wstring cls;Reset();test::handPeople[1].playerNo=11;
    Hand(1,7);CHECK(H::RecentGenerationClass(1,7,cls) && cls==L"prop_C");
    Hand(1,8,false);CHECK(H::RecentGenerationClass(1,7,cls) && !H::RecentGenerationClass(1,8,cls));
    ++test::handPeople[1].playerNo;CHECK(!H::RecentGenerationClass(1,7,cls));--test::handPeople[1].playerNo;
    ++world;CHECK(!H::RecentGenerationClass(1,7,cls));--world;
    H::g_retired[1][0].at=H::Now()-1600;CHECK(!H::RecentGenerationClass(1,7,cls));
    Hand(1,9);++test::handPeople[1].playerNo;CHECK(!H::RecentGenerationClass(1,9,cls));Hand(1,10,false);
    CHECK(!H::RecentGenerationClass(1,9,cls));--test::handPeople[1].playerNo;
    Hand(1,11);++world;CHECK(!H::RecentGenerationClass(1,11,cls));Hand(1,12,false);CHECK(!H::RecentGenerationClass(1,11,cls));
    Reset();Hand(1,1);Hand(1,2);Hand(1,3);Hand(1,4);Hand(1,5);Hand(1,6);
    CHECK(!H::RecentGenerationClass(1,1,cls) && H::RecentGenerationClass(1,2,cls));
    H::OnSlotDisconnected(1);CHECK(!H::RecentGenerationClass(1,6,cls) && !H::RecentGenerationClass(1,5,cls));
    Hand(1,1);H::Reset();CHECK(!H::RecentGenerationClass(1,1,cls));
    CHECK(!H::RecentGenerationClass(255,1,cls) && !H::RecentGenerationClass(1,0,cls));
    puts("PASS actual hand generation: final-use class proof, four retired generations, expiry, world/person binding across stow, disconnect/reset");return 0;
}
