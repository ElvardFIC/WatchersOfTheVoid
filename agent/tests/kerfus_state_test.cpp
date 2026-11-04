#define KERFUS_MANAGER_FIXTURE
#define PROP_CLAIM_NO_MAIN
#include "prop_claim_test.cpp"
namespace test {
bool readable=true,writeCharging=true,writeEnergy=true,updAvailable=true,updResult=true,kind=true;
int heldPackets=0,neverBound=0;
std::function<void()> deferred;
void ResetState(N::Session& s,bool client=false){ResetClaims(s);s.generations.fill(0);s.generations[1]=s.generations[31]=1;
 s.mode=client?N::Role::Client:N::Role::Host;KF::g_watched=KF::g_saidLive=false;
 readable=writeCharging=writeEnergy=updAvailable=updResult=kind=true;heldPackets=neverBound=0;deferred={};
 for(auto& o:objects){o.state=0x80;o.stateEnergy=10;}calls.clear();}
E::Element* Mirror(uint32_t id=1,int actor=0,E::ElementType type=E::ElementType::Prop){auto p=std::make_unique<E::Element>(type);p->SetTypeName("p_kerfus_C");CHECK(E::Registry::Get().RegisterMirror(id,p.get()));if(actor>=0){p->SetActor(&objects[actor],actor);objects[actor].eid=id;}auto* out=p.get();owners.push_back(std::move(p));return out;}
void Sample(int actor=0){ue_wrap::script_gate::Call call{};call.object=&objects[actor];KF::OnTickPost(call);}
void Advance(){now+=250;KF::Tick();}
N::KerfusStatePayload Packet(uint32_t id=1,uint8_t flags=3,float energy=42){N::KerfusStatePayload p{};p.elementId=id;p.flags=flags;p.energy=energy;return p;}
void Replace(int cause,bool client,uint32_t id){if(cause==0)++world;if(cause==1)++serial;if(cause==2)++objects[0].serial;if(cause==3)++objects[1].serial;
 if(cause==4){owners.clear();if(client)Mirror(id);else {for(uint32_t n=0;n<E::kHostRangeSize;++n){auto p=std::make_unique<E::Element>(E::ElementType::Prop);CHECK(E::Registry::Get().AllocHostId(p.get())!=E::kInvalidId);const bool match=p->GetId()==id;if(match){p->SetTypeName("test_C");p->SetActor(&objects[0],0);}owners.push_back(std::move(p));if(match)break;}CHECK(E::Registry::Get().EidForActor(&objects[0])==id);}}
}
}
namespace ue_wrap::script_gate {
#ifndef SERVED_PLAYER_FIXTURE
bool WatchName(const facts::Function&,int,PreFn,PostFn){test::Call("watch");return true;}
void ResolvePendingNames(){test::Call("resolve");}
bool NameWatchLive(const facts::Function&,int){test::Call("watchLive");return true;}
#endif
}
namespace coop::element::bind_wait {
bool Hold(uint32_t,uint16_t,int,Replay replay,const void* bytes,size_t size,bool coalesce){CHECK(coalesce);++test::heldPackets;std::vector<uint8_t> owned(static_cast<const uint8_t*>(bytes),static_cast<const uint8_t*>(bytes)+size);test::deferred=[=]{replay(owned.data(),owned.size(),0);};return true;}
void NoteNeverBound(uint16_t,uint32_t){++test::neverBound;}
}
namespace ue_wrap::kerfus {
using Guard=std::function<bool()>;
bool Current(const Guard& fn){return !fn||fn();}
bool IsKerfusWhile(void*,const Guard& current){if(!Current(current))return false;test::Call("classGate");return Current(current)&&test::kind;}
bool ReadActiveWhile(void* actor,bool& value,const Guard& current){if(!Current(current)||!test::readable)return false;bool v=(static_cast<test::Object*>(actor)->state&1)!=0;test::Call(test::leaseSession->mode==coop::net::Role::Host?"hostActive":"clientActive");if(!Current(current))return false;value=v;return true;}
bool ReadChargingWhile(void* actor,bool& value,const Guard& current){if(!Current(current)||!test::readable)return false;bool v=(static_cast<test::Object*>(actor)->state&2)!=0;test::Call("hostCharging");if(!Current(current))return false;value=v;return true;}
bool ReadEnergyWhile(void* actor,float& value,const Guard& current){if(!Current(current)||!test::readable)return false;float v=static_cast<test::Object*>(actor)->stateEnergy;test::Call("hostEnergy");if(!Current(current))return false;value=v;return true;}
bool WriteActiveWhile(void* actor,bool value,const Guard& current,bool* committed){if(committed)*committed=false;if(!Current(current))return false;auto& state=static_cast<test::Object*>(actor)->state;state=value?state|1:state&~1;if(committed)*committed=true;test::Call("writeActive");return Current(current);}
bool WriteChargingWhile(void* actor,bool value,const Guard& current){if(!Current(current)||!test::writeCharging)return false;auto& state=static_cast<test::Object*>(actor)->state;state=value?state|2:state&~2;test::Call("writeCharging");return Current(current);}
bool WriteEnergyWhile(void* actor,float value,const Guard& current){if(!Current(current)||!test::writeEnergy)return false;static_cast<test::Object*>(actor)->stateEnergy=value;test::Call("writeEnergy");return Current(current);}
bool RunUpdWhile(void*,bool skip,const Guard& current,bool* dispatched){CHECK(!skip);if(dispatched)*dispatched=false;if(!Current(current)||!test::updAvailable)return false;if(dispatched)*dispatched=true;test::Call("upd");return Current(current)&&test::updResult;}
bool IsKerfus(void* a){return IsKerfusWhile(a,{});}bool ReadActive(void* a,bool& v){return ReadActiveWhile(a,v,{});}bool ReadCharging(void* a,bool& v){return ReadChargingWhile(a,v,{});}bool ReadEnergy(void* a,float& v){return ReadEnergyWhile(a,v,{});}
bool WriteActive(void* a,bool v){return WriteActiveWhile(a,v,{},nullptr);}bool WriteCharging(void* a,bool v){return WriteChargingWhile(a,v,{});}bool WriteEnergy(void* a,float v){return WriteEnergyWhile(a,v,{});}bool RunUpd(void* a,bool v){return RunUpdWhile(a,v,{},nullptr);}
}
#ifndef KERFUS_STATE_NO_MAIN
int main(){using namespace test;N::Session s,other;
 for(int n=1;n<=25;++n){if(n>=20&&n<=23)continue;ResetState(s);
 switch(n){
 case 1: for(int cause=0;cause<7;++cause){ResetState(s);Register(0,E::ElementType::Prop);Sample();CHECK(!KF::g_send.empty());if(cause==0)++world;if(cause==1)++serial;if(cause==2)++s.stops;if(cause==3)s.mode=N::Role::Client;if(cause==4)s.active=false;if(cause==5)KF::g_session.store(&other);if(cause==6)worldObject=&objects[46];KF::Tick();CHECK(KF::g_host.empty()&&KF::g_send.empty()&&s.kerfusAttempts.empty());}break;
 case 2: {Register(0,E::ElementType::Prop);Sample();auto row=KF::g_host.at(&objects[0]);owners.clear();CHECK(row.identity.id&&row.identity.incarnation&&!KF::Bound(row));KF::Tick();CHECK(s.kerfusAttempts.empty());}break;
 case 3: for(int cause=0;cause<4;++cause){ResetState(s);auto* e=Register(0,E::ElementType::Prop);auto id=e->GetId();Sample();if(cause==0)++objects[0].serial;if(cause==1)++objects[1].serial;if(cause==2)e->SetActor(&objects[5],5);if(cause==3)owners.clear();CHECK(!KF::HostKnows(id));KF::Tick();CHECK(s.kerfusAttempts.empty());}break;
 case 4: objects[0].state|=1;Sample();CHECK(KF::g_host.size()==1&&H::Rows().empty()&&KF::g_send.empty());Register(0,E::ElementType::Prop);Sample();CHECK(KF::g_host.at(&objects[0]).lease&&KF::HostKnows(objects[0].eid));break;
 case 5: {Register(0,E::ElementType::Prop);objects[0].stateEnergy=std::numeric_limits<float>::quiet_NaN();Sample();CHECK(KF::g_send.empty());objects[0].stateEnergy=200;Sample();KF::Tick();CHECK(s.kerfusAttempts.size()==2&&s.kerfusAttempts[0].packet.energy==200);ResetState(s,true);Mirror();auto bad=Packet();bad.flags=4;KF::OnState(bad);bad=Packet();bad.energy=INFINITY;KF::OnState(bad);CHECK(KF::g_apply.empty());}break;
 case 6: Register(0,E::ElementType::Prop);Sample();s.endSendOk[31]=false;KF::Tick();CHECK(s.kerfusAttempts.size()==2&&KF::g_send.size()==1&&KF::g_send.begin()->second->slot==31);s.endSendOk[31]=true;Advance();CHECK(KF::g_send.empty()&&s.kerfusAttempts.size()==3);break;
 case 7: Register(0,E::ElementType::Prop);Sample();s.sendOk=false;KF::Tick();for(int i=0;i<7;++i)Advance();CHECK(s.kerfusAttempts.size()==16&&KF::g_send.size()==2);KF::Tick();CHECK(s.kerfusAttempts.size()==16);s.sendOk=true;Advance();CHECK(KF::g_send.empty());break;
 case 8: {Register(0,E::ElementType::Prop);Sample();KF::Tick();++s.generations[1];Advance();CHECK(s.kerfusAttempts.size()==3);++s.worldRevisions[1];Advance();CHECK(s.kerfusAttempts.size()==4);s.ready[1]=false;++s.worldRevisions[1];Advance();CHECK(KF::g_send.size()==1);s.ready[1]=true;Advance();CHECK(KF::g_send.empty()&&s.kerfusAttempts.size()==5);for(int cause=0;cause<2;++cause){ResetState(s);auto id=Register(0,E::ElementType::Prop)->GetId();Sample();bool fired=false;hook=[&](const std::string& name){if(name=="kerfusSend"&&!fired){fired=true;if(cause==0)++s.generations[1];else ++s.worldRevisions[1];}};KF::Tick();CHECK(fired&&KF::g_send.count(KF::SendKey(1,id)));hook={};Advance();CHECK(KF::g_send.empty());}}break;
 case 9: {Register(0,E::ElementType::Prop);Sample();bool fired=false;hook=[&](const std::string& name){if(name=="kerfusSend"&&!fired){fired=true;KF::OnPeerWorldReady(1);}};KF::Tick();CHECK(fired&&KF::g_send.count(KF::SendKey(1,objects[0].eid)));hook={};Advance();CHECK(KF::g_send.empty());}break;
 case 10: {Register(0,E::ElementType::Prop);Sample();KF::Tick();KF::OnPeerWorldReady(1);bool fired=false;hook=[&](const std::string& name){if(name=="hostActive"&&!fired){fired=true;KF::g_host.reserve(2048);auto copy=KF::g_host.at(&objects[0]);for(int i=6;i<40;++i)KF::g_host[&objects[i]]=copy;}};Advance();CHECK(fired&&KF::g_send.empty());}break;
 case 11: Register(0,E::ElementType::Prop);Sample();KF::Tick();objects[0].stateEnergy=77;readable=false;KF::OnPeerWorldReady(1);Advance();CHECK(KF::g_send.size()==1);readable=true;Advance();CHECK(KF::g_send.empty()&&s.kerfusAttempts.back().packet.energy==77);break;
 case 12: {Register(0,E::ElementType::Prop);s.generations.fill(1);Sample();KF::Tick();CHECK(s.kerfusAttempts.size()==16);Advance();CHECK(s.kerfusAttempts.size()==31&&KF::g_send.empty());bool slot31=false;for(auto& a:s.kerfusAttempts)slot31|=a.slot==31;CHECK(slot31);ResetState(s,true);for(int id=1;id<=35;++id){Mirror(id,id+5);KF::OnState(Packet(id));}KF::Tick();CHECK(Count("upd")==16);Advance();Advance();CHECK(Count("upd")==35);}break;
 case 13: {Register(0,E::ElementType::Prop);Sample();auto pub=KF::g_host.at(&objects[0]).publication;objects[0].stateEnergy=10.4f;now+=1000;Sample();CHECK(KF::g_host.at(&objects[0]).publication==pub);objects[0].stateEnergy=10.5f;Sample();CHECK(KF::g_host.at(&objects[0]).publication>pub);pub=KF::g_host.at(&objects[0]).publication;objects[0].stateEnergy=11;now+=999;Sample();CHECK(KF::g_host.at(&objects[0]).publication==pub);objects[0].state|=2;Sample();CHECK(KF::g_host.at(&objects[0]).publication>pub);pub=KF::g_host.at(&objects[0]).publication;now=1;objects[0].stateEnergy=12;Sample();CHECK(KF::g_host.at(&objects[0]).publication>pub);}break;
 case 14: {Mirror();KF::OnState(Packet());CHECK(KF::g_apply.empty());s.mode=N::Role::Client;KF::OnState(Packet(0));KF::OnState(Packet(E::kHostRangeSize));CHECK(KF::g_apply.empty());owners.clear();Register(0,E::ElementType::Prop);KF::OnState(Packet(objects[0].eid));CHECK(KF::g_apply.empty());owners.clear();Mirror(1,0,E::ElementType::Npc);KF::OnState(Packet());CHECK(KF::g_apply.empty());}break;
 case 15: ResetState(s,true);KF::OnState(Packet());CHECK(heldPackets==1&&deferred);{auto* e=Mirror(1,-1);deferred();KF::Tick();CHECK(!KF::g_apply.at(1)->done);e->SetActor(&objects[0],0);Advance();CHECK(KF::g_apply.at(1)->done&&objects[0].stateEnergy==42);KF::OnState(Packet());Advance();CHECK(Count("upd")==1);objects[0].state=0x80;objects[0].stateEnergy=0;KF::OnState(Packet());Advance();CHECK(objects[0].state==0x83&&objects[0].stateEnergy==42&&Count("upd")==2);kind=false;KF::OnState(Packet(1,0,13));Advance();CHECK(!KF::g_apply.at(1)->done);kind=true;Advance();CHECK(KF::g_apply.at(1)->done&&objects[0].stateEnergy==13);for(uint32_t id=2;id<=514;++id){Mirror(id,-1);KF::OnState(Packet(id));}CHECK(KF::g_apply.size()==KF::kClientCapacity);KF::OnState(Packet(2,0,9));CHECK(KF::g_apply.at(2)->packet.energy==9);}break;
 case 16: ResetState(s,true);Mirror();writeCharging=false;KF::OnState(Packet());KF::Tick();CHECK(Count("writeActive")==1&&!Count("writeEnergy")&&!Count("upd"));writeCharging=true;writeEnergy=false;Advance();CHECK(Count("writeActive")==1&&Count("writeCharging")==1);writeEnergy=true;Advance();CHECK(KF::g_apply.at(1)->done&&Count("upd")==1&&Count("writeCharging")==1);break;
 case 17: ResetState(s,true);Mirror();writeCharging=false;KF::OnState(Packet());KF::Tick();CHECK(objects[0].state&1);KF::OnState(Packet(1,3,99));writeCharging=true;Advance();CHECK(Count("upd")==1&&objects[0].stateEnergy==99);break;
 case 18: for(int scenario=0;scenario<3;++scenario){ResetState(s,true);Mirror();updResult=false;updAvailable=scenario!=0;KF::OnState(Packet());if(scenario==2){hook=[](const std::string& name){if(name=="upd")KF::OnState(Packet(1,3,99));};}KF::Tick();if(scenario==0){CHECK(!Count("upd")&&!KF::g_apply.at(1)->done);updAvailable=true;Advance();}hook={};Advance();CHECK(Count("upd")==1&&KF::g_apply.at(1)->done);}break;
 case 19: for(int cause=0;cause<5;++cause){ResetState(s,true);Mirror();writeCharging=false;KF::OnState(Packet());KF::Tick();Replace(cause,true,1);writeCharging=true;Advance();CHECK(!Count("upd")&&!Count("writeEnergy"));}break;
 case 24: Register(0,E::ElementType::Prop);hook=[](const std::string& name){if(name=="hostActive"){KF::OnDisconnect();CHECK(KF::g_sampling);KF::Tick();Sample();}};Sample();CHECK(KF::g_host.empty()&&!KF::g_sampling&&Count("hostActive")==1);break;
 case 25: Register(0,E::ElementType::Prop);Sample();CHECK(KF::HostKnows(objects[0].eid));KF::g_watched=true;hook=[](const std::string& name){if(name=="resolve")++world;};KF::Tick();CHECK(!Count("watchLive")&&s.kerfusAttempts.empty()&&!KF::HostKnows(objects[0].eid));break;
 }Pass("KD",n);}
 const char* stages[]={"hostActive","hostCharging","hostEnergy","kerfusSend","classGate","clientActive","writeActive","writeCharging","writeEnergy","upd"};
 for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){const bool client=stage>=4;ResetState(s,client);uint32_t id=1;if(client)Mirror();else id=Register(0,E::ElementType::Prop)->GetId();if(stage==3){Sample();KF::SyncPeers(*KF::g_bound);}if(client)KF::OnState(Packet());bool fired=false;
 hook=[&](const std::string& name){if(!fired&&name==stages[stage]){fired=true;Replace(cause,client,id);}};
 if(stage<3)Sample();else KF::Tick();CHECK(fired);hook={};
 if(stage<3){CHECK(s.kerfusAttempts.empty());auto it=KF::g_host.find(&objects[0]);CHECK(it==KF::g_host.end()||!it->second.sent);}
 if(stage==3){CHECK(s.kerfusAttempts.size()==1&&KF::g_send.count(KF::SendKey(1,id)));}
 if(client){CHECK(Count("upd")==int(stage==9));CHECK(Count("writeEnergy")==int(stage>=8));CHECK(Count("writeCharging")==int(stage>=7));CHECK(Count("writeActive")==int(stage>=6));CHECK(!KF::g_apply.at(1)->done);}
 Pass("KX",stage*5+cause+1);
 }
 ResetState(s);H::OnDisconnect();puts("PASS Kerfus manager: 21 base cases and 50 replacement cells; actual manager, registry and claims");
}

#endif
