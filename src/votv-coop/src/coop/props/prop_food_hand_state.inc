// Hand condition samples can overtake HandItem on its separate reliable lane.
bool ValidFoodKey(const coop::net::FoodStatePayload& p) {
    if(p.key.len>sizeof(p.key.data))return false;
    for(unsigned i=0;i<p.key.len;++i)if(uint8_t(p.key.data[i])<33 || uint8_t(p.key.data[i])>126)return false;
    return !p.key.len || coop::net::StringFromWireKey(p.key)!=L"None";
}
bool HandAfter(uint32_t a,uint32_t b){return int32_t((a-b)<<6)>0;}
uint32_t AnnouncedHand(uint8_t slot){return coop::hand_item::Generation(slot)&0x03FFFFFFu;}
void ReconcileHandOwners() {
    for(int slot=0;slot<coop::net::kMaxPeers;++slot) {
        const auto person=coop::roster_ledger::Get(slot).playerNo;
        if(person==g_handOccupants[slot])continue;
        if(g_handOccupants[slot])OnPeerLeft(slot);
        g_handOccupants[slot]=person;
    }
}
void QueueHandCondition(const coop::net::FoodStatePayload& p,uint8_t slot,uint32_t generation) {
    if(!p.key.len || !p.playerNo || p.playerNo!=coop::roster_ledger::Get(slot).playerNo)return;
    const auto current=AnnouncedHand(slot);
    if(current && current!=generation && !HandAfter(generation,current))return;
    const auto& old=g_pendingHand[slot];uint8_t oldSlot=0;uint32_t oldGeneration=0;
    if(old.have && DecodeHandAddress(old.state.elementId,oldSlot,oldGeneration) &&
       oldGeneration!=generation && !HandAfter(generation,oldGeneration))return;
    g_pendingHand[slot]={p,++g_handRevision,true};
}
enum class HandApply {Retry,Done};
HandApply ApplyHandCondition(const coop::net::FoodStatePayload& p,uint8_t slot,uint32_t generation,uint64_t revision) {
    auto* s=Session();const auto epoch=g_epoch,peerEpoch=g_peerEpoch[slot];const auto role=s?s->role():coop::net::Role::Host;
    const auto current=[&]{return Session()==s && s && s->connected() && epoch==g_epoch &&
        g_world==ue_wrap::world_identity::Generation() && s->role()==role && g_peerEpoch[slot]==peerEpoch &&
        coop::roster_ledger::Get(slot).playerNo==p.playerNo && g_pendingHand[slot].revision==revision;};
    if(!p.playerNo || !current())return HandApply::Done;
    const auto announced=AnnouncedHand(slot);
    if(announced && announced!=generation && !HandAfter(generation,announced))return HandApply::Done;
    void* actor=HandActor(slot,generation);if(!actor || !F::IsFood(actor))return HandApply::Retry;
    ue_wrap::CachedObjRef live;live.Set(actor);
    const auto key=coop::net::StringFromWireKey(p.key);
    const bool local=slot==coop::players::Registry::Get().LocalPeerId();
    if(local && ue_wrap::prop::GetInteractableKeyString(actor)!=key)return HandApply::Retry;
    if(!current() || !live.Get())return HandApply::Done;
    if(s->role()==coop::net::Role::Host) {
        const bool allowed=bool(coop::element::IntentTarget::ForClientIntent(*s,slot,kBiteReach).Authorize(actor));
        if(!current() || !live.Get() || HandActor(slot,generation)!=actor)return HandApply::Done;
        if(!allowed)return HandApply::Done;
    }
    const auto condition=DecodeCondition(p);
    const auto valid=[&]{return current() && live.Get() && HandActor(slot,generation)==actor;};
    if(!F::Write(actor,condition,valid))return valid()?HandApply::Retry:HandApply::Done;
    if(!current() || !live.Get() || HandActor(slot,generation)!=actor)return HandApply::Done;
    g_handKeys[slot]=key;
    if(s->role()==coop::net::Role::Host) {
        g_handAccepted[slot]={generation,p.playerNo,key,revision};
        g_handSent[slot].sent=false;BroadcastHandState(*s,actor,slot,generation);
    } else {
        if(local)g_clientHandGeneration[key]=coop::hand_item::Generation(slot);
        g_clientBitePending.erase(key);
        g_clientHandCanonical[key]=g_clientHandSent[key]=Remember(condition);
    }
    return HandApply::Done;
}
void RetryHandConditions() {
    static bool busy=false;if(busy)return;struct Busy{Busy(){busy=true;}~Busy(){busy=false;}}guard;
    const auto epoch=g_epoch;auto* s=Session();
    for(uint8_t slot=0;slot<coop::net::kMaxPeers;++slot) {
        const auto pending=g_pendingHand[slot];if(!pending.have)continue;
        uint8_t owner=0;uint32_t generation=0;
        if(!DecodeHandAddress(pending.state.elementId,owner,generation)){g_pendingHand[slot]={};continue;}
        const auto result=ApplyHandCondition(pending.state,owner,generation,pending.revision);
        if(epoch!=g_epoch || Session()!=s || g_world!=ue_wrap::world_identity::Generation())return;
        if(g_pendingHand[slot].revision==pending.revision && result==HandApply::Done)g_pendingHand[slot]={};
    }
}

uint64_t g_localHandPublishRevision=0;
void PublishLocalHand(coop::net::Session& session,uint64_t now){
    const auto owner=coop::players::Registry::Get().LocalPeerId();if(owner>=coop::net::kMaxPeers || g_pendingHand[owner].have)return;
    void* actor=coop::hand_item::LocalHandActor();const auto generation=coop::hand_item::Generation(owner);
    const auto person=coop::roster_ledger::Get(owner).playerNo;const auto peerEpoch=g_peerEpoch[owner],epoch=g_epoch,capture=++g_publishSequence;
    const auto handRevision=g_handRevision,worldRevision=g_worldRevision;ue_wrap::CachedObjRef ref;ref.Set(actor);
    const auto identityCurrent=[&]{return Session()==&session && session.connected() && session.role()==coop::net::Role::Client &&
        epoch==g_epoch && g_world==ue_wrap::world_identity::Generation() && ref.Get() &&
        coop::players::Registry::Get().LocalPeerId()==owner && coop::hand_item::LocalHandActor()==actor &&
        coop::hand_item::Generation(owner)==generation && g_peerEpoch[owner]==peerEpoch && coop::roster_ledger::Get(owner).playerNo==person;};
    const auto sourceCurrent=[&]{return identityCurrent() && !g_pendingHand[owner].have && handRevision==g_handRevision && worldRevision==g_worldRevision;};
    F::Condition condition{};
    if(!actor || !generation || !person || !sourceCurrent() || !F::IsFood(actor) || !sourceCurrent() || !F::Read(actor,condition) || !sourceCurrent() || !Valid(condition))return;
    coop::net::FoodStatePayload p{};FillKey(actor,p.key,p.elementId);
    if(!p.key.len || !sourceCurrent() || g_localHandPublishRevision>capture)return;g_localHandPublishRevision=capture;
    const auto current=[&]{return sourceCurrent() && g_localHandPublishRevision==capture;};const auto key=coop::net::StringFromWireKey(p.key);
    const auto canonical=g_clientHandCanonical.find(key);const auto oldGeneration=g_clientHandGeneration.find(key);
    if(canonical!=g_clientHandCanonical.end() && (oldGeneration==g_clientHandGeneration.end() || oldGeneration->second!=generation)){
        const auto saved=canonical->second;condition={saved.temperature,saved.ripeness,saved.uses,saved.cookProgress};
        if(!F::Write(actor,condition,current) || !current())return;g_clientHandGeneration[key]=generation;
    }
    auto bite=g_clientBitePending.find(key);if(bite!=g_clientBitePending.end()){
        if(now-bite->second<kClientBiteAckTimeoutMs)return;g_clientBitePending.erase(bite);
    }
    const auto previous=g_clientHandSent[key];const auto lastSent=g_clientHandSentAt[key];
    if(!WorthSending(previous,condition) && now-lastSent<2000)return;
    p.elementId=HandAddress(owner,generation);p.playerNo=person;EncodeCondition(condition,p);
    const bool sent=session.SendReliableToSlot(0,coop::net::ReliableKind::FoodState,&p,sizeof(p));
    if(!current()){if(identityCurrent())g_clientHandSent[key].sent=false;return;}
    if(sent){g_clientHandSent[key]=Remember(condition);g_clientHandSentAt[key]=now;}
}
