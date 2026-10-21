#define PROP_FIRE_FIXTURE_ONLY
#include "prop_fire_2137_test.cpp"
int main(){using coop::net::Role;using ue_wrap::script_gate::Verdict;
    const auto setup=[](Role role){Reset(role);gasoline=true;};
    const auto packet=[](uint8_t op=0,uint32_t seq=1){auto p=Packet(op,seq);coop::net::WireKeyFromString(coop::drink_spill_sync::KeyFor(&actor),p.key);return p;};
    setup(Role::Host);state={true,30,.25f};Tick();CHECK(session.sent.size()==3 && session.sent[0].state.key.len==28);
    setup(Role::Host);S::Receive(packet(),1);Tick();CHECK(ignites==1 && state.burning);S::Receive(packet(),1);Tick();CHECK(ignites==1);
    S::Receive(packet(1,2),1);Tick();CHECK(extinguishes==1 && !state.burning);
    setup(Role::Client);S::Receive(packet(2),0);canonical=false;Tick();CHECK(!applies);canonical=true;Tick();CHECK(applies==1 && state.burning);
    setup(Role::Client);CHECK(Verb()==Verdict::Cancel && S::g_requests.size()==1);canonical=false;S::g_requests.clear();CHECK(Verb()==Verdict::Cancel && S::g_requests.empty());
    ue_wrap::script_gate::Call call{};call.object=&actor;CHECK(S::OnDamage(call)==Verdict::Cancel);
    // Native spread/expiry stays with host; flames may still target the local player.
    canonical=true;CHECK(Verb(&actor,0x46490001,(void*)123)==Verdict::Cancel && S::g_requests.empty());
    CHECK(Verb((void*)456)==Verdict::Run);call.object=(void*)456;CHECK(S::OnDamage(call)==Verdict::Run);
    call.object=(void*)789;call.callerObject=call.object;CHECK(S::OnGasExtinguish(call)==Verdict::Cancel && S::g_requests.empty());
    call.callerObject=nullptr;CHECK(S::OnGasExtinguish(call)==Verdict::Cancel && S::g_requests.size()==1 && S::g_requests.front().p.op==1);
    {S::Applying apply;CHECK(S::OnGasExtinguish(call)==Verdict::Run && Verb()==Verdict::Run);}
    call.object=(void*)123;CHECK(S::OnGasExtinguish(call)==Verdict::Run);
    setup(Role::Host);auto waiting=packet();canonical=false;S::Receive(waiting,1);Tick();CHECK(!ignites);canonical=true;Tick();CHECK(ignites==1);
    setup(Role::Host);coop::element::reach=false;S::Receive(packet(),1);Tick();CHECK(!ignites);
    setup(Role::Client);S::Receive(packet(2),0);callback=[]{S::OnDisconnect();};Tick();CHECK(!S::g_rows[0].applied);
    setup(Role::Host);auto old=packet();++people[1].playerNo;S::Receive(old,1);Tick();CHECK(!ignites);
    S::Receive(packet(),1);Tick();CHECK(ignites==1);S::Receive(old,1);Tick();CHECK(ignites==1);
    setup(Role::Host);S::Receive(packet(),1);++people[1].playerNo;Tick();CHECK(!ignites && S::g_requests.empty());
    for(int cause=0;cause<7;++cause){setup(Role::Host);S::Receive(packet(),1);coop::element::onProbe=[cause]{
        if(cause==0)S::OnPeerLeft(1);if(cause==1)++world;if(cause==2)++people[1].playerNo;
        if(cause==3)S::OnDisconnect();if(cause==4)session.mode=Role::Client;if(cause==5)session.ready[1]=false;if(cause==6)++actor.serial;};
        Tick();CHECK(!ignites);}
    setup(Role::Host);S::Receive(packet(),1);coop::element::onProbe=[&]{S::Receive(packet(1,2),1);};Tick();CHECK(!ignites);Tick();CHECK(extinguishes==1);
    for(int cause=0;cause<5;++cause){setup(Role::Client);CHECK(Verb()==Verdict::Cancel);session.sendOk=false;session.onSend=[cause]{
        if(cause==0)S::OnDisconnect();if(cause==1)++world;if(cause==2)S::OnPeerLeft(1);if(cause==3)++people[1].playerNo;if(cause==4)session.mode=Role::Host;};
        Tick();CHECK(S::g_requests.empty());}
    setup(Role::Client);CHECK(Verb()==Verdict::Cancel);session.sendOk=false;session.onSend=[]{CHECK(Verb(&actor,0x46490002)==Verdict::Cancel);};
    Tick();CHECK(S::g_requests.size()==1 && S::g_requests.front().p.op==1);session.sendOk=true;Tick();CHECK(session.sent.size()==1 && session.sent[0].state.op==1);
    setup(Role::Client);session.ready[0]=false;CHECK(Verb()==Verdict::Cancel);Tick();CHECK(session.sent.empty() && S::g_requests.size()==1);
    session.ready[0]=true;Tick();CHECK(session.sent.size()==1 && session.sent[0].state.playerNo==people[1].playerNo);
    setup(Role::Host);state={true,30,.25f};S::QueueConnect(2);session.onSend=[]{S::OnDisconnect();};Tick();CHECK(session.sent.size()==1 && S::g_rows[0].key.empty());
    for(bool gas:{false,true}){Reset();gasoline=gas;auto intent=gas?packet():Packet();applyOk=false;S::Receive(intent,1);Tick();
        CHECK(ignites==1 && state.fuel==30 && S::g_requests.empty());Tick();CHECK(ignites==1 && state.fuel==30);
        applyOk=true;intent.sequence=2;S::Receive(intent,1);Tick();CHECK(ignites==2 && state.fuel==60);
        nativeReady=false;intent.sequence=3;intent.op=1;S::Receive(intent,1);Tick();CHECK(!extinguishes && S::g_requests.size()==1);
        nativeReady=true;applyOk=false;Tick();CHECK(extinguishes==1 && S::g_requests.empty());Tick();CHECK(extinguishes==1);}
    // One blocked recipient cannot lose the final off state or stop other peers.
    for(bool gas:{false,true}){Reset();gasoline=gas;state={true,30,.25f};Tick();session.sent.clear();
        session.allowSend=[](int slot){return slot!=2;};state={};Tick();
        CHECK(session.sent.size()==2 && S::g_rows[0].dirty && !session.sent[0].state.burning);
        session.allowSend={};Tick();CHECK(!S::g_rows[0].dirty);
        CHECK(std::any_of(session.sent.begin(),session.sent.end(),[](const auto& row){return row.slot==2 && !row.state.burning;}));}
    puts("PASS gasoline fire: canonical identity/readiness, host ignite/extinguish, deferred flame presentation, spread/damage/expiry refusal and preserved player paths");return 0;
}
