struct JoinValue {float away=0;bool haveAway=false;};
struct JoinSnapshot {
    std::map<std::wstring,JoinValue> keys;
    std::wstring cursor;
    uint64_t sequence=0;
    uint16_t person=0;
    bool waiting=false;
};
std::array<JoinSnapshot,net::kMaxPeers> g_joins{};
uint64_t g_joinSequence=0,g_joinEpoch=0;
uint32_t g_joinWorld=0;
unsigned g_joinTurn=1;
bool g_drainingJoin=false;
void ResetJoins(){++g_joinEpoch;g_joinWorld=ue_wrap::world_identity::Generation();g_joins={};g_joinTurn=1;ClearFalls();}
void JoinAway(const std::wstring& key,float value){
    for(auto& join:g_joins){const auto it=join.keys.find(key);if(it!=join.keys.end()){
        auto& entry=it->second;if(!entry.haveAway || value<entry.away)entry.away=value;entry.haveAway=true;
    }}
}
bool SnapshotKeys(int slot){
    if(!IndexCurrent())return false;
    auto& join=g_joins[slot];
    {std::lock_guard<std::mutex> lock(g_indexMutex);for(const auto& [key,ref]:g_byKey)join.keys.try_emplace(key);}
    for(const auto& [key,value]:Floor())join.keys[key]={value,true};
    for(const auto& [exact,parked]:g_parked.Get()){
        auto& value=join.keys[parked.key];if(!value.haveAway || parked.value<value.away)value.away=parked.value;value.haveAway=true;
    }
    join.waiting=false;return true;
}
void DrainJoins(){
    if(g_drainingJoin)return;
    auto* session=g_session.load();if(!session || !session->connected() || session->role()!=net::Role::Host)return;
    if(g_joinWorld!=ue_wrap::world_identity::Generation())ResetJoins();
    g_drainingJoin=true;struct Busy{~Busy(){g_drainingJoin=false;}}busy;
    const auto epoch=g_joinEpoch;
    const auto sourceCurrent=[&]{return epoch==g_joinEpoch && g_joinWorld==ue_wrap::world_identity::Generation() &&
        g_session.load()==session && session->connected() && session->role()==net::Role::Host;};
    std::array<std::set<std::wstring>,net::kMaxPeers> visited;
    for(unsigned work=0;work<24;++work){
        if(!sourceCurrent())return;
        const auto slot=g_joinTurn;g_joinTurn=g_joinTurn%(net::kMaxPeers-1)+1;
        auto& join=g_joins[slot];if(!join.sequence)continue;
        if(roster_ledger::Get(slot).playerNo!=join.person){join={};continue;}
        if(!session->IsSlotWorldReady(slot))continue;
        if(join.waiting && !SnapshotKeys(slot))continue;
        if(join.keys.empty()){join={};continue;}
        const auto sequence=join.sequence;const auto person=join.person;
        auto it=join.keys.upper_bound(join.cursor);if(it==join.keys.end())it=join.keys.begin();
        const auto key=it->first;const auto value=it->second;join.cursor=key;
        if(!visited[slot].insert(key).second)continue;
        const auto current=[&]{return sourceCurrent() && session->IsSlotWorldReady(slot) &&
            g_joins[slot].sequence==sequence && roster_ledger::Get(slot).playerNo==person;};
        void* actor=ResolveFast(key);ue_wrap::CachedObjRef ref;ref.Set(actor);
        float process=0;bool have=value.haveAway;if(have)process=value.away;
        if(actor){float live=0;const bool read=G::ReadProcess(actor,live);
            if(!current()){if(!sourceCurrent())return;continue;}
            if(!read || !ref.Get() || ResolveFast(key)!=actor || !std::isfinite(live))continue;
            if(!have || live<process)process=live;have=true;
        }
        const auto floor=Floor().find(key);if(floor!=Floor().end()){
            if(!have || floor->second<process)process=floor->second;have=true;
        }
        if(!have){if(current())g_joins[slot].keys.erase(key);continue;}
        if(!std::isfinite(process))continue;
        net::KeyedScalarPayload packet{};WireKeyFromString(key,packet.key);
        if(key.size()>sizeof(packet.key.data)){g_joins[slot].keys.erase(key);continue;}
        packet.value=(std::max)(0.f,process);packet.adopt=1;
        const bool sent=session->SendReliableToSlot(slot,net::ReliableKind::GrimeState,&packet,sizeof(packet));
        if(!sourceCurrent())return;
        if(sent && current())g_joins[slot].keys.erase(key);
    }
}
