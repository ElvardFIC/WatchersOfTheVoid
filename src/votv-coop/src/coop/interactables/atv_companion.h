// Private host companion lifecycle. Included inside atv_sync's private namespace.
#pragma once
struct CompanionScope {
    coop::net::Session* session=nullptr;uint32_t serial=0,generation=0;void* world=nullptr;bool active=false;
    bool operator==(const CompanionScope&)const=default;
};
CompanionScope CompanionNow(){
    CompanionScope s;s.session=LaneSession();s.serial=coop::net::session_serial::Current();
    s.generation=ue_wrap::world_identity::Generation();s.world=ue_wrap::world_identity::CurrentWorld();
    s.active=s.session&&s.session->running()&&s.session->connected()&&s.session->role()==coop::net::Role::Host&&s.world;
    return s;
}
bool CompanionCurrent(const CompanionScope& s){return s.active&&s==CompanionNow();}
struct CompanionActor {
    ue_wrap::CachedObjRef body,cls;int32_t serial=0;
    void Set(void* actor){body.Set(actor);cls.Set(actor?R::ClassOf(actor):nullptr);serial=actor?R::SlotSerial(body.Idx()):0;}
    bool Current(const CompanionScope& s)const{return body.Get()&&cls.Get()&&s.world&&
        s.generation==ue_wrap::world_identity::Generation()&&s.world==ue_wrap::world_identity::CurrentWorld()&&
        ue_wrap::world_identity::WorldOf(body.Get())==s.world&&R::ClassOf(body.Get())==cls.Get()&&A::IsAtv(body.Get());}
    bool Matches(const AtvEntry& e,const CompanionScope& s)const{return e.actor==body.Raw()&&e.idx==body.Idx()&&e.world==s.world&&R::SlotSerial(body.Idx())==serial;}
};
struct CompanionState {
    CompanionScope scope;CompanionActor actor,source;
    std::wstring key;FVector location{};FRotator rotation{};
    uint64_t revision=0,stillAt=0,searchAt=0,savedAt=0,mintedAt=0,announceAt=0,lastNow=0;
    bool waiting=false,searched=false,savedWaiting=false,cleanup=false,announceDue=false,announcedOnce=false;
};
CompanionState g_companion;
uint64_t g_companionRevision=0;
bool g_companionBusy=false;
bool CompanionKey(const std::wstring& key){
    if(key.empty()||key==L"None"||key.size()>31)return false;
    for(wchar_t c:key)if(c<33||c>126)return false;
    return true;
}
bool CompanionPose(const FVector& p,const FRotator& r){
    for(float v:{p.X,p.Y,p.Z,r.Pitch,r.Yaw,r.Roll})if(!std::isfinite(v))return false;
    return true;
}
bool CompanionMoved(const FVector& p,const FRotator& r,const FVector& old,const FRotator& facing){
    const double x=double(p.X)-old.X,y=double(p.Y)-old.Y,z=double(p.Z)-old.Z;
    return x*x+y*y+z*z>4.0||std::fabs(ue_wrap::NormalizeAxis(r.Pitch-facing.Pitch))>1.f||
        std::fabs(ue_wrap::NormalizeAxis(r.Yaw-facing.Yaw))>1.f||std::fabs(ue_wrap::NormalizeAxis(r.Roll-facing.Roll))>1.f;
}
void ResetCompanion(bool cleanup){
    const auto old=g_companion;g_companion={};g_companion.revision=++g_companionRevision;
    if(cleanup&&old.key.empty()&&old.scope.session==LaneSession()&&old.scope.serial==coop::net::session_serial::Current()&&
       old.actor.Current(old.scope))A::DestroyMirror(old.actor.body.Get());
}
void SendAtvDestroy(const std::wstring& key){
    auto* s=LaneSession();if(!s||!s->running()||!s->connected()||s->role()!=coop::net::Role::Host||!CompanionKey(key))return;
    coop::net::AtvDestroyPayload p{};WireKeyFromString(key,p.key);
    s->SendReliable(coop::net::ReliableKind::AtvDestroy,&p,sizeof(p));
}
int LiveAtvCount(){return A::CountLiveSettled();}
void ResetCompanionStill(){g_companion.source={};g_companion.waiting=false;g_companion.savedWaiting=false;}
bool RemoveMintedCompanion(){
    const auto captured=g_companion;const auto revision=captured.revision;
    if(!captured.actor.Current(captured.scope)){
        if(g_companion.revision==revision)ResetCompanion(false);
        return true;
    }
    g_companion.cleanup=true;
    const auto found=g_atvs.find(captured.key);
    const bool owned=found!=g_atvs.end()&&captured.actor.Matches(found->second,captured.scope);
    if(!A::DestroyMirror(captured.actor.body.Get()))return false;
    if(g_companion.revision!=revision||!CompanionCurrent(captured.scope))return true;
    const auto after=g_atvs.find(captured.key);
    const bool erase=owned&&after!=g_atvs.end()&&captured.actor.Matches(after->second,captured.scope);
    if(erase)g_atvs.erase(after);
    ResetCompanion(false);
    if(erase)SendAtvDestroy(captured.key);
    return true;
}
void RetryCompanionAnnouncement(uint64_t now){
    if(!g_companion.announceDue||!CompanionKey(g_companion.key)||!g_companion.actor.Current(g_companion.scope))return;
    if(g_companion.announcedOnce&&now>=g_companion.announceAt&&now-g_companion.announceAt<250)return;
    const auto captured=g_companion;g_companion.announceAt=now;g_companion.announcedOnce=true;
    const bool sent=SendAtvSpawn(captured.key,captured.actor.body.Get(),-1);
    if(g_companion.revision==captured.revision&&CompanionCurrent(captured.scope)&&captured.actor.Current(captured.scope)&&g_companion.key==captured.key&&sent)
        g_companion.announceDue=false;
}
bool PublishCompanion(void* actor){
    const auto captured=g_companion;const auto revision=captured.revision;
    const auto current=[&]{return g_companion.revision==revision&&CompanionCurrent(captured.scope)&&captured.actor.body.Is(actor)&&captured.actor.Current(captured.scope);};
    if(!current())return false;
    const std::wstring key=A::EnsureOwnSaveKey(actor);
    if(!current()||!CompanionKey(key)||A::GetKeyString(actor)!=key||!current())return false;
    auto found=g_atvs.find(key);
    if(found!=g_atvs.end()&&!captured.actor.Matches(found->second,captured.scope))return false;
    if(found==g_atvs.end()){
        AtvEntry entry{};entry.actor=actor;entry.idx=captured.actor.body.Idx();entry.world=captured.scope.world;
        g_atvs.emplace(key,std::move(entry));
    }
    g_companion.key=key;g_companion.announceDue=true;g_companion.announcedOnce=false;
    RetryCompanionAnnouncement(g_companion.lastNow);
    return current();
}
bool RebindCompanionKey(const std::wstring& from,const std::wstring& to){
    const auto captured=g_companion;const auto revision=captured.revision;
    const auto current=[&]{return g_companion.revision==revision&&CompanionCurrent(captured.scope)&&captured.actor.Current(captured.scope);};
    if(!current()||!CompanionKey(to)||from==to)return false;
    auto old=g_atvs.find(from);auto target=g_atvs.find(to);
    if(old==g_atvs.end()||!captured.actor.Matches(old->second,captured.scope)||
       (target!=g_atvs.end()&&!captured.actor.Matches(target->second,captured.scope)))return false;
    if(!A::WriteSaveKey(captured.actor.body.Get(),to.c_str())||!current()||A::GetKeyString(captured.actor.body.Get())!=to||!current())return false;
    old=g_atvs.find(from);target=g_atvs.find(to);
    if(old==g_atvs.end()||!captured.actor.Matches(old->second,captured.scope)||
       (target!=g_atvs.end()&&!captured.actor.Matches(target->second,captured.scope)))return false;
    const auto entry=old->second;
    g_atvs[to]=entry;g_atvs.erase(from);g_companion.key=to;
    g_companion.announceDue=true;g_companion.announcedOnce=false;
    SendAtvDestroy(from);
    if(current())RetryCompanionAnnouncement(g_companion.lastNow);
    return current();
}
void RepairCompanionKeyCollision(){
    const auto captured=g_companion;
    const auto current=[&]{return g_companion.revision==captured.revision&&CompanionCurrent(captured.scope)&&captured.actor.Current(captured.scope);};
    if(captured.key.empty()||!current())return;
    const auto mine=A::GetKeyString(captured.actor.body.Get());
    if(!current()||!CompanionKey(mine))return;
    const bool collision=A::FindOtherLiveByKey(mine,captured.actor.body.Get())!=nullptr;
    if(!current()||!collision)return;
    const int count=LiveAtvCount();if(!current()||count<0)return;
    if(count<=2&&mine!=L"coopatv2"&&!A::FindOtherLiveByKey(L"coopatv2",captured.actor.body.Get())&&current()&&
       !A::LoadedSaveNamesKey(L"coopatv2")&&current()&&RebindCompanionKey(captured.key,L"coopatv2"))return;
    if(current())RemoveMintedCompanion();
}
void WatchMintedCompanion(uint64_t now){
    const auto captured=g_companion;
    const auto current=[&]{return g_companion.revision==captured.revision&&CompanionCurrent(captured.scope)&&captured.actor.Current(captured.scope);};
    if(!current()){if(g_companion.revision==captured.revision)ResetCompanion(false);return;}
    if(captured.cleanup){RemoveMintedCompanion();return;}
    const int count=LiveAtvCount();if(!current()||count<0)return;
    if(count>2){RemoveMintedCompanion();return;}
    const int saved=A::LoadedSaveAtvCount();if(!current())return;
    // Give an incoming saved body a bounded window; a stale record cannot stall publication forever.
    if(saved>count-1&&now>=captured.mintedAt&&now-captured.mintedAt<2000)return;
    if(captured.key.empty()&&!PublishCompanion(captured.actor.body.Get())){
        if(current())RemoveMintedCompanion();return;
    }
    RetryCompanionAnnouncement(now);
}
void TrySpawnCompanion(uint64_t now){
    if(g_companionBusy)return;
    g_companionBusy=true;struct Exit{~Exit(){g_companionBusy=false;}}exit;
    const auto scope=CompanionNow();
    if(!(g_companion.scope==scope)){ResetCompanion(false);g_companion.scope=scope;}
    if(!scope.active)return;
    if(now<g_companion.lastNow){ResetCompanionStill();g_companion.searched=false;g_companion.announcedOnce=false;}
    g_companion.lastNow=now;
    if(g_companion.actor.body.Raw()){
        WatchMintedCompanion(now);RepairCompanionKeyCollision();return;
    }
    const auto revision=g_companion.revision;
    const auto current=[&]{return g_companion.revision==revision&&CompanionCurrent(scope);};
    const int count=LiveAtvCount();if(!current())return;
    if(count!=1){ResetCompanionStill();return;}
    // Snapshot candidates before any callback-capable transform read. Persisted coop keys are valid sources.
    std::vector<CompanionActor> candidates;
    for(const auto& row:g_atvs){
        if(!row.second.actor||!R::IsLiveByIndex(row.second.actor,row.second.idx))continue;
        CompanionActor actor;actor.Set(row.second.actor);if(actor.Current(scope))candidates.push_back(actor);
    }
    CompanionActor source;FVector location{};FRotator rotation{};
    for(const auto& candidate:candidates){
        if(!current())return;
        if(candidate.Current(scope)&&A::GetRootTransform(candidate.body.Get(),location,rotation)&&
           candidate.Current(scope)&&current()&&CompanionPose(location,rotation)){source=candidate;break;}
    }
    if(!source.Current(scope)){ResetCompanionStill();return;}
    const auto sourceCurrent=[&]{return current()&&source.Current(scope);};
    const bool driven=A::IsDriven(source.body.Get());if(!sourceCurrent())return;
    const bool carried=IsLocalGrabber(source.body.Get(),coop::players::Registry::Get().Local());if(!sourceCurrent())return;
    if(driven||carried){ResetCompanionStill();return;}
    if(!g_companion.waiting||!g_companion.source.body.Is(source.body.Get())||
       CompanionMoved(location,rotation,g_companion.location,g_companion.rotation)){
        g_companion.source=source;g_companion.location=location;g_companion.rotation=rotation;
        g_companion.stillAt=now;g_companion.waiting=true;return;
    }
    if(now<g_companion.stillAt||now-g_companion.stillAt<2000)return;
    const int saved=A::LoadedSaveAtvCount();if(!sourceCurrent())return;
    if(saved>=2){
        if(!g_companion.savedWaiting){g_companion.savedWaiting=true;g_companion.savedAt=now;return;}
        if(now<g_companion.savedAt||now-g_companion.savedAt<5000)return;
    }else g_companion.savedWaiting=false;
    if(g_companion.searched&&now>=g_companion.searchAt&&now-g_companion.searchAt<2000)return;
    g_companion.searchAt=now;g_companion.searched=true;
    FVector spawn{};
    if(!A::FindCompanionLocation(source.body.Get(),spawn)||!sourceCurrent()||!CompanionPose(spawn,rotation))return;
    if(LiveAtvCount()!=1||!sourceCurrent())return;
    if(A::IsDriven(source.body.Get())||!sourceCurrent()||IsLocalGrabber(source.body.Get(),coop::players::Registry::Get().Local())||!sourceCurrent())return;
    FVector confirmed{};FRotator facing{};
    if(!A::GetRootTransform(source.body.Get(),confirmed,facing)||!sourceCurrent()||!CompanionPose(confirmed,facing)||
       CompanionMoved(confirmed,facing,location,rotation)){ResetCompanionStill();return;}
    void* spawned=A::SpawnMirror(L"ATV_C",spawn,rotation);
    CompanionActor minted;
    if(spawned&&R::IsLive(spawned))minted.Set(spawned);
    if(!minted.Current(scope))return; // never clean up a foreign return
    if(!sourceCurrent()){if(minted.Current(scope))A::DestroyMirror(minted.body.Get());return;}
    g_companion.actor=minted;g_companion.mintedAt=now;g_companion.revision=++g_companionRevision;
    const int after=LiveAtvCount();
    if(g_companion.actor.body.Is(spawned)&&CompanionCurrent(scope)&&after>2)RemoveMintedCompanion();
}
