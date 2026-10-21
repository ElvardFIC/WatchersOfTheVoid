struct BucketLocal {Launch packet{};CachedObjRef source;uint64_t expires=0,next=0;bool hand=false;};
struct BucketHost {Launch packet{};uint64_t expires=0,peerEpoch=0;};
std::deque<BucketLocal> g_bucketLocal;
std::deque<BucketHost> g_bucketHost;
uint32_t g_bucketSequence=0;
bool g_bucketHooksReady=false;
CachedObjRef g_bucketSoundAt,g_bucketSound2D;
std::array<uint32_t,net::kMaxPeers> g_bucketHigh{};
std::array<std::bitset<256>,net::kMaxPeers> g_bucketSeen{};
std::array<uint64_t,net::kMaxPeers> g_bucketRateAt{};
std::array<unsigned,net::kMaxPeers> g_bucketRate{};
bool CuePoint(const Launch& p){
    if(p.kind>2 || p.op>1 || p.reserved[0] || p.reserved[1] || p.reserved[2])return false;
    for(float v:{p.x,p.y,p.z})if(!std::isfinite(v) || std::fabs(v)>1.e7f)return false;
    return true;
}
void CanonicalBucketSound(Launch p,int owner,bool hostSound){
    if(!Current() || !Host() || !CuePoint(p) || g_cues.size()>=kMaxCues)return;
    Cue cue{};p.op=1;p.sequence=Next(g_splashSequence);cue.launch=p;cue.bucket=cue.surface=true;cue.sound=!hostSound;
    cue.expires=active_drive::NowMs()+1500;
    for(int slot=1;slot<net::kMaxPeers;++slot)if(slot!=owner && g_session->IsSlotWorldReady(slot)){
        cue.pending|=1u<<slot;cue.people[slot]=roster_ledger::Get(slot).playerNo;cue.epochs[slot]=g_peerEpoch[slot];
    }
    if(cue.pending || !cue.sound)g_cues.push_back(cue);
}
SG::Verdict ObserveBucketSound(void* source,const uint8_t* params,bool twoD);
SG::Verdict BeforeBucketSoundAt(void*,void* source,const uint8_t* params){return ObserveBucketSound(source,params,false);}
SG::Verdict BeforeBucketSound2D(void*,void* source,const uint8_t* params){return ObserveBucketSound(source,params,true);}
void InstallBucketSoundHooks(const ue_wrap::facts::Function& atRow,const ue_wrap::facts::Function& twoDRow){
    if(g_bucketHooksReady && g_bucketSoundAt.Get() && g_bucketSound2D.Get())return;
    g_bucketHooksReady=false;void* cls=ue_wrap::object_index::ClassByName(L"GameplayStatics");if(!cls)return;
    const auto at=ue_wrap::facts::ResolveOn(atRow,cls),twoD=ue_wrap::facts::ResolveOn(twoDRow,cls);
    const bool first=at && ue_wrap::ufunction_hook::InstallPreHook(at,&BeforeBucketSoundAt,true);
    const bool second=twoD && ue_wrap::ufunction_hook::InstallPreHook(twoD,&BeforeBucketSound2D,true);
    if(first)g_bucketSoundAt.Set(at.Fn());if(second)g_bucketSound2D.Set(twoD.Fn());g_bucketHooksReady=first && second;
}
void DrainBucketSounds(uint64_t now){
    const auto epoch=g_epoch;
    if(!Host()){
        const auto pending=g_bucketLocal;
        for(auto row:pending){auto& p=row.packet;
            const auto erase=[&]{std::erase_if(g_bucketLocal,[&](const BucketLocal& old){return old.packet.sequence==p.sequence;});};
            if(now>row.expires){erase();continue;}
            if(!row.source.Get() || p.slot!=Local() || (p.playerNo && roster_ledger::Get(p.slot).playerNo!=p.playerNo)){erase();continue;}
            const bool axis=row.hand?hand_item::LocalHandActor()==row.source.Get():local_prop_hold::IsHeld(row.source.Get());
            if(!Current(epoch))return;if(!axis){erase();continue;}
            if(!p.playerNo)p.playerNo=roster_ledger::Get(p.slot).playerNo;
            if(!p.generation)p.generation=row.hand?hand_item::Generation(p.slot):
                (local_streams::LastHeldActor()==row.source.Get()?local_streams::CurrentHoldGen():0);
            if(!p.playerNo || !p.generation)continue;
            const bool own=OwnedSource(p,row.source);if(!Current(epoch))return;if(!own){erase();continue;}
            if(now<row.next)continue;
            auto it=std::find_if(g_bucketLocal.begin(),g_bucketLocal.end(),[&](const BucketLocal& old){return old.packet.sequence==p.sequence;});
            if(it==g_bucketLocal.end())continue;row.next=now+150;*it=row;
            const bool sent=g_session->SendReliableToSlot(0,net::ReliableKind::WaterLaunch,&p,sizeof(p));
            if(!Current(epoch))return;if(sent)erase();
        }
        return;
    }
    const auto count=g_bucketHost.size();unsigned work=8;
    for(size_t i=0;i<count && !g_bucketHost.empty() && work;++i){
        auto row=g_bucketHost.front();g_bucketHost.pop_front();const auto& p=row.packet;
        if(now>row.expires || g_peerEpoch[p.slot]!=row.peerEpoch || roster_ledger::Get(p.slot).playerNo!=p.playerNo || !g_session->IsSlotWorldReady(p.slot))continue;
        --work;
        void* source=HostSource(p);CachedObjRef identity;identity.Set(source);
        const auto valid=[&]{return Current(epoch) && g_peerEpoch[p.slot]==row.peerEpoch && identity.Get() && HostSource(p)==source && Current(epoch);};
        if(!source){g_bucketHost.push_back(row);continue;}
        if(!ue_wrap::building_material::IsBucket(source) || !valid())continue;
        const auto target=element::IntentTarget::ForClientIntent(*g_session,p.slot,600.f);
        const bool reach=bool(target.Authorize(source));if(!Current(epoch))return;
        if(!valid())continue;if(!reach)continue;
        const bool point=bool(target.AuthorizeSegment({p.x,p.y,p.z},{p.x,p.y,p.z}));if(!Current(epoch))return;
        if(!valid() || !point)continue;
        ue_wrap::FVector at{};const bool read=ue_wrap::engine::TryGetActorLocation(source,at);if(!Current(epoch))return;
        if(!valid())continue;if(!read){g_bucketHost.push_back(row);continue;}
        const float x=p.x-at.X,y=p.y-at.Y,z=p.z-at.Z,distance=x*x+y*y+z*z;
        if(std::isfinite(distance) && distance<=600.f*600.f)CanonicalBucketSound(p,p.slot,true);
    }
}
void ReceiveBucketSound(const Launch& p,uint8_t sender){
    if(!Current() || !p.sequence || !CuePoint(p))return;
    if(!Host()){
        if(sender || p.op!=1 || (g_lastSplash && !After(p.sequence,g_lastSplash)) || g_cues.size()>=kMaxCues)return;
        g_lastSplash=p.sequence;Cue cue{};cue.launch=p;cue.bucket=cue.surface=true;cue.expires=active_drive::NowMs()+1500;g_cues.push_back(cue);return;
    }
    if(!sender || sender>=net::kMaxPeers || sender!=p.slot || p.op || !p.playerNo || !p.generation ||
       (p.source.len && (!Key(p.source) || p.generation>65535)) || !g_session->IsSlotWorldReady(sender) ||
       !Reconcile() || roster_ledger::Get(sender).playerNo!=p.playerNo || g_bucketHost.size()>=128)return;
    auto& high=g_bucketHigh[sender];auto& seen=g_bucketSeen[sender];
    if(high && !After(p.sequence,high)){const auto age=high-p.sequence;if(age>=256 || seen[age])return;}
    const auto now=active_drive::NowMs();if(now-g_bucketRateAt[sender]>=1000){g_bucketRateAt[sender]=now;g_bucketRate[sender]=0;}
    if(g_bucketRate[sender]>=32)return;++g_bucketRate[sender];
    if(!high){high=p.sequence;seen.reset();}else if(After(p.sequence,high)){
        const auto shift=p.sequence-high;if(shift>=256)seen.reset();else seen<<=shift;high=p.sequence;}
    seen.set(high-p.sequence);g_bucketHost.push_back({p,now+1500,g_peerEpoch[sender]});
}
void ClearBucketSounds(int slot){
    if(slot<0){g_bucketLocal.clear();g_bucketHost.clear();g_bucketHigh={};g_bucketSeen={};g_bucketRate={};g_bucketRateAt={};g_bucketSequence=0;return;}
    std::erase_if(g_bucketLocal,[slot](const BucketLocal& row){return row.packet.slot==slot;});
    std::erase_if(g_bucketHost,[slot](const BucketHost& row){return row.packet.slot==slot;});
    g_bucketHigh[slot]=0;g_bucketSeen[slot].reset();g_bucketRate[slot]=0;g_bucketRateAt[slot]=0;
}
