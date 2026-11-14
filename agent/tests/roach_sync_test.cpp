#include "../../src/votv-coop/src/coop/creatures/roach_sync.cpp"
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <limits>

namespace test {
namespace C=coop::roach_sync;namespace N=coop::net;
struct Object {void* master=nullptr;C::FArrayRaw array{};C::Vec3 location{},scale{1,1,1};} objects[180];
void* slots[256];int serials[180];bool alive[180];uint32_t world=1;
N::Session session;bool gameThread=true,metadata=true,mutate=true,callOk=true,getOk=true;
int calls[12],nextComponent=20;
const wchar_t* failParam=nullptr;std::function<void(void*,int)> callback;
void* gm=&objects[0];
void Population(int count) {
    std::fill(std::begin(slots),std::end(slots),nullptr);
    nextComponent=20;
    for(int i=0;i<count;++i){slots[i]=&objects[nextComponent++];}
    objects[1].array={slots,count,256};objects[2].array={slots,count,256};
}
void Reset(bool client=false,int count=1) {
    C::OnDisconnect();session=N::Session{};session.mode=client?N::Role::Client:N::Role::Host;
    for(int i=0;i<180;++i){objects[i]={};serials[i]=1;alive[i]=true;}
    std::fill(std::begin(calls),std::end(calls),0);
    gm=&objects[0];objects[0].master=&objects[1];Population(count);
    gameThread=metadata=mutate=callOk=getOk=true;failParam=nullptr;callback={};world=1;N::testNow=10000;
    C::Install(&session);C::ResolvePass();
}
N::RoachStatePayload Page(uint32_t seq=1,int total=1,int page=0) {
    N::RoachStatePayload p{};p.seq=seq;p.totalCount=uint8_t(total);p.page=uint8_t(page);
    p.pageCount=uint8_t(std::max(1,(total+11)/12));p.entryCount=uint8_t(std::max(0,std::min(12,total-page*12)));
    for(int i=0;i<p.entryCount;++i)p.entries[i]={float(page*12+i+10),2,3,2};return p;
}
void Receive(uint32_t seq=1,int count=1){for(int i=0;i<std::max(1,(count+11)/12);++i)C::OnState(Page(seq,count,i));}
void Apply(){C::Tick();}
int Mutations(){return calls[6]+calls[7];}
template<class F>void Case(const char* id,F fn){fn();printf("PASS %s\n",id);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<180;++i)if(p==&test::objects[i])return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<180&&p==&test::objects[i]&&test::alive[i];}
bool IsLive(void* p){return IsLiveByIndex(p,InternalIndexOf(p));}
int32_t SlotSerial(int32_t i){return i>=0&&i<180?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p==&test::objects[0]?&test::objects[3]:p==&test::objects[1]||p==&test::objects[2]?&test::objects[4]:&test::objects[5];}
void* FindClass(const wchar_t* name){if(!test::metadata)return nullptr;return !wcscmp(name,L"mainGamemode_C")?&test::objects[3]:!wcscmp(name,L"cockroachMaster_C")?&test::objects[4]:&test::objects[5];}
int32_t FindPropertyOffset(void*,const wchar_t* name){return !wcscmp(name,L"cockroachMaster")?offsetof(test::Object,master):offsetof(test::Object,array);}
void* FindFunction(void*,const wchar_t* name){
    const wchar_t* names[]={L"addRoach",L"deleteRoach",L"K2_GetComponentLocation",L"K2_GetComponentScale",L"K2_SetWorldLocation",L"SetWorldScale3D"};
    for(int i=0;i<6;++i)if(!wcscmp(name,names[i]))return &test::objects[6+i];return nullptr;
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return &test::world;}void* WorldOf(void*){return &test::world;}
}
namespace ue_wrap::world_singleton {void* Gamemode(){return test::gm;}}
namespace ue_wrap::game_thread {bool IsGameThread(){return test::gameThread;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(32){}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t size){
    if(test::failParam&&!wcscmp(name,test::failParam))return false;
    int off=!wcscmp(name,L"Size")?12:!wcscmp(name,L"bypassCheck")?24:!wcscmp(name,L"crush")?4:0;
    std::memcpy(buf_.data()+off,value,size);return true;
}
bool ParamFrame::GetRaw(const wchar_t*,void* value,int32_t size)const{
    if(!test::getOk)return false;std::memcpy(value,buf_.data(),size);return true;
}
bool Call(void* object,ParamFrame& frame){
    using namespace test;const int fn=reflection::InternalIndexOf(frame.function());assert(fn>=6&&fn<12);++calls[fn];
    auto* o=static_cast<Object*>(object);
    if(fn==8)std::memcpy(frame.data(),&o->location,sizeof(o->location));
    if(fn==9)std::memcpy(frame.data(),&o->scale,sizeof(o->scale));
    if(fn==10&&mutate)std::memcpy(&o->location,frame.data(),sizeof(o->location));
    if(fn==11&&mutate)std::memcpy(&o->scale,frame.data(),sizeof(o->scale));
    if(fn==7&&mutate){int index=0;std::memcpy(&index,frame.data(),4);assert(index>=0&&index<o->array.Num);
        auto** data=static_cast<void**>(o->array.Data);int idx=reflection::InternalIndexOf(data[index]);if(idx>=0)alive[idx]=false;data[index]=nullptr;
    }
    if(fn==6&&mutate){assert(nextComponent<180);auto* comp=&objects[nextComponent++];alive[reflection::InternalIndexOf(comp)]=true;
        std::memcpy(&comp->location,frame.data(),12);std::memcpy(&comp->scale,static_cast<uint8_t*>(frame.data())+12,12);
        auto** data=static_cast<void**>(o->array.Data);data[o->array.Num++]=comp;
    }
    if(callback)callback(object,fn);return callOk;
}
}
int main(){using namespace test;
    Case("R01 session/world reset",[]{Reset(true);Receive();++world;C::Tick();assert(!C::g_pending.active);});
    Case("R02 metadata incarnations",[]{Reset();assert(C::Resolved());++serials[8];assert(!C::Resolved());N::testNow+=2000;C::ResolvePass();assert(C::Resolved());});
    Case("R03 retry metadata",[]{Reset();C::OnDisconnect();metadata=false;C::Install(&session);for(int i=0;i<8;++i){N::testNow+=2000;C::ResolvePass();}assert(!C::Resolved());metadata=true;N::testNow+=2000;C::ResolvePass();assert(C::Resolved());});
    Case("R04 current master classes",[]{Reset();auto old=C::g_gmClass;C::g_gmClass.Set(&objects[4]);assert(!C::Master());C::g_gmClass=old;assert(C::Master()==&objects[1]);});
    Case("R05 array shape",[]{for(int mode=0;mode<4;++mode){Reset();auto& a=objects[1].array;if(mode==0)a.Num=-1;if(mode==1)a.Max=0;if(mode==2)a.Data=nullptr;if(mode==3)a.Max=-1;std::vector<C::RoachSlot> out;assert(!C::ReadRoaches(&objects[1],out));}});
    Case("R06 bounded null scans",[]{Reset();objects[1].array={slots,65537,65537};std::vector<C::RoachSlot> out;assert(!C::ReadRoaches(&objects[1],out));Population(129);assert(!C::ReadRoaches(&objects[1],out));Population(3);slots[0]=nullptr;assert(C::ReadRoaches(&objects[1],out)&&out[0].rawIdx==1);});
    Case("R07 component serial",[]{Reset();std::vector<C::RoachSlot> rows;C::ReadRoaches(&objects[1],rows);C::Receipt receipt(&objects[1]);++serials[20];assert(!C::Bound(receipt,rows[0]));});
    Case("R08 location callback",[]{Reset();C::Receipt receipt(&objects[1]);callback=[](void*,int fn){if(fn==8)++serials[1];};std::vector<C::SnapEntry> rows;assert(!C::ReadSnapshot(receipt,rows)&&calls[9]==0);});
    Case("R09 scale callback",[]{Reset();C::Receipt receipt(&objects[1]);callback=[](void*,int fn){if(fn==9)++world;};std::vector<C::SnapEntry> rows;assert(!C::ReadSnapshot(receipt,rows));});
    Case("R10 incomplete sample",[]{Reset(false,2);getOk=false;C::QueueConnectBroadcastForSlot(1);assert(!C::g_delivery[1].sequence);});
    Case("R11 native numeric bounds",[]{Reset();for(float x:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),1.e9f}){objects[20].location.x=x;C::Receipt r(&objects[1]);std::vector<C::SnapEntry> rows;assert(!C::ReadSnapshot(r,rows));}objects[20].location={};objects[20].scale.x=0;C::Receipt r(&objects[1]);std::vector<C::SnapEntry> rows;assert(!C::ReadSnapshot(r,rows));});
    Case("R12 refused delivery debt",[]{Reset();session.fail=2;C::QueueConnectBroadcastForSlot(1);C::Flush();assert(C::g_delivery[1].sequence&&C::g_delivery[1].page==0);session.fail=0;C::Flush();assert(!C::g_delivery[1].sequence);});
    Case("R13 nonzero wrap",[]{Reset();C::g_sequence=0xffffffffu;C::QueueConnectBroadcastForSlot(1);assert(C::g_delivery[1].sequence==1);});
    Case("R14 recipient isolation",[]{Reset(false,13);session.fail=2;C::Receipt r(&objects[1]);std::vector<C::SnapEntry> rows;assert(C::ReadSnapshot(r,rows));C::QueueSnapshot(r,rows);C::Flush();C::Flush();assert(C::g_delivery[1].page==0&&!C::g_delivery[2].sequence&&!C::g_delivery[31].sequence);});
    Case("R15 peer generation",[]{Reset();C::QueueConnectBroadcastForSlot(1);++session.generations[1];C::Flush();assert(session.sends.empty()&&!C::g_delivery[1].sequence);});
    Case("R16 empty join",[]{Reset(false,0);C::QueueConnectBroadcastForSlot(31);C::Flush();assert(session.sends.size()==1);N::RoachStatePayload p{};std::memcpy(&p,session.sends[0].bytes.data(),sizeof(p));assert(p.pageCount==1&&p.totalCount==0&&p.entryCount==0);});
    Case("R17 empty keepalive",[]{Reset(false,0);C::Tick();C::Flush();session.sends.clear();N::testNow+=10000;C::Tick();C::Flush();assert(!session.sends.empty());});
    Case("R18 role/liveness receipt",[]{Reset();Receive();assert(!C::g_pending.active);session.mode=N::Role::Client;session.active=false;Receive();assert(!C::g_pending.active);session.active=true;gameThread=false;Receive();assert(!C::g_pending.active);});
    Case("R19 page shape",[]{for(int mode=0;mode<5;++mode){Reset(true);auto p=Page();if(mode==0)p.pageCount=2;if(mode==1)p.entryCount=0;if(mode==2)p.page=1;if(mode==3)p.totalCount=129;if(mode==4)p.entryCount=13;C::OnState(p);assert(!C::g_pending.active&&!C::g_pages);}});
    Case("R20 coherent headers",[]{Reset(true);C::OnState(Page(1,13,0));C::OnState(Page(1,14,1));assert(!C::g_pending.active&&C::g_total==13);C::OnState(Page(1,13,1));assert(C::g_pending.rows.size()==13);});
    Case("R21 stale/wrapped ordering",[]{Reset(true);Receive(0xfffffffeu);Receive(0xfffffffdu);assert(C::g_asmSeq==0xfffffffeu);Receive(1);assert(C::g_asmSeq==1);Receive(0);assert(C::g_asmSeq==1);});
    Case("R22 duplicates",[]{Reset(true);Receive();const auto revision=C::g_pending.revision;Receive();assert(C::g_pending.revision==revision);});
    Case("R23 wire numeric bounds",[]{for(int mode=0;mode<5;++mode){Reset(true);auto p=Page();if(mode==0)p.entries[0].x=std::numeric_limits<float>::quiet_NaN();if(mode==1)p.entries[0].scale=0;if(mode==2)p.entries[0].scale=-1;if(mode==3)p.entries[0].scale=10001;if(mode==4)p.entries[0].z=1.e9f;C::OnState(p);assert(!C::g_pending.active);}});
    Case("R24 unresolved master debt",[]{Reset(true);objects[0].master=nullptr;Receive();C::Tick();assert(C::g_pending.active);objects[0].master=&objects[1];C::Tick();assert(C::g_haveApplied);});
    Case("R25 master incarnation",[]{Reset(true);Receive();++serials[1];C::Tick();assert(!C::g_pending.active&&calls[10]==0);});
    Case("R26 receive during native apply",[]{Reset(true);Receive();callback=[](void*,int fn){if(fn==8)Receive(2,2);};C::Tick();assert(C::g_pending.active&&C::g_pending.rows.size()==2&&Mutations()==0);});
    Case("R27 nested tick",[]{Reset(true);Receive();callback=[](void*,int){C::Tick();};C::Tick();assert(C::g_haveApplied&&calls[10]==1&&calls[11]==1);});
    Case("R28 location schema",[]{for(auto name:{L"NewLocation",L"bSweep",L"bTeleport"}){Reset(true);Receive();failParam=name;C::Tick();assert(calls[10]==0&&C::g_pending.active);failParam=nullptr;C::Tick();assert(C::g_haveApplied);}});
    Case("R29 scale-write debt",[]{Reset(true);Receive();failParam=L"NewScale";C::Tick();assert(calls[11]==0&&C::g_pending.active);failParam=nullptr;C::Tick();assert(C::g_haveApplied&&calls[10]==1);});
    Case("R30 deletion schema",[]{for(auto name:{L"IndexToRemove",L"crush"}){Reset(true,2);Receive();failParam=name;C::Tick();assert(calls[7]==0&&C::g_pending.active);failParam=nullptr;C::Tick();assert(C::g_haveApplied&&calls[7]==1);}});
    Case("R31 creation schema",[]{for(auto name:{L"Location",L"Size",L"bypassCheck"}){Reset(true,0);Receive();failParam=name;C::Tick();assert(calls[6]==0&&C::g_pending.active);failParam=nullptr;C::Tick();assert(C::g_haveApplied&&calls[6]==1);}});
    Case("R32 uncertain mutation",[]{for(int count:{0,2}){Reset(true,count);Receive();mutate=false;C::Tick();C::Tick();assert(Mutations()==1&&C::g_pending.uncertain);Receive(2);mutate=true;C::Tick();assert(C::g_haveApplied&&Mutations()==2);}});
    Case("R33 raw-index binding",[]{Reset();std::vector<C::RoachSlot> rows;C::ReadRoaches(&objects[1],rows);C::Receipt r(&objects[1]);slots[0]=&objects[21];assert(!C::Bound(r,rows[0]));});
    Case("R34 reversible retry",[]{Reset(true);Receive();callOk=false;C::Tick();assert(C::g_pending.active&&!C::g_pending.uncertain);callOk=true;C::Tick();assert(C::g_haveApplied);});
    Case("R35 reconstruction exclusion",[]{Reset(true,5);Receive(1,0);C::Tick();assert(C::g_pending.active&&calls[7]==4&&session.sends.empty());C::Tick();assert(C::g_haveApplied&&session.sends.empty());});
    Case("R36 serial consumption",[]{Reset(true);Receive();C::Tick();++serials[20];C::DetectConsumption(&session);assert(session.sends.size()==1&&session.sends[0].kind==N::ReliableKind::RoachConsumed);});
    Case("R37 replaced master",[]{Reset(true);Receive();C::Tick();++serials[1];alive[20]=false;C::DetectConsumption(&session);assert(session.sends.empty()&&!C::g_haveApplied);});
    Case("R38 send callback ownership",[]{Reset(true);Receive();C::Tick();alive[20]=false;session.sendHook=[] {C::OnDisconnect();};C::DetectConsumption(&session);assert(C::g_tracked.empty());});
    Case("R39 consumed admission",[]{for(int slot:{-1,0,32,255}){Reset();C::OnConsumedIntent({0,0,0},slot);assert(calls[7]==0);}Reset();session.ready=0;C::OnConsumedIntent({0,0,0},1);assert(calls[7]==0);session.ready=2;C::OnConsumedIntent({std::numeric_limits<float>::infinity(),0,0},1);assert(calls[7]==0);});
    Case("R40 nearest callback receipt",[]{for(int mode=0;mode<5;++mode){Reset();callback=[mode](void*,int fn){if(fn!=8)return;if(mode==0)++session.generations[1];if(mode==1)objects[0].master=&objects[2];if(mode==2)slots[0]=&objects[21];if(mode==3)++world;if(mode==4)C::OnDisconnect();};C::OnConsumedIntent({0,0,0},1);assert(calls[7]==0);}Reset();C::OnConsumedIntent({0,0,0},31);assert(calls[7]==1);});
    puts("PASS 40 roach adapter boundaries with production controller and controlled native/transport callbacks");
}
