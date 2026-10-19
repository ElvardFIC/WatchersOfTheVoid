#include "support_2135.h"
#include "../../src/votv-coop/src/coop/items/hook_retrieve.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
namespace test {
bool phase=true,saved=false,recoverOk=true,queryOk=true,kindOk=true,gateLive=true;
int queries=0,recoveries=0,destroys=0;
std::wstring key=L"hook-key";void* fn=&ui;
void* active=nullptr;
}
namespace ue_wrap::hook {
Kind KindOf(void*){return test::kindOk?Kind::Hook:Kind::Count;}
void* ActiveHookOf(void*){return test::active;}
bool ReadState(void*,State& s){s.attachedA=s.attachedB=test::phase;return true;}
bool CaptureRecord(void*,save_record::SaveRecord& r){r.key=test::key;test::Callback();
 if(test::saved)r.names={{},{L"None",L"None"}};return true;}
bool EnableAnchorQueries(void*){++test::queries;return test::queryOk;}
bool ReadAnchorEnds(void*,FVector& a,FVector& b){a={10,0,0};b={20,0,0};return true;}
bool RecoverToWorld(void* a,void*){++test::recoveries;test::Callback();
 if(test::recoverOk)static_cast<test::Obj*>(a)->live=false;return test::recoverOk;}
}
namespace ue_wrap::reflection {
const FName& NameOf(void*){static FName n{};return n;}
bool NameEquals(const FName&,const wchar_t* n){return !wcscmp(n,L"actionOptionIndex");}
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
int32_t FunctionFrameSize(void*){return sizeof(void*);}
}
namespace ue_wrap::script_gate {
bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}
bool NameWatchLive(const wchar_t*,int){return test::gateLive;}
}
namespace ue_wrap::engine {
bool DestroyActor(void* a){++test::destroys;static_cast<test::Obj*>(a)->live=false;test::Callback();return true;}
}
namespace coop::hook_sync::detail {
uint64_t NowMs(){return test::now;}
std::vector<Owned>& OwnedHooks(){static std::vector<Owned> rows;return rows;}
std::unordered_map<Key,Mirror>& Mirrors(){static std::unordered_map<Key,Mirror> rows;return rows;}
}
int main(){using namespace test;namespace S=coop::hook_retrieve;namespace A=coop::element;
 CHECK(coop::net::LaneForKind(coop::net::ReliableKind::HookRetrieve)==coop::net::Lane::Normal);
 CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::HookRetrieve));
 using V=ue_wrap::script_gate::Verdict;
 coop::net::Session session;S::Install(&session);
 coop::players::Registry::Get().local=&ui;coop::players::Registry::Get().puppet.actor=&ui;
 auto tick=[] {now+=500;S::Tick();};
 auto reset=[&]{S::OnDisconnect();actor.live=true;++actor.serial;phase=true;saved=false;recoverOk=true;
  kindOk=true;active=nullptr;S::D::OwnedHooks().clear();S::D::Mirrors().clear();
  S::Install(&session);now+=1000;A::headAllowed=A::tailAllowed=true;callback={};};
 S::Note(&actor);auto p=S::Make(key,0,1);
 session.ready=false;S::Receive(p,1);CHECK(!recoveries);session.ready=true;
 A::headAllowed=A::tailAllowed=false;S::Receive(p,1);CHECK(!recoveries);
 now+=500;A::tailAllowed=true;recoverOk=false;S::Receive(p,1);CHECK(recoveries==1&&!S::g_retired.count(key));
 now+=500;recoverOk=true;callback=[&]{S::Receive(p,2);};S::Receive(p,1);
 CHECK(recoveries==2&&!actor.live&&S::g_retired.count(key)); // reentrant competing request cannot mint again
 S::Receive(p,1);S::Receive(p,2);CHECK(recoveries==2);
 session.sendOk=false;tick();CHECK(S::g_retired.at(key).mask!=0);
 session.sendOk=true;tick();CHECK(S::g_retired.at(key).mask==0);
 S::SendAllToSlot(2);CHECK(S::g_retired.at(key).mask==4);S::OnPeerLeftSlot(2);CHECK(!S::g_retired.at(key).mask);
 reset();phase=false;saved=true;active=&actor;S::Note(&actor);CHECK(S::g_hooks.empty());
 active=nullptr;S::D::Mirror flying;flying.ref.Set(&actor);S::D::Mirrors()[1]=flying;
 S::Note(&actor);CHECK(S::g_hooks.empty());S::D::Mirrors().clear();
 S::Note(&actor);CHECK(S::g_hooks.size()==1);
 S::Receive(p,1);CHECK(recoveries==3); // saved native hook lacks phase flags
 reset();S::Note(&actor);actor.live=false;tick();CHECK(S::g_retired.count(key)); // host picked it up natively
 reset();kindOk=false;S::Note(&actor);CHECK(S::g_hooks.empty());kindOk=true;
 session.mode=coop::net::Role::Client;S::Note(&actor,true);
 S::D::Mirror mirror;mirror.ref.Set(&actor);mirror.anchored=true;S::D::Mirrors()[2]=mirror;
 gateLive=false;tick();CHECK(!queries);gateLive=true;queryOk=false;tick();CHECK(queries==1);
 queryOk=true;tick();CHECK(queries==2);tick();CHECK(queries==2);
 void* player=&ui;ue_wrap::script_gate::Call c{};c.object=&actor;c.function=&ui;c.locals=reinterpret_cast<uint8_t*>(&player);
 CHECK(S::OnVerb(c)==V::Cancel&&S::g_pending.size()==1);CHECK(S::OnVerb(c)==V::Cancel&&S::g_pending.size()==1);
 auto sent=session.sent.size();session.sendOk=false;tick();CHECK(session.sent.size()==sent);
 session.sendOk=true;tick();CHECK(session.sent.size()==sent+1);
 auto denied=S::Make(key,2,S::g_pending.at(key).packet.seq+1);S::Receive(denied,0);CHECK(S::g_pending.size()==1);
 --denied.seq;S::Receive(denied,0);CHECK(S::g_pending.empty());
 auto retired=S::Make(key,1);S::Receive(retired,2);CHECK(actor.live); // forged canonical
 S::Receive(retired,0);CHECK(!actor.live&&S::g_hooks.empty());
 actor.live=true;++actor.serial;S::Note(&actor,true);CHECK(!actor.live); // late saved/replayed spawn stays retired
 reset();S::Note(&actor,true);CHECK(S::OnVerb(c)==V::Cancel);now+=9000;S::Tick();CHECK(S::g_pending.empty());
 ++world;tick();CHECK(S::g_hooks.empty()&&S::g_retired.empty());
 reset();S::Note(&actor,true);auto nativeQueries=queries;tick();CHECK(queries==nativeQueries);
 reset();callback=[&]{actor.live=false;};CHECK(S::OnVerb(c)==V::Cancel);CHECK(S::g_hooks.empty());
 reset();S::Note(&actor,true);callback=[&]{S::OnDisconnect();};S::Receive(retired,0);
 CHECK(S::g_hooks.empty()&&S::g_retired.empty()); // no iterator survives native destroy
 reset();session.mode=coop::net::Role::Host;S::Note(&actor,true);
 callback=[&]{S::OnDisconnect();};S::Receive(p,1);CHECK(S::g_hooks.empty()&&S::g_retired.empty());
 puts("PASS actual hook retrieval: host/reach/one winner/native failure/retries/late join/saved flags/client gate/reset");
}
