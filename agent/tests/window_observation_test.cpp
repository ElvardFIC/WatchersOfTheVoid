#define main PriorGrimeMain
#include "grime_join_test.cpp"
#undef main
namespace coop::config_registry::rows {inline constexpr FlagRow window_log{},window_synth{};}
#include "../../src/votv-coop/src/coop/interactables/window_sync.cpp"
namespace ue_wrap::reflection {
void* ClassOf(void* actor){return actor;}
void* FindDispatchFunctionCached(void*,const wchar_t*){return nullptr;}
std::wstring ToString(const FName&){return L"window";}
}
namespace ue_wrap::script_gate {bool NameWatchLive(const facts::Function&,int){return false;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void*){}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int){return false;}
bool Call(void*,ParamFrame&){CHECK(false);return false;}
}
namespace ue_wrap::base_window {
bool EnsureResolved(){return true;}
bool IsBaseWindow(void* actor){return reflection::IsLive(actor);}
std::wstring GetKeyString(void* actor){return L"window"+std::to_wstring(static_cast<test::Actor*>(actor)->id);}
bool ReadClean(void* actor,float& out){return grime::ReadProcess(actor,out);}
bool WriteCleanAndApply(void* actor,float value,const std::function<bool()>& current){
    if(current && !current())return false;static_cast<test::Actor*>(actor)->process=value;
    auto fn=std::move(test::onPaint);test::onPaint={};if(fn)fn();return test::paintReady && (!current || current());}
}
namespace W=coop::window_sync;
void WindowReset(coop::net::Session& session){W::OnDisconnect();test::Reset(session,2);W::Install(&session);
    W::g_byKey.clear();W::HubPassBegin(nullptr,true);for(int i=0;i<2;++i)W::HubMatch(nullptr,&test::actors[i]);W::HubPassComplete(nullptr,true,test::world);}
#ifndef WINDOW_OBSERVATION_NO_MAIN
int main(){using namespace test;coop::net::Session session;
    ue_wrap::script_gate::Call outer{},inner{},unrelated{};outer.object=&actors[0];outer.invocation=1;
    inner.object=&actors[1];inner.invocation=2;unrelated.invocation=3;
    WindowReset(session);activeInvocations={1,2,3};W::OnWipePre(outer);W::OnWipePre(unrelated);W::OnWipePost(unrelated);
    W::OnWipePre(inner);actors[1].process=.5f;W::OnWipePost(inner);actors[0].process=.6f;W::OnWipePost(outer);
    CHECK(W::Sent().at(L"window0")==.6f && W::Sent().at(L"window1")==.5f && W::g_wipes.empty());
    CHECK(session.sent.size()==2);
    WindowReset(session);inner.object=&actors[0];activeInvocations={1,2};W::OnWipePre(outer);W::OnWipePre(inner);
    actors[0].process=.3f;W::OnWipePost(inner);actors[0].process=.6f;W::OnWipePost(outer);
    CHECK(W::Sent().at(L"window0")==.3f && actors[0].process==.3f);
    WindowReset(session);activeInvocations={1};W::OnWipePre(outer);activeInvocations.clear();W::PruneWipes();CHECK(W::g_wipes.empty());
    for(int cause=0;cause<6;++cause){WindowReset(session);activeInvocations={1};W::OnWipePre(outer);actors[0].process=.5f;
        onRead=[&,cause]{if(cause==0)++actors[0].serial;if(cause==1)++world;if(cause==2)W::OnDisconnect();
            if(cause==3)session.mode=coop::net::Role::Client;if(cause==4)session.linked=false;
            if(cause==5){W::OnDisconnect();W::Install(&session);}};
        W::OnWipePost(outer);CHECK(W::Sent().empty());
    }
    WindowReset(session);activeInvocations={1};onRead=[]{++actors[0].serial;};W::OnWipePre(outer);CHECK(W::g_wipes.empty());
    WindowReset(session);activeInvocations={1};actors[0].process=NAN;W::OnWipePre(outer);CHECK(W::g_wipes.empty());
    W::OnDisconnect();CHECK(!W::g_session.load());W::OnWipePre(outer);CHECK(W::g_wipes.empty());
    WindowReset(session);activeInvocations={1};session.allow=[](int,const auto&){return false;};
    W::OnWipePre(outer);actors[0].process=0;W::OnWipePost(outer);CHECK(W::g_wipeSends.size()==1 && session.sent.empty());
    session.allow={};W::DrainWipeSends();CHECK(W::g_wipeSends.empty() && session.sent.size()==1 && session.sent.back().payload.value==0);
    WindowReset(session);activeInvocations={1};bool once=false;
    session.allow=[&](int,const auto&){if(!once){once=true;W::OnWipePre(outer);actors[0].process=.1f;W::OnWipePost(outer);}return true;};
    W::OnWipePre(outer);actors[0].process=.5f;W::OnWipePost(outer);CHECK(W::g_wipeSends.size()==1);
    session.allow={};W::DrainWipeSends();CHECK(W::g_wipeSends.empty() && session.sent.size()==2 && session.sent.back().payload.value==.1f);
    WindowReset(session);activeInvocations={1};session.allow=[](int,const auto&){return false;};
    W::OnWipePre(outer);actors[0].process=.3f;W::OnWipePost(outer);W::Sent()[L"window0"]=.1f;
    session.allow={};W::DrainWipeSends();CHECK(session.sent.back().payload.value==.1f);
    WindowReset(session);activeInvocations={1};session.allow=[](int,const auto&){return false;};
    for(int id=0;id<12;++id){auto call=outer;call.object=&actors[id];W::OnWipePre(call);actors[id].process=.4f;W::OnWipePost(call);}
    CHECK(W::g_wipeSends.size()==12);int attempts=0;
    session.allow=[&](int,const auto& p){++attempts;return coop::net::StringFromWireKey(p.key)==L"window9";};
    W::g_wipeCursor.clear();W::DrainWipeSends();CHECK(attempts==8 && session.sent.empty());
    attempts=0;W::DrainWipeSends();CHECK(attempts<=8 && session.sent.size()==1 && W::g_wipeSends.size()==11);
    WindowReset(session);activeInvocations={1};actors[0].readable=false;W::OnWipePre(outer);CHECK(W::g_wipes.empty());
    actors[0].readable=true;W::OnWipePre(outer);actors[0].process=NAN;W::OnWipePost(outer);CHECK(W::g_wipes.empty() && session.sent.empty());
    for(int cause=0;cause<3;++cause){WindowReset(session);activeInvocations={1};
        session.allow=[&,cause](int,const auto&){if(cause==0)W::OnDisconnect();if(cause==1)++world;if(cause==2)session.mode=coop::net::Role::Client;return false;};
        W::OnWipePre(outer);actors[0].process=.4f;W::OnWipePost(outer);W::DrainWipeSends();CHECK(session.sent.empty() && W::g_wipeSends.empty());
    }
    puts("PASS window wipe observation: independent nested snapshots, newer minimum repaint, Cancel/fault pruning, finite reads, serial/world/role/session fences and detached session teardown");
    return 0;
}
#endif
