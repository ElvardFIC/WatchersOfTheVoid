#define PROFILE_JOIN_HARNESS
#define main profile_baseline_main
#include "profile_receive_2169_test.cpp"
#undef main
namespace J=coop::player_inventory_sync;
namespace join_test {
using Profile=coop::player_profile::Profile;
using Items=ue_wrap::inventory::PlayerInventory;
Profile profile;std::vector<uint8_t> hostBlob;
bool inventoryOK=true,defaultsOK=true,vitalsOK=true,poseOK=true;
int effectsResult=0,effects=0,inventories=0,defaults=0,vitals=0,poses=0,gets=0,quarantines=0;
ue_wrap::vitals::Snapshot appliedVitals;
coop::player_profile::Pose appliedPose;
Items appliedItems;
std::function<void()> onEffects,onInventory,onDefaults,onApplyVitals,onPose,onGet;
coop::player_profile_store::Found found=coop::player_profile_store::Found::Yes;
void Callback(std::function<void()>& fn){auto once=std::move(fn);fn={};if(once)once();}
Profile Make(int units=5) {
    Profile p;CHECK(coop::inventory_wire::Deserialize(test::Blob(units),p));
    p.hasVitals=true;p.vitals.health=25;p.vitals.maxHealth=100;p.vitals.food=50;p.vitals.sleep=60;
    p.pose={100,200,300,40,true};return p;
}
void Reset(coop::net::Role role=coop::net::Role::Client) {
    test::Reset();test::sessionSerial=1;test::session.mode=role;profile=Make();
    inventoryOK=defaultsOK=vitalsOK=poseOK=true;effectsResult=0;effects=inventories=defaults=vitals=poses=gets=quarantines=0;
    onEffects=onInventory=onDefaults=onApplyVitals=onPose=onGet={};appliedVitals={};appliedPose={};appliedItems={};
    hostBlob=coop::inventory_wire::Serialize(profile);found=coop::player_profile_store::Found::Yes;
    test::session.slots={false,true,false,false};
}
void Offer(int units=5,uint32_t seq=1){profile=Make(units);test::Send(coop::inventory_wire::Serialize(profile),seq,0);}
void Apply(){J::BeginJoinApply();J::OnSaveObjectReady(&test::liveUnits);}
void HostTick(){J::g_lastSweep={};J::HostPersistTick(&test::session);}
}
namespace ue_wrap::inventory {
bool ApplyToSaveObjectWhile(void*,const PlayerInventory& items,const std::function<bool()>& current) {
    CHECK(current());++join_test::inventories;
    const PlayerInventory owned=items;join_test::Callback(join_test::onInventory);
    if(!join_test::inventoryOK||!current())return false;join_test::appliedItems=owned;return true;
}
bool ApplyToSaveObject(void* save,const PlayerInventory& items){return ApplyToSaveObjectWhile(save,items,[]{return true;});}
}
namespace ue_wrap::vitals {
bool ReadDefaults(Snapshot& out){++join_test::defaults;out={};out.health=100;out.maxHealth=100;out.food=75;out.sleep=90;join_test::Callback(join_test::onDefaults);return join_test::defaultsOK;}
bool ApplySnapshot(void*,const Snapshot& value){++join_test::vitals;join_test::appliedVitals=value;join_test::Callback(join_test::onApplyVitals);return join_test::vitalsOK;}
bool WritePlayerTransform(void*,float x,float y,float z,float yaw){++join_test::poses;join_test::appliedPose={x,y,z,yaw,true};join_test::Callback(join_test::onPose);return join_test::poseOK;}
}
namespace ue_wrap::effects {int ResetOnSaveObject(void*,bool* set){++join_test::effects;if(set)*set=true;join_test::Callback(join_test::onEffects);return join_test::effectsResult;}}
namespace coop::prop_synth_key {std::wstring RandomKeyString(){static int n=0;return L"new_"+std::to_wstring(++n);}}
namespace coop::player_profile_store {
Found Get(const std::string&,std::vector<uint8_t>& out){++join_test::gets;out=join_test::hostBlob;join_test::Callback(join_test::onGet);return join_test::found;}
void Quarantine(const std::string&){++join_test::quarantines;}
}
int main(){using namespace join_test;
    // The mode comes from the transfer, before the local GI leaves the menu.
    // Tutorial must retain the level/host placement even with a valid old KPP pose.
    for(bool haveProfile:{false,true}) {
        Reset();if(haveProfile)Offer();
        J::BeginJoinApply(ue_wrap::game_mode::kTutorial);J::OnSaveObjectReady(&test::liveUnits);
        CHECK(poses==0&&J::g_joinPlacement==J::JoinPlacement::NativeMap);
        CHECK(J::g_profileApplied==haveProfile);
        float x=0,y=0,z=0,yaw=0;
        CHECK(J::TakeJoinPlacement(x,y,z,yaw)==J::JoinPlacement::NativeMap);
        CHECK(J::TakeJoinPlacement(x,y,z,yaw)==J::JoinPlacement::NativeMap); // respawn too
    }
    for(int mode:{0,1,3,4,5,6,7,-1,8}) {
        Reset();Offer();J::BeginJoinApply(mode);J::OnSaveObjectReady(&test::liveUnits);
        CHECK(poses==1&&J::g_joinPlacement==J::JoinPlacement::ProfilePose);
    }
    Reset();Offer();onEffects=[]{J::BeginJoinApply(ue_wrap::game_mode::kTutorial);};
    Apply();CHECK(!J::g_profileApplied&&poses==0);
    J::OnSaveObjectReady(&test::liveUnits);CHECK(J::g_profileApplied&&poses==0&&J::g_joinPlacement==J::JoinPlacement::NativeMap);
    puts("PASS Tutorial join: request-owned host mode preserves native placement with missing/old profiles, reentry and respawn; other modes retain their profile placement");
    Reset();Offer();Apply();CHECK(J::g_profileApplied&&effects==1&&inventories==1&&vitals==1&&poses==1&&appliedItems.hold[0].data.ints[0][0]==5&&appliedVitals.health==25&&appliedPose.x==100);
    J::OnSaveObjectReady(&test::liveUnits);CHECK(effects==1);
    Reset(coop::net::Role::Host);Apply();CHECK(effects==0&&!J::g_profileApplied);
    Reset();Apply();CHECK(!J::g_profileApplied&&appliedItems.hold.empty()&&defaults==1&&appliedVitals.health==100&&J::g_joinPlacement==J::JoinPlacement::StartPoint);
    Reset();profile=Make();profile.vitals.health=0;test::Send(coop::inventory_wire::Serialize(profile),1,0);Apply();CHECK(J::g_profileApplied&&defaults==1&&appliedVitals.health==100&&J::g_joinPlacement==J::JoinPlacement::StartPoint);
    for(int failure=0;failure<4;++failure){Reset();Offer();if(failure==0)effectsResult=-1;if(failure==1)inventoryOK=false;if(failure==2)vitalsOK=false;if(failure==3)poseOK=false;Apply();CHECK(!J::g_profileApplied);}
    puts("PASS real join callback: one-shot/client-only requests, own profile/vitals/pose, dead/missing-profile defaults and no false stream admission after partial native restore");
    for(int stage=0;stage<5;++stage)for(int change=0;change<7;++change){
        Reset();profile=Make();if(stage==2)profile.hasVitals=false;test::Send(coop::inventory_wire::Serialize(profile),1,0);
        auto callback=[change]{if(change==0)J::OnDisconnect();if(change==1)++test::world;if(change==2)++test::sessionSerial;if(change==3)++test::pawnSerial;if(change==4)test::session.mode=coop::net::Role::Host;if(change==5)test::session.started=false;if(change==6)J::BeginJoinApply();};
        if(stage==0)onEffects=callback;if(stage==1)onInventory=callback;if(stage==2)onDefaults=callback;if(stage==3)onApplyVitals=callback;if(stage==4)onPose=callback;
        Apply();CHECK(!J::g_profileApplied);
        if(stage==0)CHECK(inventories==0);if(stage==1)CHECK(defaults==0&&vitals==0&&poses==0);if(stage==2)CHECK(vitals==0&&poses==0);if(stage==3)CHECK(poses==0);
        if(change==6)CHECK(J::g_joinApplyState.load()&1);
    }
    for(int stage=0;stage<3;++stage){Reset();Offer();auto callback=[]{Offer(9,2);};if(stage==0)onEffects=callback;if(stage==1)onInventory=callback;if(stage==2)onApplyVitals=callback;Apply();CHECK(!J::g_profileApplied&&J::g_pendingApply.items.hold[0].data.ints[0][0]==9&&J::HasPendingApply());if(stage==0)CHECK(inventories==0);if(stage<2)CHECK(vitals==0);CHECK(poses==0);}
    Reset();Offer();onEffects=[]{Offer(9,2);J::BeginJoinApply();J::OnSaveObjectReady(&test::liveUnits);};Apply();CHECK(J::g_profileApplied&&appliedItems.hold[0].data.ints[0][0]==9&&inventories==1);
    puts("PASS join lifetime and revision: each native callback cancels old world/session/save/request work; newer profile and nested join remain coherent");

    Reset(coop::net::Role::Host);CHECK(J::SendInventoryToSlot(1)&&test::session.sent.size()>1);
    for(int change=0;change<7;++change){Reset(coop::net::Role::Host);onGet=[change]{if(change==0)J::OnDisconnect();if(change==1)++test::world;if(change==2)++test::sessionSerial;if(change==3)++test::session.generations[1];if(change==4)test::guids[1]="other";if(change==5)test::session.slots[1]=false;if(change==6)J::OnDisconnectForSlot(1);};CHECK(!J::SendInventoryToSlot(1)&&test::session.sent.empty());}
    for(int change=0;change<6;++change){Reset(coop::net::Role::Host);test::session.onSend=[change]{if(change==0)J::OnDisconnect();if(change==1)++test::world;if(change==2)++test::session.generations[1];if(change==3)test::guids[1]="other";if(change==4)J::OnDisconnectForSlot(1);if(change==5)test::session.slots[1]=false;};CHECK(!J::SendInventoryToSlot(1)&&!test::session.sent.empty()&&test::session.sent.size()==test::session.sent.front().chunks);}
    Reset(coop::net::Role::Host);onGet=[]{CHECK(J::SendInventoryToSlot(1));};CHECK(!J::SendInventoryToSlot(1));CHECK(test::session.sent.size()>1);for(const auto& p:test::session.sent)CHECK(p.blobSeq==J::g_hostSendSeq[1]);
    puts("PASS host join send: owned GUID and exact source/world/recipient/sequence guards stop stale preparation and retired completion after immutable batch admission");
    Reset(coop::net::Role::Host);HostTick();CHECK(J::g_applySentToSlot[1]);const int before=gets;HostTick();CHECK(gets==before);
    ++test::session.generations[1];HostTick();CHECK(gets==before+1&&J::g_applySentToSlot[1]);
    test::guids[1]="new_guid";HostTick();CHECK(gets==before+2&&J::g_applySentToSlot[1]);
    Reset(coop::net::Role::Host);test::session.sendOk=false;HostTick();CHECK(!J::g_applySentToSlot[1]);test::session.sendOk=true;HostTick();CHECK(J::g_applySentToSlot[1]);
    Reset(coop::net::Role::Host);test::session.onSend=[]{++test::session.generations[1];};HostTick();CHECK(!J::g_applySentToSlot[1]);HostTick();CHECK(J::g_applySentToSlot[1]);
    Reset(coop::net::Role::Host);test::session.onSend=[]{J::OnDisconnectForSlot(1);};HostTick();CHECK(!J::g_applySentToSlot[1]);HostTick();CHECK(J::g_applySentToSlot[1]);
    Reset(coop::net::Role::Host);test::session.slots[2]=true;test::session.onSend=[]{++test::world;};HostTick();
    CHECK(!test::session.sent.empty() && test::session.sentSlots==std::vector<int>(test::session.sent.front().chunks,1) && !J::g_applySentToSlot[2]);
    puts("PASS occupancy-bound sent latch: silent same-GUID/new-GUID replacement, refused sends and disconnect during send keep the correct profile owed");
    return 0;
}
