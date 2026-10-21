struct JoinSnapshot {
    std::set<std::wstring> keys;
    std::wstring cursor;
    uint64_t sequence=0;
    uint16_t person=0;
    bool waiting=false;
};
std::array<JoinSnapshot,coop::net::kMaxPeers> g_joins{};
std::array<uint64_t,coop::net::kMaxPeers> g_peerEpoch{};
uint64_t g_joinSequence=0;
unsigned g_joinTurn=1;
bool g_drainingJoin=false;
void DrainJoins(){
    if(g_drainingJoin)return;
    auto* session=g_session.load();if(!session || !session->connected() || session->role()!=coop::net::Role::Host)return;
    struct Guard{Guard(){g_drainingJoin=true;}~Guard(){g_drainingJoin=false;}}guard;
    const auto epoch=g_epoch;const auto world=g_world;
    const auto sourceCurrent=[&]{return session==g_session.load() && epoch==g_epoch && world==ue_wrap::world_identity::Generation() &&
        session->connected() && session->role()==coop::net::Role::Host;};
    std::array<std::set<std::wstring>,coop::net::kMaxPeers> visited;
    for(unsigned work=0;work<24;++work){
        if(!sourceCurrent())return;const auto slot=g_joinTurn;g_joinTurn=g_joinTurn%(coop::net::kMaxPeers-1)+1;
        auto& join=g_joins[slot];if(!join.sequence)continue;
        if(!join.person || join.person!=roster_ledger::Get(slot).playerNo){join={};continue;}
        if(!session->IsSlotWorldReady(slot))continue;
        if(join.waiting){
            if(!IndexCurrent())continue;
            {std::lock_guard<std::mutex> lock(g_indexMutex);for(const auto& [key,ref]:g_byKey)if(ValidKey(key))join.keys.insert(key);}
            for(const auto& [key,value]:Sent())if(ValidKey(key))join.keys.insert(key);
            join.waiting=false;
        }
        if(join.keys.empty()){join={};continue;}
        const auto sequence=join.sequence,peerEpoch=g_peerEpoch[slot];const auto person=join.person;
        auto it=join.keys.upper_bound(join.cursor);if(it==join.keys.end())it=join.keys.begin();const auto key=*it;join.cursor=key;
        if(!visited[slot].insert(key).second)continue;
        const auto current=[&]{return sourceCurrent() && session->IsSlotWorldReady(slot) && g_joins[slot].sequence==sequence &&
            g_peerEpoch[slot]==peerEpoch && roster_ledger::Get(slot).playerNo==person;};
        void* actor=ResolveFast(key);ue_wrap::CachedObjRef ref;ref.Set(actor);float value=0;bool have=false;
        if(actor){const bool read=BW::ReadClean(actor,value);if(!sourceCurrent())return;
            if(!read || !current() || !ref.Get() || ResolveFast(key)!=actor || !std::isfinite(value))continue;have=true;}
        const auto floor=Sent().find(key);const bool haveFloor=floor!=Sent().end();const float oldFloor=haveFloor?floor->second:0;
        if(haveFloor){value=have?std::min(value,oldFloor):oldFloor;have=true;}
        if(!have)continue;
        const auto valid=[&]{if(!current() || (actor && (!ref.Get() || ResolveFast(key)!=actor)))return false;
            const auto latest=Sent().find(key);return (latest!=Sent().end())==haveFloor && (!haveFloor || latest->second==oldFloor);};
        coop::net::KeyedScalarPayload packet{};WireKeyFromString(key,packet.key);packet.value=(std::max)(0.f,value);packet.adopt=1;
        if(!valid())continue;
        const bool sent=session->SendReliableToSlot(slot,coop::net::ReliableKind::WindowCleanState,&packet,sizeof(packet));
        if(!sourceCurrent())return;
        if(sent && valid())g_joins[slot].keys.erase(key);
    }
}
