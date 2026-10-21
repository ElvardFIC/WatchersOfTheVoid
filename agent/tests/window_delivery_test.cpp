#define WINDOW_OBSERVATION_NO_MAIN
#include "window_observation_test.cpp"
namespace window_test {
void Index(int count){W::HubPassBegin(nullptr,true);for(int i=0;i<count;++i)W::HubMatch(nullptr,&test::actors[i]);W::HubPassComplete(nullptr,true,test::world);}
void Reset(coop::net::Session& s,int count=2){WindowReset(s);Index(count);}
void WindowPump(int count=1){for(int i=0;i<count;++i)W::Tick();}
coop::net::KeyedScalarPayload State(float value,bool adopt=false,const wchar_t* key=L"window0"){
    coop::net::KeyedScalarPayload p{};coop::net::WireKeyFromString(key,p.key);p.value=value;p.adopt=adopt;return p;
}
}
int main(){using namespace test;coop::net::Session session;using namespace window_test;
    window_test::Reset(session,100);W::QueueConnectBroadcastForSlot(1);CHECK(session.sent.empty());
    int allowance=1;session.allow=[&](int slot,const auto&){CHECK(slot==1);return allowance-->0;};WindowPump();
    CHECK(session.sent.size()==1 && W::g_joins[1].keys.size()==99);session.allow={};WindowPump(16);CHECK(session.sent.size()==100);
    std::set<std::wstring> unique;for(const auto& sent:session.sent){CHECK(sent.payload.adopt==1 && sent.payload.value==.8f);unique.insert(coop::net::StringFromWireKey(sent.payload.key));}CHECK(unique.size()==100);
    window_test::Reset(session,12);W::QueueConnectBroadcastForSlot(1);W::QueueConnectBroadcastForSlot(2);
    session.allow=[](int slot,const auto&){return slot==2;};WindowPump(3);CHECK(session.sent.size()==12 && W::g_joins[1].keys.size()==12);
    actors[0].process=.15f;session.allow={};WindowPump(3);bool fresh=false;
    for(const auto& sent:session.sent)if(sent.slot==1 && coop::net::StringFromWireKey(sent.payload.key)==L"window0")fresh=sent.payload.value==.15f;CHECK(fresh);
    window_test::Reset(session);W::g_indexGen=0;W::QueueConnectBroadcastForSlot(1);WindowPump();CHECK(session.sent.empty() && W::g_joins[1].waiting);
    Index(2);WindowPump();CHECK(session.sent.size()==2);
    window_test::Reset(session,1);actors[0].readable=false;W::QueueConnectBroadcastForSlot(1);const auto before=reads;WindowPump();CHECK(reads==before+1 && session.sent.empty());
    actors[0].readable=true;W::Sent()[L"window0"]=.2f;WindowPump();CHECK(session.sent.size()==1 && session.sent.back().payload.value==.2f);
    window_test::Reset(session,1);W::QueueConnectBroadcastForSlot(1);++actors[0].serial;WindowPump();CHECK(session.sent.empty());
    Index(1);actors[0].process=.4f;WindowPump();CHECK(session.sent.size()==1 && session.sent.back().payload.value==.4f);
    window_test::Reset(session,1);W::QueueConnectBroadcastForSlot(1);onRead=[]{W::Sent()[L"window0"]=.1f;};WindowPump();CHECK(session.sent.back().payload.value==.1f);
    for(int cause=0;cause<7;++cause){window_test::Reset(session,1);W::QueueConnectBroadcastForSlot(1);
        onRead=[&,cause]{if(cause==0)++world;if(cause==1)W::OnDisconnect();if(cause==2)session.mode=coop::net::Role::Client;
            if(cause==3)++people[1].playerNo;if(cause==4)++actors[0].serial;if(cause==5)session.ready[1]=false;
            if(cause==6)W::OnPeerLeftSlot(1);};WindowPump();CHECK(session.sent.empty());
    }
    window_test::Reset(session,1);W::QueueConnectBroadcastForSlot(1);bool replaced=false;
    session.allow=[&](int,const auto&){if(!replaced){replaced=true;W::OnPeerLeftSlot(1);W::QueueConnectBroadcastForSlot(1);return false;}return true;};
    WindowPump();session.allow={};WindowPump();CHECK(session.sent.size()==1 && W::g_joins[1].keys.empty());
    window_test::Reset(session,1);W::QueueConnectBroadcastForSlot(1);session.allow=[](int,const auto&){W::Sent()[L"window0"]=.1f;return true;};WindowPump();
    CHECK(W::g_joins[1].keys.size()==1);session.allow={};WindowPump();CHECK(session.sent.back().payload.value==.1f);
    window_test::Reset(session,1);W::QueueConnectBroadcastForSlot(1);session.allow=[](int,const auto&){W::OnDisconnect();return false;};WindowPump();CHECK(session.sent.empty() && !W::g_joins[1].sequence);
    // Receiver retains scalar-equal partial paints and same-key nested replacements.
    window_test::Reset(session);paintReady=false;W::OnReliable(State(.2f),1);CHECK(actors[0].process==.2f && W::g_pending.size()==1);
    paintReady=true;WindowPump();CHECK(W::g_pending.empty() && W::Sent().at(L"window0")==.2f);
    window_test::Reset(session);W::g_indexGen=0;W::OnReliable(State(.3f),1);CHECK(W::g_pending.size()==1);
    Index(2);WindowPump();CHECK(actors[0].process==.3f && W::g_pending.empty());
    window_test::Reset(session);paintReady=false;W::OnReliable(State(.4f),1);onPaint=[]{W::OnReliable(State(.1f),1);};WindowPump();
    CHECK(W::g_pending.at(L"window0").clean==.1f);paintReady=true;WindowPump();CHECK(actors[0].process==.1f && W::g_pending.empty());
    window_test::Reset(session);onRead=[]{W::OnReliable(State(.1f),1);};W::OnReliable(State(.4f),1);CHECK(actors[0].process==.8f);
    WindowPump();CHECK(actors[0].process==.1f);
    // Only the host's baseline may raise a client's value; delayed local cleaning remains a minimum.
    window_test::Reset(session);session.mode=coop::net::Role::Client;actors[0].process=.1f;W::OnReliable(State(.6f,true),0);CHECK(actors[0].process==.6f);
    W::OnReliable(State(.9f,true),2);CHECK(actors[0].process==.6f);
    window_test::Reset(session);session.mode=coop::net::Role::Client;paintReady=false;W::OnReliable(State(.6f,true),0);
    activeInvocations={1};ue_wrap::script_gate::Call call{};call.object=&actors[0];call.invocation=1;
    W::OnWipePre(call);actors[0].process=.2f;W::OnWipePost(call);paintReady=true;WindowPump();CHECK(actors[0].process==.2f);
    for(int cause=0;cause<6;++cause){window_test::Reset(session);onPaint=[&,cause]{if(cause==0)++world;if(cause==1)W::OnDisconnect();
        if(cause==2)++actors[0].serial;if(cause==3)session.mode=coop::net::Role::Client;
        if(cause==4){W::OnDisconnect();W::Install(&session);}if(cause==5)W::Sent()[L"window0"]=.05f;};
        W::OnReliable(State(.2f),1);CHECK(!W::Sent().contains(L"window0") || W::Sent().at(L"window0")==.05f);
    }
    window_test::Reset(session);W::OnReliable(State(NAN),1);W::OnReliable(State(-1),1);W::OnReliable(State(.2f,false,L"None"),1);CHECK(W::g_pending.empty());
    puts("PASS window delivery: bounded person/peer-epoch joins, current floors/serial index, delayed readiness and per-recipient retry; retained partial/native paint, same-key supersession, adopt trust and session/world fences");
}
