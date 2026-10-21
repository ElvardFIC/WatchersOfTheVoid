#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/bucket_sync.cpp"
namespace test {
ue_wrap::building_material::Water water{35,0,false};
bool hand=false,mirror=false,driven=false,quiesced=true,sendOk=true;
int publishes=0,paints=0,repairs=0;std::wstring key=L"bucket";
ue_wrap::building_material::Stock stock{};
bool coopDispatch=false;Obj outDry{};
int persisted=0;
std::vector<std::pair<void*,void*>> active;
std::function<void()> onRead,onRepair,onPaint,onContents;bool contentsOk=true;
std::vector<void*> published;
void Run(std::function<void()>& callback){auto f=std::move(callback);callback={};if(f)f();}
}
namespace ue_wrap::building_material {
bool IsTrowel(void*){return test::stock.kind==StockKind::Trowel;}
bool ReadWater(void* a,Water& s){if(!reflection::IsLive(a)||test::stock.kind!=StockKind::None)return false;s=test::water;test::Run(test::onRead);return true;}
bool ReadStock(void* a,Stock& s){if(!reflection::IsLive(a)||test::stock.kind==StockKind::None)return false;s=test::stock;test::Run(test::onRead);return true;}
bool RepairBucketEmpty(void*){++test::repairs;test::Run(test::onRepair);return true;}
bool RepaintBucket(void*,const std::function<bool()>& current){if(current && !current())return false;++test::paints;test::Run(test::onPaint);return !current || current();}
bool TickBucketContents(void*,float seconds){CHECK(seconds==.016f);test::Run(test::onContents);return test::contentsOk;}
}
namespace ue_wrap::inventory {
bool PersistHeldMaterialStock(void*,const std::function<bool()>& current){if(!current())return false;++test::persisted;return true;}
}
namespace coop::hand_item {
void* LocalHandActor(){return test::hand?&test::actor:nullptr;}
bool IsHandAxisActor(void*){return test::hand||test::mirror;}
bool IsMirrorActor(void*){return test::mirror;}
}
namespace coop::remote_prop {bool IsActorUnderAnyDrive(void*){return test::driven;}}
namespace coop::join_membership_sweep {bool HasLoadTailQuiesced(){return test::quiesced;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}}
namespace coop::prop_save_data {
bool Covers(void*){return true;}
bool Publish(net::Session*,void* a,const std::wstring&){++test::publishes;test::published.push_back(a);test::Callback();return test::sendOk;}
}
namespace ue_wrap::reflection {
bool InCoopDispatch(){return test::coopDispatch;}
const FName& NameOf(void*){static FName n{};return n;}
bool NameEquals(const FName&,const wchar_t* n){return !wcscmp(n,L"loadData");}
int32_t FindParamOffset(void*,const wchar_t* n){return !wcscmp(n,L"to")?int(sizeof(void*)):0;}
int32_t FunctionFrameSize(void*){return test::stock.kind==ue_wrap::building_material::StockKind::None?sizeof(float):sizeof(test::Obj);}
}
namespace ue_wrap::script_gate {
bool IsBodyActive(void* f,void*){for(auto& a:test::active)if(f&&a.first==f)return true;return false;}
bool IsObjectBodyActive(void* f,void* o){for(auto& a:test::active)if(f&&o&&a==std::make_pair(f,o))return true;return false;}
uint8_t* OutParamPtr(const Call&,int32_t off){CHECK(off==0);return reinterpret_cast<uint8_t*>(&test::outDry);}
}
int main(){using namespace test;namespace S=coop::bucket_sync;using V=ue_wrap::script_gate::Verdict;
 coop::net::Session session;S::Install(&session);
 const auto watched=[](const wchar_t* cls,const wchar_t* fn){for(auto& p:ue_wrap::script_gate::watchedBodies)if(p.first==cls&&p.second==fn)return true;return false;};
 CHECK(watched(L"prop_concreteBucket_C",L"ReceiveTick")&&watched(L"prop_concreteBucket_C",L"takeConcrete"));
 CHECK(watched(L"prop_cementBag_C",L"transferUnit")&&watched(L"prop_wallfixer_C",L"updConcrete"));
 CHECK(watched(L"prop_sponge_C",L"enterWater")&&watched(L"prop_sponge_C",L"wash")&&!watched(L"prop_sponge_C",L"enteredTheWater"));
 float seconds=.016f;ue_wrap::script_gate::Call c{};c.object=&actor;c.callerFunction=&ui;c.function=&ui;c.locals=reinterpret_cast<uint8_t*>(&seconds);
 S::BeforeUpd(c);CHECK(repairs==1);S::AfterLoad(c);S::Tick();CHECK(publishes==0);
 S::AfterTick(c);S::Tick();CHECK(publishes==0); // stable baseline stays quiet
 now+=300;water.height=30;S::AfterTick(c);S::Tick();CHECK(publishes==1);
 water.height=28;S::AfterUse(c);now+=300;S::Tick();CHECK(publishes==2); // final fast dip is not coalesced away
 auto paintBefore=paints;driven=true;CHECK(S::BeforeTick(c)==V::Cancel&&paints==paintBefore+1);S::BeforeTick(c);CHECK(paints==paintBefore+1);
 coop::local_prop_hold::held=true;CHECK(S::BeforeTick(c)==V::Run);coop::local_prop_hold::held=false;driven=false;
 session.mode=coop::net::Role::Client;CHECK(S::BeforeTick(c)==V::Cancel);
 quiesced=false;CHECK(S::BeforeTick(c)==V::Run);water.height=20;S::AfterUse(c);now+=400;S::Tick();CHECK(publishes==2);quiesced=true;
 hand=true;CHECK(S::BeforeTick(c)==V::Run);auto savedBefore=persisted;
 S::AfterUse(c);S::Tick();CHECK(publishes==2&&persisted==savedBefore+1);hand=false;
 S::AfterUse(c);sendOk=false;S::Tick();CHECK(publishes==3);now+=400;sendOk=true;S::Tick();CHECK(publishes==4);
 water.height=19;S::AfterUse(c);now+=400;callback=[&]{water.height=17;S::AfterUse(c);};S::Tick();CHECK(S::Find(&actor)->dirty);
 now+=400;S::Tick();CHECK(!S::Find(&actor)->dirty);
 water.height=12;S::AfterUse(c);now+=400;callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session);
 S::Install(&session);S::AfterUse(c);++actor.serial;actor.live=false;now+=400;auto old=publishes;S::Tick();CHECK(publishes==old);
 actor.live=true;S::AfterUse(c);++world;S::Tick();CHECK(publishes==old);
 // Concrete uses the same real carrier/retry loop, with native save fields.
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);
 stock={ue_wrap::building_material::StockKind::ConcreteBucket,20,false};
 S::AfterStockLoad(c);S::Tick();CHECK(publishes==old);
 stock.units=19;S::AfterStockChange(c);S::Tick();CHECK(publishes==old+1);
 stock.dry=true;S::AfterStockChange(c);now+=400;S::Tick();CHECK(publishes==old+2);
 CHECK(S::BeforeCementTick(c)==V::Run);driven=true;CHECK(S::BeforeCementTick(c)==V::Cancel);driven=false;
 session.mode=coop::net::Role::Client;CHECK(S::BeforeCementTick(c)==V::Cancel);
 quiesced=false;CHECK(S::BeforeCementTick(c)==V::Run);quiesced=true;
 coop::local_prop_hold::held=true;CHECK(S::BeforeCementTick(c)==V::Run);
 active={{&ui,&actor}};CHECK(S::IsMaterialBirth());
 coopDispatch=true;CHECK(!S::IsMaterialBirth());coopDispatch=false;
 coop::local_prop_hold::held=false;CHECK(!S::IsMaterialBirth());
 active.clear();CHECK(!S::IsMaterialBirth());
 // Empty native take must overwrite BOTH local and caller out storage, retaining unrelated bits.
 Obj frame{};c.locals=reinterpret_cast<uint8_t*>(&frame);stock.units=0;outDry.flags=0x80;frame.flags=0x80;
 CHECK(S::BeforeTake(c)==V::Cancel&&outDry.flags==0x81&&frame.flags==0x81);
 stock.units=-3;CHECK(S::BeforeTake(c)==V::Cancel);
 stock.units=1;CHECK(S::BeforeTake(c)==V::Run);
 stock.units=0;fields=false;CHECK(S::BeforeTake(c)==V::Run);fields=true;
 // A bag transfer observes both endpoints, including when the receiver is neither endpoint.
 S::g_rows={};stock={ue_wrap::building_material::StockKind::CementBag,8,false};
 void* args[2]={&actor,&ui};c.locals=reinterpret_cast<uint8_t*>(args);
 S::AfterBagTransfer(c);CHECK(S::Find(&actor)->dirty&&S::Find(&ui)->dirty);
 S::AfterStockLoad(c);CHECK(!S::Find(&actor)->dirty); // received load does not echo
 // Selected hand is not a world record, but its exact native use bracket admits replacements.
 hand=true;S::BeforeBagUse(c);active={{&ui,&actor}};CHECK(S::IsMaterialBirth());
 active={{&ui,&ui}};CHECK(!S::IsMaterialBirth());active.clear();CHECK(!S::IsMaterialBirth());hand=false;
 Obj takeFn{3},toolFn{4};c.function=&takeFn;stock={ue_wrap::building_material::StockKind::ConcreteBucket,1,false};
 S::BeforeTake(c);c.function=&toolFn;S::BeforeTrowelUse(c);hand=true;
 active={{&toolFn,&actor}};CHECK(!S::IsMaterialBirth());
 active.emplace_back(&takeFn,&ui);CHECK(S::IsMaterialBirth());
 active={{&toolFn,&ui},{&takeFn,&ui}};CHECK(!S::IsMaterialBirth());active.clear();hand=false;
 S::g_rows={};stock={ue_wrap::building_material::StockKind::Trowel,4,false};
 S::AfterStockLoad(c);stock.units=0;S::AfterStockChange(c);CHECK(S::Find(&actor)->dirty);
 S::g_rows={};stock={ue_wrap::building_material::StockKind::Sponge,0,false,0.f};
 S::AfterStockLoad(c);old=publishes;stock.wetness=1;S::AfterStockChange(c);S::Tick();CHECK(publishes==old+1);
 stock.wetness=.97f;S::AfterStockChange(c);stock.wetness=.94f;S::AfterStockChange(c);now+=400;S::Tick();
 CHECK(publishes==old+2&&S::Find(&actor)->last.stock.wetness==.94f);
 S::g_rows={};stock={ue_wrap::building_material::StockKind::ConcreteBucket,5,false};stock.dryTimer=10;
 S::AfterStockLoad(c);old=publishes;stock.dryTimer=10.5f;S::AfterStockChange(c);now+=400;S::Tick();CHECK(publishes==old);
 stock.dryTimer=11.1f;S::AfterStockChange(c);now+=400;S::Tick();CHECK(publishes==old+1);
 S::g_rows={};stock={ue_wrap::building_material::StockKind::Sponge,0,false,.75f};stock.soapAmount=20;
 S::AfterStockLoad(c);old=publishes;stock.soapAmount=15;S::AfterStockChange(c);now+=400;S::Tick();
 CHECK(publishes==old+1&&S::Find(&actor)->last.stock.soapAmount==15);
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);S::g_rows={};
 stock.units=7;onRead=[&]{stock.units=4;S::AfterStockChange(c);};S::AfterStockChange(c);
 CHECK(S::Find(&actor)->last.stock.units==4); // newer native observation wins the interrupted read
 now+=400;old=publishes;onRead=[]{S::OnDisconnect();};S::Tick();CHECK(publishes==old && !S::g_session);
 S::Install(&session);S::AfterStockChange(c);now+=400;onRead=[]{++world;};S::Tick();CHECK(publishes==old && !S::Find(&actor)->dirty);
 auto beforePaint=paints;onRepair=[]{S::OnDisconnect();};S::AfterUse(c);CHECK(paints==beforePaint && !S::g_session);
 S::Install(&session);session.mode=coop::net::Role::Client;stock={};c.locals=reinterpret_cast<uint8_t*>(&seconds);
 contentsOk=false;onContents=[]{S::OnDisconnect();};CHECK(S::BeforeTick(c)==V::Cancel && paints==beforePaint);contentsOk=true;
 S::Install(&session);session.mode=coop::net::Role::Host;stock={ue_wrap::building_material::StockKind::ConcreteBucket,5,false};
 // Continuously changing early rows cannot starve a ready row at the end of the bounded pool.
 CHECK(S::Current());Obj late{2};S::g_rows={};S::g_cursor=0;S::g_rows[0].actor.Set(&actor);S::g_rows[1].actor.Set(&ui);S::g_rows.back().actor.Set(&late);
 published.clear();S::g_rows.back().dirty=true;
 for(int i=0;i<2;++i){for(size_t row:{size_t(0),size_t(1)}){S::g_rows[row].dirty=true;S::g_rows[row].sendAt=0;}
  now+=1000;S::Tick();}
 CHECK(std::find(published.begin(),published.end(),&late)!=published.end());
 S::OnDisconnect();CHECK(S::BeforeTick(c)==V::Run);
 S::Install(&session);stock.kind=ue_wrap::building_material::StockKind::Trowel;hand=true;mirror=false;c.fromOurCode=false;
 savedBefore=persisted;S::AfterStockChange(c);CHECK(persisted==savedBefore+1);
 hand=false;S::AfterStockChange(c);CHECK(persisted==savedBefore+1);
 hand=true;c.fromOurCode=true;S::AfterStockChange(c);CHECK(persisted==savedBefore+1);c.fromOurCode=false;
 for(auto kind:{ue_wrap::building_material::StockKind::ConcreteBucket,ue_wrap::building_material::StockKind::CementBag,
                ue_wrap::building_material::StockKind::Sponge}){
  stock.kind=kind;savedBefore=persisted;S::AfterStockChange(c);CHECK(persisted==savedBefore+1);
 }
 stock.kind=ue_wrap::building_material::StockKind::ConcreteBucket;savedBefore=persisted;
 S::AfterTick(c);CHECK(persisted==savedBefore+1);
 stock.kind=ue_wrap::building_material::StockKind::CementBag;c.locals=reinterpret_cast<uint8_t*>(args);
 savedBefore=persisted;S::AfterBagTransfer(c);CHECK(persisted==savedBefore+1);hand=false;
 CHECK(!S::IsMaterialBirth());
 puts("PASS cement/sponge: real callback registration, amount/dry/wet publications, passive tick, empty out params, two transfer endpoints, exact local use/pour provenance, load no echo");
 puts("PASS actual bucket controller: load/no echo, changed/final state, authority/hand/load gates, retry, reentry, identity/world/teardown");
}
