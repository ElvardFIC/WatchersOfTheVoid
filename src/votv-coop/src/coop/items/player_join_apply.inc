// The registered save-ready callback consumes one exact BeginJoinApply request.
// Keep profile/vitals/pose owned while native encoders can reenter the receiver.
void OnSaveObjectReady(void* saveSlotObject) {
    auto* s=g_session.load(std::memory_order_acquire);
    if(!s || !s->running() || !s->connected() || s->role()!=coop::net::Role::Client ||
       !saveSlotObject || !ue_wrap::reflection::IsLive(saveSlotObject))return;
    auto armed=g_joinApplyState.load(std::memory_order_acquire);
    if(!(armed&1) || !g_joinApplyState.compare_exchange_strong(armed,armed&~uint64_t{1},std::memory_order_acq_rel))return;
    const auto request=armed&~uint64_t{1};
    const auto epoch=g_streamEpoch,revision=g_pendingRevision;
    const auto world=ue_wrap::world_identity::Generation(),serial=coop::net::session_serial::Current();
    const bool haveProfile=g_hasPendingApply.load(std::memory_order_acquire);
    const coop::player_profile::Profile profile=haveProfile?g_pendingApply:coop::player_profile::Profile{};
    ue_wrap::CachedObjRef target;target.Set(saveSlotObject);
    const auto current=[&]{return g_session.load()==s && epoch==g_streamEpoch && g_joinApplyState.load(std::memory_order_acquire)==request &&
        revision==g_pendingRevision && haveProfile==g_hasPendingApply.load(std::memory_order_acquire) &&
        world==ue_wrap::world_identity::Generation() && serial==coop::net::session_serial::Current() &&
        target.Get()==saveSlotObject && s->running() && s->connected() && s->role()==coop::net::Role::Client;};
    if(!current())return;
    const bool nativeMap = ((armed >> 1) & 7) == ue_wrap::game_mode::kTutorial;
    g_profileApplied=false;g_joinPose={};
    g_joinPlacement=nativeMap?JoinPlacement::NativeMap:JoinPlacement::StartPoint;
    bool gamemodeSet=false;
    const int effectsReset=ue_wrap::effects::ResetOnSaveObject(saveSlotObject,&gamemodeSet);
    if(!current())return;
    const auto initialPlacement=nativeMap?JoinPlacement::NativeMap:
        (JoinStaysAtHost()?JoinPlacement::AtHost:JoinPlacement::StartPoint);
    const bool itemsApplied=ue_wrap::inventory::ApplyToSaveObjectWhile(saveSlotObject,profile.items,current);
    if(!current())return;
    if(!itemsApplied){UE_LOGW("player_inventory[client]: inventory restore refused on %p; profile reporting stays closed",saveSlotObject);return;}
    ue_wrap::vitals::Snapshot vitals=profile.vitals;
    const bool alive=haveProfile && profile.hasVitals && std::isfinite(vitals.health) && std::isfinite(vitals.maxHealth) &&
        std::isfinite(vitals.food) && std::isfinite(vitals.sleep) && vitals.health>0.f;
    bool defaults=alive;
    if(!defaults){defaults=ue_wrap::vitals::ReadDefaults(vitals);if(!current())return;}
    bool vitalsOk=false;
    if(defaults){vitalsOk=ue_wrap::vitals::ApplySnapshot(saveSlotObject,vitals);if(!current())return;}
    auto placement=initialPlacement;coop::player_profile::Pose pose{};
    const auto inWorld=[](float n){return std::isfinite(n) && std::fabs(n)<=coop::net::kMaxCoord;};
    if(alive && profile.pose.valid && inWorld(profile.pose.x) && inWorld(profile.pose.y) && inWorld(profile.pose.z) && std::isfinite(profile.pose.yaw))pose=profile.pose;
    if(placement==JoinPlacement::StartPoint && pose.valid)placement=JoinPlacement::ProfilePose;
    bool placed=true;
    namespace N=ue_wrap::profile::name;
    if(placement==JoinPlacement::ProfilePose)placed=ue_wrap::vitals::WritePlayerTransform(saveSlotObject,pose.x,pose.y,pose.z,pose.yaw);
    else if(placement==JoinPlacement::StartPoint)placed=ue_wrap::vitals::WritePlayerTransform(saveSlotObject,N::kKPPSpawnX,N::kKPPSpawnY,N::kKPPSpawnZ,0.f);
    if(!current())return;
    g_joinPose=pose;g_joinPlacement=placement;
    g_profileApplied=haveProfile && effectsReset>=0 && vitalsOk && placed;
    UE_LOGI("player_inventory[client]: join request applied to %p profile=%d effects=%d vitals=%d pose=%d stream=%d",
        saveSlotObject,haveProfile?1:0,effectsReset,vitalsOk?1:0,placed?1:0,g_profileApplied?1:0);
}
