#include "../../src/votv-coop/src/coop/interactables/grime_sync.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace S=coop::grime_sync;
struct Actor {int id=0,serial=1;bool live=true,readable=true;float process=.8f;ue_wrap::FVector location{};} actors[256];
uint32_t world=1;int reads=0,locations=0,writes=0;
bool paintReady=true;
std::array<coop::roster_ledger::Row,4> people{};
std::function<void()> onRead;
std::function<void()> onPaint;
std::set<uint64_t> activeInvocations;
void Callback(){auto callback=std::move(onRead);onRead={};if(callback)callback();}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a && static_cast<test::Actor*>(a)->live;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Actor*>(a)->id:-1;}
int32_t SlotSerial(int32_t id){return test::actors[id].serial;}
int32_t AllocateSlotSerial(int32_t id){return SlotSerial(id);}
bool IsLiveByIndex(void* a,int32_t id){return id>=0 && id<256 && a==&test::actors[id] && IsLive(a);}
void* ObjectAt(int32_t id){return &test::actors[id];}
const FName& NameOf(void*){static FName name{};return name;}
bool NameStartsWith(const FName&,const wchar_t*){return false;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::engine {bool TryGetActorLocation(void* actor,FVector& out){++test::locations;out=static_cast<test::Actor*>(actor)->location;return true;}}
namespace ue_wrap::grime {
bool EnsureResolved(){return true;}bool IsGrime(void* a){return reflection::IsLive(a);}
bool ReadProcess(void* a,float& out){++test::reads;auto& actor=*static_cast<test::Actor*>(a);out=actor.process;test::Callback();return actor.readable;}
bool ReadEndedProcess(void* a,int32_t,int32_t,float& out){return ReadProcess(a,out);}
bool ReadType(void*,int32_t& out){out=0;return true;}
bool WriteProcessAndApply(void* a,float value,const std::function<bool()>& current){++test::writes;static_cast<test::Actor*>(a)->process=value;
    auto callback=std::move(test::onPaint);test::onPaint={};if(callback)callback();return test::paintReady && (!current || current());}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
    :ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode){}
}
namespace ue_wrap::script_gate {bool WatchName(const facts::Function&,int,PreFn,PostFn){return true;}void ResolvePendingNames(){}
bool IsInvocationActive(uint64_t id){return test::activeInvocations.contains(id);}}
namespace coop::roster_ledger {const Row& Get(int slot){return test::people.at(slot);}}
namespace coop::element::scan_hub {void Register(const Consumer&) {}}
namespace coop::element::death_seam {bool Install(){return true;}bool SubscribeClass(const wchar_t*,ClassHandler){return true;}}
#ifndef DRINK_SPILL_ACTUAL_TEST
namespace coop::drink_spill_sync {std::wstring KeyFor(void*){return {};}bool IsPredicted(void*){return false;}bool ExcludeFromStaticIndex(void*){return false;}
void ObserveExisting(void*){}void IndexComplete(uint32_t){}void OnRemoved(void*){}}
#endif
namespace test {
void Reset(coop::net::Session& session,int count=1){
    S::OnDisconnect();++world;session={};onRead=onPaint={};activeInvocations.clear();paintReady=true;reads=locations=writes=0;
    {std::lock_guard<std::mutex> lock(S::g_indexMutex);S::g_byKey.clear();}
    S::g_posKeyByActor.clear();S::g_indexGen=0;S::Install(&session);
    for(int slot=0;slot<4;++slot)people[slot].playerNo=uint16_t(slot+10);
    for(int id=0;id<256;++id){actors[id]={};actors[id].id=id;actors[id].location={float(id*10),0,0};}
    S::HubPassBegin(nullptr,true);for(int id=0;id<count;++id)S::HubMatch(nullptr,&actors[id]);S::HubPassComplete(nullptr,true,world);
}
std::wstring Key(int id=0){return S::PosKey(&actors[id]);}
void Pump(int count=1){for(int i=0;i<count;++i)S::Tick();}
}
int main(){using namespace test;coop::net::Session session;
    Reset(session,100);S::QueueConnectBroadcastForSlot(1);CHECK(session.sent.empty());
    const auto indexedLocations=locations;int budget=1;
    session.allow=[&](int slot,const auto&){CHECK(slot==1);return budget-->0;};Pump();
    CHECK(session.sent.size()==1 && S::g_joins[1].keys.size()==99 && reads<=8 && locations==indexedLocations);
    session.allow={};Pump(20);CHECK(session.sent.size()==100 && S::g_joins[1].keys.empty());
    std::set<std::wstring> unique;for(const auto& sent:session.sent){CHECK(sent.slot==1 && sent.payload.adopt==1 && sent.payload.value==.8f);
        unique.insert(coop::net::StringFromWireKey(sent.payload.key));}CHECK(unique.size()==100);
    Reset(session,20);S::QueueConnectBroadcastForSlot(1);S::QueueConnectBroadcastForSlot(2);
    session.allow=[](int slot,const auto&){return slot!=1;};Pump(5);CHECK(session.sent.size()==20 && S::g_joins[1].keys.size()==20);
    actors[0].process=.15f;session.allow={};Pump(5);bool fresh=false;
    for(const auto& sent:session.sent)if(sent.slot==1 && coop::net::StringFromWireKey(sent.payload.key)==Key())fresh=sent.payload.value==.15f;CHECK(fresh);
    Reset(session,10);actors[0].readable=false;S::QueueConnectBroadcastForSlot(1);Pump(3);
    CHECK(session.sent.size()==9 && S::g_joins[1].keys.size()==1);const auto before=reads;Pump();CHECK(reads==before+1);
    actors[0].readable=true;actors[0].process=.2f;Pump();CHECK(session.sent.size()==10 && session.sent.back().payload.value==.2f);
    Reset(session);S::g_indexGen=0;S::QueueConnectBroadcastForSlot(1);Pump();CHECK(S::g_joins[1].waiting && session.sent.empty());
    S::g_indexGen=world;Pump();CHECK(session.sent.size()==1);
    Reset(session,0);S::Floor()[L"away"]=.4f;S::g_parked.Get()[{}]={L"parked",.3f};S::QueueConnectBroadcastForSlot(1);Pump();
    CHECK(session.sent.size()==2);for(const auto& p:session.sent)CHECK(p.payload.value==.4f || p.payload.value==.3f);
    Reset(session);S::Floor()[Key()]=.2f;S::QueueConnectBroadcastForSlot(1);Pump();CHECK(session.sent.size()==1 && session.sent.front().payload.value==.2f);
    Reset(session);S::QueueConnectBroadcastForSlot(1);onRead=[]{S::Floor()[Key()]=.1f;};Pump();CHECK(session.sent.size()==1 && session.sent.front().payload.value==.1f);
    Reset(session);actors[0].process=NAN;S::QueueConnectBroadcastForSlot(1);Pump();CHECK(session.sent.empty());actors[0].process=.6f;Pump();CHECK(session.sent.size()==1);
    for(int cause=0;cause<6;++cause){Reset(session);S::QueueConnectBroadcastForSlot(1);
        onRead=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++world;if(cause==2)++people[1].playerNo;
            if(cause==3)++actors[0].serial;if(cause==4)session.mode=coop::net::Role::Client;if(cause==5)session.ready[1]=false;};
        Pump();CHECK(session.sent.empty());
    }
    Reset(session);S::QueueConnectBroadcastForSlot(1);onRead=[]{std::lock_guard<std::mutex> lock(S::g_indexMutex);S::g_byKey.clear();};
    Pump();CHECK(session.sent.empty());S::Floor()[Key()]=0;Pump();CHECK(session.sent.size()==1 && session.sent.front().payload.value==0);
    Reset(session);S::QueueConnectBroadcastForSlot(1);session.ready[1]=false;Pump();CHECK(session.sent.empty());session.ready[1]=true;Pump();CHECK(session.sent.size()==1);
    Reset(session);S::QueueConnectBroadcastForSlot(1);S::OnPeerLeftSlot(1);Pump();CHECK(session.sent.empty());
    Reset(session);S::QueueConnectBroadcastForSlot(1);session.allow=[](int,const auto&){S::OnDisconnect();return false;};Pump();CHECK(!S::g_session.load() && S::g_joins[1].keys.empty());
    Reset(session);S::QueueConnectBroadcastForSlot(1);bool once=false;
    session.allow=[&](int,const auto&){if(!once){once=true;++people[1].playerNo;S::QueueConnectBroadcastForSlot(1);}return false;};Pump();
    CHECK(S::g_joins[1].person==people[1].playerNo && S::g_joins[1].keys.size()==1);session.allow={};Pump();CHECK(session.sent.size()==1);
    Reset(session);S::QueueConnectBroadcastForSlot(1);session.allow=[&](int,const auto&){Pump();return true;};Pump();CHECK(session.sent.size()==1);
    Reset(session);session.mode=coop::net::Role::Client;S::QueueConnectBroadcastForSlot(1);Pump();CHECK(session.sent.empty());
    Reset(session);S::QueueConnectBroadcastForSlot(1);session.allow=[](int,const auto&){return false;};Pump();
    actors[0].process=.3f;S::OnGrimeEnd({&actors[0],0,actors[0].serial,true});session.allow={};Pump();
    CHECK(session.sent.size()==1 && session.sent.front().payload.value==.3f);
    // Live minimum updates, including the final wipe, survive a refused queue.
    Reset(session);const auto key=Key();session.allow=[](int,const auto&){return false;};
    S::Lower(key,.3f);S::Send(key,.3f);CHECK(S::g_falls.size()==1);
    S::Lower(key,0);S::Send(key,0);CHECK(S::g_falls.at(key).value==0);
    session.allow={};Pump();CHECK(session.sent.size()==1 && session.sent.front().payload.value==0 && !session.sent.front().payload.adopt && S::g_falls.empty());
    Reset(session);S::Lower(Key(),.4f);session.ready={false,false,false,false};S::Send(Key(),.4f);
    Pump();CHECK(session.sent.empty() && S::g_falls.size()==1);session.ready[1]=true;Pump();CHECK(session.sent.size()==1 && S::g_falls.empty());
    Reset(session);bool lowered=false;
    session.allow=[&](int,const auto&){if(!lowered){lowered=true;S::Lower(Key(),0);S::Send(Key(),0);}return true;};
    S::Lower(Key(),.5f);S::Send(Key(),.5f);CHECK(session.sent.size()==1 && S::g_falls.contains(Key()) && S::g_falls.at(Key()).value==0);
    session.allow={};Pump();CHECK(session.sent.size()==2 && session.sent.back().payload.value==0 && S::g_falls.empty());
    for(int cause=0;cause<4;++cause){Reset(session);
        session.allow=[&,cause](int,const auto&){if(cause==0)S::OnDisconnect();if(cause==1)++world;
            if(cause==2)session.mode=coop::net::Role::Client;if(cause==3)session.linked=false;return false;};
        S::Lower(Key(),0);S::Send(Key(),0);CHECK(session.sent.empty());
        if(cause==0)CHECK(S::g_falls.empty());Pump();CHECK(session.sent.empty());
    }
    Reset(session);session.mode=coop::net::Role::Client;session.allow=[](int,const auto&){return false;};
    S::Lower(Key(),0);S::Send(Key(),0);CHECK(S::g_falls.size()==1);session.allow={};Pump();CHECK(session.sent.size()==1 && S::g_falls.empty());
    Reset(session);const auto paintKey=Key();paintReady=false;S::Lower(paintKey,.2f);
    S::ApplyResolved(&actors[0],paintKey,.2f,"test",true);CHECK(actors[0].process==.2f && S::g_paints.size()==1 && writes==1);
    Pump();CHECK(writes==2 && S::g_paints.size()==1);paintReady=true;Pump();CHECK(writes==3 && S::g_paints.empty());
    Reset(session);paintReady=false;S::Lower(Key(),.4f);S::ApplyResolved(&actors[0],Key(),.4f,"test",true);
    onPaint=[]{coop::net::KeyedScalarPayload p{};coop::net::WireKeyFromString(Key(),p.key);p.value=.1f;S::OnReliable(p,1);};
    Pump();CHECK(actors[0].process==.1f && S::g_paints.size()==1 && S::g_paints.begin()->second.target==.1f);
    paintReady=true;Pump();CHECK(S::g_paints.empty() && actors[0].process==.1f);
    Reset(session);paintReady=false;S::Lower(Key(),.4f);S::ApplyResolved(&actors[0],Key(),.4f,"test",true);
    ++actors[0].serial;const auto beforePaint=writes;Pump();CHECK(writes==beforePaint && S::g_paints.empty());
    // Nested clean/fix calls own independent observations, including non-grime calls.
    Reset(session,2);ue_wrap::script_gate::Call outer{},inner{},unrelated{};
    outer.object=&actors[0];outer.invocation=1;inner.object=&actors[1];inner.invocation=2;unrelated.invocation=3;
    activeInvocations={1,2,3};S::OnLowerPre(outer);S::OnLowerPre(unrelated);S::OnLowerPost(unrelated);
    S::OnLowerPre(inner);actors[1].process=.6f;S::OnLowerPost(inner);actors[0].process=.7f;S::OnLowerPost(outer);
    CHECK(S::Floor().at(Key(0))==.7f && S::Floor().at(Key(1))==.6f && S::g_lowerCaptures.empty());
    Reset(session);inner.object=&actors[0];activeInvocations={1,2};S::OnLowerPre(outer);S::OnLowerPre(inner);
    actors[0].process=.4f;S::OnLowerPost(inner);actors[0].process=.6f;S::OnLowerPost(outer);
    CHECK(S::Floor().at(Key())==.4f && actors[0].process==.4f);
    Reset(session);activeInvocations={1};S::OnLowerPre(outer);activeInvocations.clear(); // Cancel/fault skipped POST
    activeInvocations.insert(2);S::OnLowerPre(inner);CHECK(S::g_lowerCaptures.size()==1);
    actors[0].process=.5f;S::OnLowerPost(inner);CHECK(S::g_lowerCaptures.empty() && S::Floor().at(Key())==.5f);
    for(int cause=0;cause<5;++cause){Reset(session);activeInvocations={1};S::OnLowerPre(outer);actors[0].process=.5f;
        onRead=[&,cause]{if(cause==0)++actors[0].serial;if(cause==1)++world;if(cause==2)S::OnDisconnect();if(cause==3)session.mode=coop::net::Role::Client;
            if(cause==4){S::OnDisconnect();S::Install(&session);}};
        S::OnLowerPost(outer);CHECK(session.sent.empty());
    }
    Reset(session);activeInvocations={1};S::OnLowerPre(outer);actors[0].process=.7f;
    onRead=[]{S::Lower(Key(),.1f);actors[0].process=.1f;};S::OnLowerPost(outer);CHECK(S::Floor().at(Key())==.1f && actors[0].process==.1f);
    puts("PASS grime nested observation: independent invocation snapshots, non-grime callbacks, same-actor newer minimum, skipped-POST pruning and world/session/serial/role fences");
    puts("PASS grime join: bounded indexed snapshots, independent recipients, fresh retry/floors/parked values, late index/field readiness, no repeated key work or location scans, native/send reentry and recipient/serial/world/session fences");
    return 0;
}
