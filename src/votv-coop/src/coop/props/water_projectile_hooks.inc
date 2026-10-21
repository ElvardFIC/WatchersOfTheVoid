SG::Verdict BeforeShotBegin(const SG::Call& call){g_shotBegin.Set(call.function);return SG::Verdict::Run;}
void AfterLaunchEmitter(void*,void* source,void* emitted){
    const auto epoch=g_epoch;
    if(!Current() || Host() || R::InCoopDispatch() || !emitted || !FX::IsShot(source) ||
       !SG::IsObjectBodyActive(g_shotBegin.Get(),source) ||
       g_shotGraph.Get()!=ue_wrap::ufunction_hook::CurrentCallerFrame().function)return;
    void* tool=hand_item::LocalHandActor();
    if(!tool || !ue_wrap::pressure_washer::IsWasher(tool) || !SG::IsObjectBodyActive(g_washerTick.Get(),tool))return;
    CachedObjRef actor,particle;actor.Set(source);particle.Set(emitted);
    if(Current(epoch) && actor.Get() && particle.Get())ue_wrap::engine::DestroyComponent(particle.Get(),actor.Get());
}
void InstallLaunchHook(const ue_wrap::facts::Function& row){
    if(g_launchHookReady)return;
    void* cls=ue_wrap::object_index::ClassByName(L"GameplayStatics");if(!cls)return;
    const auto fn=ue_wrap::facts::ResolveOn(row,cls);
    if(fn)g_launchHookReady=ue_wrap::ufunction_hook::InstallPostHook(fn,&AfterLaunchEmitter);
}
void QueueLaunch(const FX::Birth& birth,int owner,bool hostSound){
    if(!Current() || !Host() || g_cues.size()>=kMaxCues)return;
    Cue cue{};auto& p=cue.packet;p.sequence=Next(g_splashSequence);p.kind=2;
    p.x=p.ax=birth.location.X;p.y=p.ay=birth.location.Y;p.z=p.az=birth.location.Z;p.nz=1;
    cue.expires=active_drive::NowMs()+1500;cue.surface=true;cue.sound=!hostSound;
    for(int slot=1;slot<net::kMaxPeers;++slot)if(slot!=owner && g_session->IsSlotWorldReady(slot)){
        cue.pending|=1u<<slot;cue.people[slot]=roster_ledger::Get(slot).playerNo;cue.epochs[slot]=g_peerEpoch[slot];
    }
    if(cue.pending || !cue.sound)g_cues.push_back(cue);
}
void CaptureHost(){
    static bool capturing=false;
    if(!Current() || !Host() || capturing)return;
    struct Guard{bool& value;Guard(bool& v):value(v){value=true;}~Guard(){value=false;}}guard(capturing);
    const auto epoch=g_epoch;unsigned work=8;
    const auto pending=g_nativeShots;g_nativeShots.clear();
    for(const auto& row:pending){
        if(!Current(epoch))return;if(active_drive::NowMs()>row.expires)continue;
        void* source=row.source.Get();void* actor=row.actor.Get();if(!source || !actor || row.epoch!=epoch)continue;
        if(SourceActive(source) || !work){g_nativeShots.push_back(row);continue;}--work;
        const auto valid=[&]{return Current(epoch) && Host() && row.actor.Get() && row.source.Get();};
        FX::Birth birth{};const bool read=FX::ReadBirth(actor,birth,valid);if(!Current(epoch))return;
        if(read && valid())QueueLaunch(birth,0,false);else if(valid())g_nativeShots.push_back(row);
    }
}
