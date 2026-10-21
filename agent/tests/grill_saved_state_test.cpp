#define main PriorDryingMain
#include "wall_drying_test.cpp"
#undef main
int main(){
    using namespace test;namespace SR=ue_wrap::save_record;namespace G=ue_wrap::grill_saved_state;
    actors[0].index=0;actors[1].index=1;auto& actor=actors[0];actor.cls=L"prop_bbq_C";actor.burn=.65f;actor.active=0xA4;
    alignas(16) uint8_t record[SR::kSaveStride]{};
    CHECK(SR::ReplaceFloatGroup(record,0,{12,34}));CHECK(G::SaveNative(&actor,record));
    std::vector<float> base,extension;CHECK(SR::ReadFloatGroup(record,0,base) && base==std::vector<float>({12,34}));
    CHECK(SR::ReadFloatGroup(record,1,extension) && extension==std::vector<float>({43217,1,.65f,1}));
    actor.burn=0;actor.active=0xA0;CHECK(G::LoadNative(&actor,record) && actor.burn==.65f && actor.active==0xA4);
    SR::SaveRecord held;held.className=L"prop_bbq_C";held.floats={{12,34}};held.ints={{6}};held.bools={{1,0,1},{1}};
    actor.burn=.9f;actor.active=0xA0;CHECK(G::RefreshHeldRecord(&actor,held) && held.floats[1][2]==.9f && held.floats[1][3]==0);
    CHECK(held.ints[0][0]==6 && held.bools[1][0]==1 && held.floats[0][0]==12);
    held.ints[0].push_back(91);held.bools[0].push_back(0);held.bools[1].push_back(1);
    CHECK(G::PreserveHostRecord(&actor,held) && held.ints[0]==std::vector<int32_t>({7,91}) && held.bools[1]==std::vector<uint8_t>({1,1}));
    CHECK(held.floats[0][0]==12 && held.bools[0]==std::vector<uint8_t>({1,0,1,0}));
    auto missing=held;missing.ints.clear();missing.bools.clear();CHECK(G::PreserveHostRecord(&actor,missing) && missing.ints[0][0]==7 && missing.bools[1][0]==1);
    held.className=L"other_C";CHECK(!G::RefreshHeldRecord(&actor,held));held.className=L"prop_bbq_C";
    for(int cause=0;cause<5;++cause){auto bad=extension;if(cause==0)bad[0]=7;if(cause==1)bad[1]=2;
        if(cause==2)bad[2]=NAN;if(cause==3)bad[2]=1.02f;if(cause==4)bad[3]=.5f;
        CHECK(SR::ReplaceFloatGroup(record,1,bad));actor.burn=.5f;CHECK(!G::LoadNative(&actor,record) && actor.burn==.5f);
        CHECK(!G::SaveNative(&actor,record));}
    CHECK(SR::ReplaceFloatGroup(record,1,{}));actor.burn=.3f;CHECK(G::LoadNative(&actor,record) && actor.burn==.3f);
    CHECK(G::SaveNative(&actor,record));fields=false;CHECK(!G::SaveNative(&actor,record) && !G::LoadNative(&actor,record));fields=true;
    inOffset=[]{++actors[0].serial;};CHECK(!G::SaveNative(&actor,record));
    inOffset=[]{++world;};CHECK(!G::LoadNative(&actor,record));
    actor.burn=-.1f;CHECK(!G::SaveNative(&actor,record));actor.burn=.2f;
    allocation=false;CHECK(!G::SaveNative(&actor,record));allocation=true;
    ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&actors[1];out=record;
    coop::material_save_sync::Saved(call);CHECK(SR::ReadFloatGroup(record,1,extension) && extension[2]==.2f);
    actor.burn=0;coop::material_save_sync::Loaded(call);CHECK(actor.burn==.2f);
    for(void* buffer:buffers)std::free(buffer);buffers.clear();
    puts("PASS grill persistence: actual native scalar codec, versioned burn/active extension, packed flag preservation, old saves, unknown/corrupt refusal, hand record refresh and native hook/serial/world/allocation fences");
}
