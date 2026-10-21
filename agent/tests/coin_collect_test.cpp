#include "support_2135.h"
#include "../../src/votv-coop/src/coop/items/coingun_collect.cpp"
#include "nonentity_facts_support.h"
namespace test {
coop::net::Session session;
bool mirror=true,captured=false,coinType=true,credit=true,paramsOk=true,dispatchOK=true;
bool readable=true,readAfter=true,destroyOnCredit=false,destroyOK=false;
void* materializing=nullptr;
int32_t balance=100;int calls=0,destroys=0,reads=0;
std::function<void()> inside;
void Reset() {
    coop::coingun_sync::internal::OnDisconnectCollect();
    session.mode=coop::net::Role::Host;session.linked=session.sendOk=true;session.sent.clear();
    actor.live=true;++actor.serial;mirror=coinType=credit=paramsOk=dispatchOK=readable=readAfter=true;
    captured=destroyOnCredit=destroyOK=false;balance=100;calls=destroys=reads=0;inside={};materializing=nullptr;
    coop::element::testCoinEid=20;coop::element::testTarget=&actor;coop::element::testOutcome=coop::element::IntentOutcome::Ok;
}
}
namespace coop::coingun_sync {
const wchar_t* const kVerbNameCollect=L"actionOptionIndex";
int32_t ReadCoinPoints(void*){return 5;}
namespace internal {
coop::net::Session* Session(){return &test::session;}
bool IsCoinActor(void* p){return p&&test::coinType;}
bool IsCapturedCoin(void*){return test::captured;}
void* CoinClass(){return &test::actor;}
}
}
namespace coop::world_actor_sync {
bool IsMirroredActor(void* p){return p&&test::mirror;}
void* MaterializingActor(){return test::materializing;}
unsigned int MaterializingEid(){return 24;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f){}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){return test::paramsOk;}
bool Call(void*,ParamFrame&){
    ++test::calls;auto fn=std::move(test::inside);test::inside={};if(fn)fn();
    if(test::credit)test::balance+=5;
    if(test::destroyOnCredit)test::actor.live=false;
    return test::dispatchOK;
}
}
namespace ue_wrap::economy {
bool ReadPoints(int32_t* out){++test::reads;*out=test::balance;return test::readable&&(test::reads==1||test::readAfter);}
}
namespace ue_wrap::engine {bool DestroyActor(void*){++test::destroys;if(test::destroyOK)test::actor.live=false;return test::destroyOK;}}
namespace ue_wrap::reflection {
std::wstring ClassNameOf(void*){return L"baocoin_C";}
void* FindFunction(void*,const wchar_t*){return &test::ui;}
}
namespace ue_wrap::game_thread {bool RegisterInterceptor(void*,UFunctionInterceptor){return true;}}
namespace ue_wrap::script_gate {bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}}
int main(){
    using namespace test;namespace C=coop::coingun_sync;using V=ue_wrap::script_gate::Verdict;
    auto& players=coop::players::Registry::Get();players.local=&ui;players.puppet.actor=&actor;
    coop::net::CoinCollectPayload p{};p.elementId=20;
    auto collect=[&]{C::OnCoinCollect(reinterpret_cast<uint8_t*>(&p),sizeof(p),1,&ui);};
    Reset();inside=collect;collect();collect();CHECK(calls==1&&balance==105&&destroys==1);
    destroyOK=true;C::internal::TickCollect();CHECK(!actor.live&&C::g_retiringCoins.empty()&&calls==1&&balance==105);
    Reset();dispatchOK=false;collect();collect();CHECK(calls==1&&balance==105&&destroys==1); // fault after credit
    Reset();readable=false;collect();CHECK(calls==0&&destroys==0);readable=true;collect();CHECK(calls==1);
    Reset();credit=false;collect();collect();CHECK(calls==2&&balance==100&&destroys==0);credit=true;collect();CHECK(balance==105);
    Reset();readAfter=false;collect();collect();CHECK(calls==1&&balance==105&&destroys==0); // ambiguous completion never re-spends
    Reset();dispatchOK=credit=false;collect();collect();CHECK(calls==1&&destroys==0); // failed native completion is not proof of no spend
    Reset();inside=[] {C::internal::OnDisconnectCollect();};collect();CHECK(C::g_collected.empty()&&destroys==0);
    Reset();inside=[] {++world;};collect();CHECK(destroys==0);
    Reset();collect();++actor.serial;collect();CHECK(calls==2); // recycled address is a new coin
    Reset();coop::element::testOutcome=coop::element::IntentOutcome::OutOfReach;collect();CHECK(calls==0);
    Reset();coinType=false;collect();CHECK(calls==0);
    Reset();paramsOk=false;collect();CHECK(calls==0&&balance==100);
    Reset();ue_wrap::script_gate::Call call{};call.object=&actor;
    CHECK(C::OnCollectVerb(call)==V::Cancel&&calls==1);collect();CHECK(calls==1);
    void* overlap[]{nullptr,&ui};CHECK(C::OnCollectPre(&actor,overlap)&&calls==1);
    Reset();CHECK(C::OnCollectPre(&actor,overlap)&&calls==1);CHECK(C::OnCollectVerb(call)==V::Cancel&&calls==1);
    Reset();coop::element::testCoinEid=coop::element::kInvalidId;
    CHECK(C::OnCollectVerb(call)==V::Cancel&&C::OnCollectPre(&actor,overlap)&&calls==1); // host reservation covers coins before eid enrollment
    Reset();session.mode=coop::net::Role::Client;session.sendOk=false;
    CHECK(C::OnCollectVerb(call)==V::Cancel&&C::g_pendingCollects.size()==1&&calls==0&&destroys==0);
    session.sendOk=true;C::g_nextCollectRetryMs=0;C::internal::TickCollect();CHECK(session.sent.size()==1&&C::g_pendingCollects.empty());
    mirror=false;CHECK(C::OnCollectVerb(call)==V::Run);captured=true;CHECK(C::OnCollectVerb(call)==V::Cancel);
    C::internal::OnDisconnectCollect();CHECK(C::g_collected.empty()&&C::g_pendingCollects.empty());
    puts("PASS coin collect: E/overlap/request share one native transaction; reentrancy, fault-after-credit, proven no-op, unknown completion, no premature removal, reach/type, map coins, transport and world/session/recycled identity");
}
