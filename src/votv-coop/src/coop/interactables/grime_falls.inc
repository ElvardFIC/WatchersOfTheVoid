struct PendingFall {float value=0;uint64_t sequence=0;bool host=false;};
std::map<std::wstring,PendingFall> g_falls;
std::wstring g_fallCursor;
uint64_t g_fallSequence=0;
bool g_drainingFalls=false;
std::set<std::wstring> g_sendingFalls;
void ClearFalls(){g_falls.clear();g_fallCursor.clear();}
void FlushFall(const std::wstring& key){
    auto* session=g_session.load();const auto it=g_falls.find(key);
    if(!session || !session->connected() || it==g_falls.end() || g_sendingFalls.count(key))return;
    const bool host=session->role()==net::Role::Host;if(it->second.host!=host){g_falls.erase(it);return;}
    if(host && !session->AnyWorldReadyPeer())return;
    const auto sample=it->second;const auto epoch=g_joinEpoch;
    const auto current=[&]{return epoch==g_joinEpoch && g_joinWorld==ue_wrap::world_identity::Generation() &&
        g_session.load()==session && session->connected() && (session->role()==net::Role::Host)==host;};
    if(!current())return;
    auto value=sample.value;const auto floor=Floor().find(key);if(floor!=Floor().end())value=(std::min)(value,floor->second);
    net::KeyedScalarPayload packet{};WireKeyFromString(key,packet.key);packet.value=value;packet.adopt=0;
    g_sendingFalls.insert(key);struct Sending{const std::wstring& key;~Sending(){g_sendingFalls.erase(key);}}sending{key};
    const bool sent=session->SendReliable(net::ReliableKind::GrimeState,&packet,sizeof(packet));
    if(!current())return;
    const auto latest=g_falls.find(key);
    if(sent && latest!=g_falls.end() && latest->second.sequence==sample.sequence)g_falls.erase(latest);
}
void QueueFall(const std::wstring& key,float value){
    auto* session=g_session.load();
    if(!session || !session->connected() || !IndexCurrent() || key.empty() ||
       key.size()>sizeof(net::WireKey{}.data) || !std::isfinite(value))return;
    value=(std::max)(0.f,value);
    const bool host=session->role()==net::Role::Host;
    auto old=g_falls.find(key);if(old!=g_falls.end() && old->second.host!=host)g_falls.erase(old);
    auto [it,inserted]=g_falls.try_emplace(key,PendingFall{value,++g_fallSequence,host});
    if(!inserted && value<it->second.value)it->second={value,++g_fallSequence,host};
    FlushFall(key);
}
void DrainFalls(){
    if(g_drainingFalls || g_falls.empty())return;
    g_drainingFalls=true;struct Busy{~Busy(){g_drainingFalls=false;}}busy;
    const auto epoch=g_joinEpoch;std::set<std::wstring> visited;
    for(unsigned work=0;work<16 && !g_falls.empty();++work){
        auto it=g_falls.upper_bound(g_fallCursor);if(it==g_falls.end())it=g_falls.begin();
        const auto key=it->first;g_fallCursor=key;if(!visited.insert(key).second)break;
        FlushFall(key);if(epoch!=g_joinEpoch || g_joinWorld!=ue_wrap::world_identity::Generation())return;
    }
}
