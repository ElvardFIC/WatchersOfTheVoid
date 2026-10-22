bool SameJoin(const JoinTarget& a,const JoinTarget& b){return a.revision==b.revision && a.person==b.person && a.epoch==b.epoch;}
bool JoinPerson(const JoinTarget& row,int slot){return row.revision && row.person==coop::roster_ledger::Get(slot).playerNo && row.epoch==g_peerEpoch[slot];}
void QueueHandFoodJoin(uint8_t owner,int slot){
    if(owner>=coop::net::kMaxPeers || slot<=0 || slot>=coop::net::kMaxPeers)return;
    g_handJoin[owner]|=uint32_t(1u<<slot);g_handJoinTarget[owner][slot]={coop::roster_ledger::Get(slot).playerNo,g_peerEpoch[slot],++g_foodJoinSequence};
}
void QueueFoodJoin(void* actor,int slot) {
    auto* s=Session();if(!s || !s->connected() || s->role()!=coop::net::Role::Host || slot<=0 || slot>=coop::net::kMaxPeers)return;
    const auto epoch=g_epoch,capture=++g_foodJoinSequence;const JoinTarget target{coop::roster_ledger::Get(slot).playerNo,g_peerEpoch[slot],capture};
    ue_wrap::CachedObjRef ref;ref.Set(actor);coop::net::WireKey key{};uint32_t eid=0;FillKey(actor,key,eid);
    if(Session()!=s || !s->connected() || s->role()!=coop::net::Role::Host || epoch!=g_epoch || g_world!=ue_wrap::world_identity::Generation() || !ref.Get() || !JoinPerson(target,slot))return;
    const auto id=key.len?coop::net::StringFromWireKey(key):(eid?L"#"+std::to_wstring(eid):std::wstring{});
    if(id.empty() || (g_joinFoods.size()>=4096 && !g_joinFoods.count(id)))return;
    auto& row=g_joinFoods[id];if(row.targets[slot].revision>capture)return;
    row.actor=ref;row.pending|=uint32_t(1u<<slot);row.targets[slot]=target;
}
void RetryFoodJoins() {
    static bool busy=false;if(busy)return;struct Busy{Busy(){busy=true;}~Busy(){busy=false;}}guard;
    auto* s=Session();if(!s || s->role()!=coop::net::Role::Host)return;
    const auto epoch=g_epoch;
    const auto current=[&]{return Session()==s && s->connected() && s->role()==coop::net::Role::Host && epoch==g_epoch &&
        g_world==ue_wrap::world_identity::Generation();};
    const auto count=(std::min)(size_t(16),g_joinFoods.size());
    for(size_t i=0;i<count && !g_joinFoods.empty();++i) {
        if(!current())return;auto it=g_joinFoods.upper_bound(g_joinCursor);if(it==g_joinFoods.end())it=g_joinFoods.begin();
        const auto id=it->first;g_joinCursor=id;const auto row=it->second;
        void* actor=row.actor.Get();
        if(!actor || !F::IsFood(actor) || coop::hand_item::IsHandAxisActor(actor)){g_joinFoods.erase(it);continue;}
        for(int slot=1;slot<coop::net::kMaxPeers;++slot)if(row.pending&(1u<<slot)) {
            bool done=!JoinPerson(row.targets[slot],slot);
            if(!done && s->IsSlotWorldReady(slot))done=Publish(*s,actor,slot);
            if(!current())return;it=g_joinFoods.find(id);
            if(done && it!=g_joinFoods.end() && it->second.actor.Is(actor) && SameJoin(it->second.targets[slot],row.targets[slot]))it->second.pending&=uint32_t(~(1u<<slot));
        }
        it=g_joinFoods.find(id);
        if(it!=g_joinFoods.end() && !it->second.pending)g_joinFoods.erase(it);
    }
    const auto local=coop::players::Registry::Get().LocalPeerId();
    for(uint8_t owner=0;owner<coop::net::kMaxPeers;++owner) {
        const auto pending=g_handJoin[owner];const auto targets=g_handJoinTarget[owner];if(!pending)continue;
        void* actor=owner==local?coop::hand_item::LocalHandActor():coop::hand_item::MirrorActorForSlot(owner);
        const auto generation=owner==local?coop::hand_item::Generation(owner):coop::hand_item::MirrorGeneration(owner);
        for(int slot=1;slot<coop::net::kMaxPeers;++slot)if(pending&(1u<<slot)) {
            bool done=!JoinPerson(targets[slot],slot);
            if(!done && actor && s->IsSlotWorldReady(slot))done=BroadcastHandState(*s,actor,owner,generation,slot);
            if(!current())return;
            if(done && SameJoin(targets[slot],g_handJoinTarget[owner][slot]))g_handJoin[owner]&=uint32_t(~(1u<<slot));
        }
    }
}
