#define DINGUS_WORLD_FIXTURE_ONLY
#include "dingus_2155_test.cpp"
int main(){using namespace test;namespace S=coop::dingus_music_sync;using coop::net::Role;using Packet=coop::net::DingusMusicPayload;
    coop::net::Session session;
    ResetWorldToy(session);clockReady=true;S::Sample(&actor,now);CHECK(!S::g_rows.at(key).state.episode);
    music.playing=false;S::Sample(&actor,now);now+=100;music.playing=true;S::Sample(&actor,now);
    const auto episode=S::g_rows.at(key).state.episode;CHECK(episode && S::g_rows.at(key).state.elapsed==0);
    now+=1500;music.pitch=2;S::Sample(&actor,now);CHECK(std::fabs(S::g_rows.at(key).state.elapsed-1.5f)<.001f && S::g_rows.at(key).state.pitch==2);
    now+=1000;S::Sample(&actor,now);CHECK(std::fabs(S::g_rows.at(key).state.elapsed-3.5f)<.001f && S::g_rows.at(key).state.episode==episode);
    const auto anchor=S::g_rows.at(key).state;const auto anchorAt=S::g_rows.at(key).at;session.sendOk=false;S::g_poll=now+100000;S::Tick();
    now+=2000;session.sendOk=true;S::Tick();CHECK(!session.sent.empty());Packet sent{};std::memcpy(&sent,session.sent.back().data.data(),sizeof(sent));
    CHECK(std::fabs(sent.elapsed-(anchor.elapsed+float(now-anchorAt)/1000.f*2))<.001f && sent.episode==episode);
    session.sent.clear();S::QueueConnect(2);S::Tick();auto joined=std::find_if(session.sent.begin(),session.sent.end(),[](const auto& packet){return packet.slot==2;});
    CHECK(joined!=session.sent.end());std::memcpy(&sent,joined->data.data(),sizeof(sent));CHECK(sent.episode==episode && !sent.playerNo);
    ResetWorldToy(session,Role::Client);session.testRtt=200;S::Receive(sent,0);now+=1000;S::Tick();
    CHECK(music.episode==episode && std::fabs(music.elapsed-(sent.elapsed+2.2f))<.001f);
    // A host restart gets a new episode even if both surrounding samples played.
    ResetWorldToy(session);clockReady=true;S::Sample(&actor,now);uint8_t action=5;ue_wrap::script_gate::Call call{};
    call.object=&actor;call.function=&ui;call.locals=&action;NativeAfterAction(call);CHECK(S::g_rows.at(key).state.episode);
    const auto first=S::g_rows.at(key).state.episode;NativeAfterAction(call);CHECK(S::g_rows.at(key).state.episode!=first);
    ++ui.serial;now+=1000;S::Sample(&actor,now);CHECK(!S::g_rows.at(key).state.episode); // unknown replacement sound position
    // Missing metadata cannot invent a phase or stop the ordinary state lane.
    ResetWorldToy(session);music.playing=false;S::Sample(&actor,now);music.playing=true;S::Sample(&actor,now,true);CHECK(!S::g_rows.at(key).state.episode && S::g_rows.at(key).state.playing);
    clockReady=true;now+=1000;S::Sample(&actor,now);CHECK(!S::g_rows.at(key).state.episode);
    S::Sample(&actor,now,true);CHECK(S::g_rows.at(key).state.episode);
    // Native reads and cached presentation must retain the canonical source key.
    const auto stableKey=key;
    ResetWorldToy(session);callback=[]{key=L"renamed";};S::Sample(&actor,now,true);CHECK(S::g_rows.empty());key=stableKey;
    ResetWorldToy(session);clockReady=true;clockCallback=[]{key=L"renamed";};S::Sample(&actor,now,true);CHECK(S::g_rows.empty());key=stableKey;
    ResetWorldToy(session);S::Sample(&actor,now);session.sent.clear();key=L"renamed";S::QueueConnect(2);S::g_poll=now+10000;S::Tick();
    CHECK(session.sent.empty() && S::g_rows.empty());key=stableKey;
    ResetWorldToy(session,Role::Client);S::Receive(sent,0);S::Tick();auto paints=applies;
    key=L"renamed";now+=250;S::Tick();CHECK(applies==paints);key=stableKey;now+=250;S::Tick();CHECK(applies==paints+1);
    ResetWorldToy(session,Role::Client);S::Receive(sent,0);callback=[]{key=L"renamed";};S::Tick();
    CHECK(!S::g_rows.at(stableKey).playback.episode);key=stableKey;
    ResetWorldToy(session);clockReady=true;clockCallback=[]{++world;};S::Sample(&actor,now,true);CHECK(S::g_rows.empty());
    // Old occupants cannot toggle under a new player's identity.
    ResetWorldToy(session);Packet request{};coop::net::WireKeyFromString(key,request.key);request.op=1;request.sequence=1;request.playerNo=people[1];
    ++people[1];auto before=interactions;S::Receive(request,1);CHECK(interactions==before);request.playerNo=people[1];S::Receive(request,1);CHECK(interactions==before+1);
    ResetWorldToy(session);request.sequence=1;request.playerNo=people[1];const auto originalKey=key;before=interactions;
    coop::element::callback=[]{key=L"renamed";};S::Receive(request,1);CHECK(interactions==before);key=originalKey;
    // PRE/POST is one invocation and one connection, including host-role reuse.
    for(int cause=0;cause<5;++cause){ResetWorldToy(session,Role::Client);action=6;call.invocation=10000+cause;invocations.insert(call.invocation);S::BeforeAction(call);
        if(cause==0)S::OnPeerLeft(1);if(cause==1)++people[1];if(cause==2)S::OnDisconnect();if(cause==3)invocations.erase(call.invocation);
        if(cause==4){S::OnDisconnect();S::Install(&session);}S::AfterAction(call);CHECK(S::g_intents.empty());invocations.erase(call.invocation);}
    ResetWorldToy(session,Role::Client);action=5;call.invocation=20000;invocations.insert(call.invocation);CHECK(S::BeforeAction(call)==ue_wrap::script_gate::Verdict::Cancel);
    CHECK(S::g_intents.size()==1 && S::g_intents.front().state.playerNo==people[1]);session.sendOk=false;
    session.allowSend=[&](int,auto,const void*,int){S::OnPeerLeft(1);return false;};session.sendOk=true;S::Tick();CHECK(S::g_intents.empty());invocations.erase(call.invocation);
    for(bool pressed:{false,true}){ResetWorldToy(session,Role::Client);plush=true;alt=pressed;call.tag=S::kPlushUse;call.invocation=30000+int(pressed);invocations.insert(call.invocation);
        S::BeforeAction(call);alt=!pressed;S::AfterAction(call);CHECK(S::g_intents.size()==size_t(pressed));invocations.erase(call.invocation);}
    for(auto role:{Role::Host,Role::Client}){ResetWorldToy(session,role);action=6;call.tag=0;call.invocation=40000+int(role);invocations.insert(call.invocation);
        S::BeforeAction(call);key=L"renamed";S::AfterAction(call);CHECK(S::g_intents.empty() && S::g_pulses.empty());key=stableKey;invocations.erase(call.invocation);}
    ResetWorldToy(session,Role::Client);music.used=false;callback=[]{key=L"renamed";};call.invocation=50000;invocations.insert(call.invocation);
    S::BeforeTouch(call);CHECK(S::g_intents.empty());key=stableKey;invocations.erase(call.invocation);
    ResetWorldToy(session);Packet pulse{};coop::net::WireKeyFromString(key,pulse.key);pulse.volume=1.25f;S::QueuePulse(pulse,1);
    callback=[]{key=L"renamed";};S::Tick();CHECK(!S::g_pulses.empty() && S::g_pulses.front().play);key=stableKey;
    ResetWorldToy(session,Role::Client);action=5;call.tag=0;call.invocation=60000;invocations.insert(call.invocation);
    CHECK(S::BeforeAction(call)==ue_wrap::script_gate::Verdict::Cancel);session.ready=false;S::Tick();
    CHECK(session.sent.empty() && S::g_intents.size()==1);session.ready=true;S::Tick();
    CHECK(session.sent.size()==1 && S::g_intents.empty());invocations.erase(call.invocation);
    // Malformed clock fields never reach native presentation.
    for(int cause=0;cause<7;++cause){ResetWorldToy(session,Role::Client);auto bad=sent;bad.sequence=1;
        if(cause==0)bad.pitch=NAN;if(cause==1)bad.pitch=0;if(cause==2)bad.elapsed=INFINITY;if(cause==3)bad.elapsed=-1;
        if(cause==4)bad.episode=0;if(cause==5)bad.playing=0;if(cause==6)bad.playerNo=11;S::Receive(bad,0);CHECK(S::g_rows.empty());}
    puts("PASS toy playback position: observed episodes, rate integration, queue age/RTT/join, unknown phase, metadata recovery, invocation/source identity and malformed clock refusal");return 0;
}
