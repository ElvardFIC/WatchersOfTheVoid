#include "coop/creatures/roach_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/net_clock.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/world/world_singleton.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace coop::roach_sync {
namespace {
namespace R=ue_wrap::reflection;
namespace N=coop::net;
using Ref=ue_wrap::CachedObjRef;
struct Vec3 {float x,y,z;};
struct SnapEntry {Vec3 loc;float scale;};
struct FArrayRaw {void* Data;int32_t Num,Max;};
struct RoachSlot {Ref comp;int32_t rawIdx;};
constexpr int kMaxRawSlots=65536;
constexpr uint64_t kPollMs=1000,kKeepaliveMs=10000;
std::atomic<N::Session*> g_session{nullptr};
uint64_t g_epoch=0,g_nextResolve=0,g_lastPoll=0,g_lastSend=0,g_revision=0;
uint32_t g_world=0,g_sequence=0;
bool g_ticking=false,g_applying=false;
Ref g_gmClass,g_masterClass,g_add,g_del,g_getLoc,g_getScale,g_setLoc,g_setScale;
int32_t g_offMaster=-1,g_offRoaches=-1;
Ref g_polledMaster;
std::vector<SnapEntry> g_lastSample;

bool Finite(const Vec3& v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
        std::fabs(v.x)<=1.e8f&&std::fabs(v.y)<=1.e8f&&std::fabs(v.z)<=1.e8f;
}
bool Valid(const SnapEntry& e) {return Finite(e.loc)&&std::isfinite(e.scale)&&e.scale>0&&e.scale<=10000.f;}
float Dist2(const Vec3& a,const Vec3& b) {
    const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;
}
bool Current(N::Session* s) {
    return s&&g_session.load(std::memory_order_acquire)==s&&s->running()&&s->connected()&&
        g_world==ue_wrap::world_identity::Generation();
}
bool Resolved() {
    return g_gmClass.Get()&&g_masterClass.Get()&&g_offMaster>=0&&g_offRoaches>=0&&
        g_add.Get()&&g_del.Get()&&g_getLoc.Get()&&g_getScale.Get()&&g_setLoc.Get()&&g_setScale.Get();
}
void ResolvePass() {
    if(Resolved())return;
    const auto now=N::NowMs();if(now<g_nextResolve)return;g_nextResolve=now+2000;
    g_gmClass.Set(R::FindClass(L"mainGamemode_C"));
    g_masterClass.Set(R::FindClass(L"cockroachMaster_C"));
    auto* scene=R::FindClass(L"SceneComponent");
    g_offMaster=g_gmClass.Get()?R::FindPropertyOffset(g_gmClass.Get(),L"cockroachMaster"):-1;
    g_offRoaches=g_masterClass.Get()?R::FindPropertyOffset(g_masterClass.Get(),L"roaches"):-1;
    auto bind=[](Ref& ref,void* cls,const wchar_t* name){ref.Set(cls?R::FindFunction(cls,name):nullptr);};
    bind(g_add,g_masterClass.Get(),L"addRoach");bind(g_del,g_masterClass.Get(),L"deleteRoach");
    bind(g_getLoc,scene,L"K2_GetComponentLocation");bind(g_getScale,scene,L"K2_GetComponentScale");
    bind(g_setLoc,scene,L"K2_SetWorldLocation");bind(g_setScale,scene,L"SetWorldScale3D");
}
void* Master() {
    void* gm=ue_wrap::world_singleton::Gamemode();
    if(!gm||!R::IsLive(gm)||!g_gmClass.Get()||R::ClassOf(gm)!=g_gmClass.Get()||g_offMaster<0)return nullptr;
    void* master=nullptr;std::memcpy(&master,static_cast<uint8_t*>(gm)+g_offMaster,sizeof(master));
    return master&&R::IsLive(master)&&g_masterClass.Get()&&R::ClassOf(master)==g_masterClass.Get()?master:nullptr;
}
struct Receipt {
    N::Session* session;uint64_t epoch;N::Role role;Ref master,gm;
    explicit Receipt(void* m):session(g_session.load()),epoch(g_epoch),role(session->role()) {
        master.Set(m);gm.Set(ue_wrap::world_singleton::Gamemode());
    }
    bool Current()const {
        return epoch==g_epoch&&roach_sync::Current(session)&&session->role()==role&&
            master.Get()&&gm.Get()&&ue_wrap::world_singleton::Gamemode()==gm.Raw()&&Master()==master.Raw();
    }
};
bool Array(void* master,FArrayRaw& arr) {
    if(!master||!R::IsLive(master)||g_offRoaches<0)return false;
    std::memcpy(&arr,static_cast<uint8_t*>(master)+g_offRoaches,sizeof(arr));
    return arr.Num>=0&&arr.Max>=arr.Num&&arr.Max<=kMaxRawSlots&&(!arr.Num||arr.Data);
}
bool ReadRoaches(void* master,std::vector<RoachSlot>& out) {
    out.clear();FArrayRaw arr{};if(!Array(master,arr))return false;
    auto** data=static_cast<void**>(arr.Data);
    for(int32_t i=0;i<arr.Num;++i)if(data[i]&&R::IsLive(data[i])) {
        if(out.size()>=N::kRoachSnapshotCap)return false;
        Ref ref;ref.Set(data[i]);if(!ref.Get())return false;out.push_back({ref,i});
    }
    return true;
}
bool Bound(const Receipt& receipt,const RoachSlot& slot) {
    if(!receipt.Current()||!slot.comp.Get())return false;
    FArrayRaw arr{};
    return Array(receipt.master.Get(),arr)&&slot.rawIdx>=0&&slot.rawIdx<arr.Num&&
        static_cast<void**>(arr.Data)[slot.rawIdx]==slot.comp.Raw();
}
bool ReadVector(const Receipt& receipt,const RoachSlot& slot,const Ref& function,Vec3& out) {
    if(!Bound(receipt,slot)||!function.Get())return false;
    const Ref fn=function;ue_wrap::ParamFrame frame(fn.Get());
    if(!frame.valid()||!ue_wrap::Call(slot.comp.Get(),frame)||!Bound(receipt,slot)||!fn.Get())return false;
    return frame.GetRaw(L"ReturnValue",&out,sizeof(out))&&Finite(out);
}
bool ReadSnapshot(const Receipt& receipt,std::vector<SnapEntry>& out) {
    std::vector<RoachSlot> slots;out.clear();if(!ReadRoaches(receipt.master.Get(),slots))return false;
    for(const auto& slot:slots) {
        SnapEntry entry{};Vec3 scale{};
        if(!ReadVector(receipt,slot,g_getLoc,entry.loc)||!ReadVector(receipt,slot,g_getScale,scale))return false;
        entry.scale=scale.x;if(!Valid(entry))return false;out.push_back(entry);
    }
    std::vector<RoachSlot> after;if(!receipt.Current()||!ReadRoaches(receipt.master.Get(),after)||after.size()!=slots.size())return false;
    for(size_t i=0;i<slots.size();++i)if(after[i].rawIdx!=slots[i].rawIdx||after[i].comp.Get()!=slots[i].comp.Get())return false;
    return true;
}
bool Changed(const std::vector<SnapEntry>& snap) {
    if(snap.size()!=g_lastSample.size())return true;
    for(size_t i=0;i<snap.size();++i)if(Dist2(snap[i].loc,g_lastSample[i].loc)>625.f||
        std::fabs(snap[i].scale-g_lastSample[i].scale)>.05f)return true;
    return false;
}
struct Delivery {
    std::vector<SnapEntry> rows;Ref master;uint32_t sequence=0,generation=0;uint8_t page=0;
    uint64_t revision=0,expires=0;
};
std::array<Delivery,N::kMaxPeers> g_delivery{};
int g_deliveryNext=1;
void QueueSnapshot(const Receipt& receipt,const std::vector<SnapEntry>& rows,int only=-1) {
    if(!receipt.Current())return;
    if(++g_sequence==0)++g_sequence;
    for(int slot=1;slot<N::kMaxPeers;++slot) {
        if(only>=0&&slot!=only)continue;
        const auto generation=receipt.session->peerGenerationForSlot(slot);
        if(!generation||!receipt.session->IsSlotWorldReady(slot))continue;
        g_delivery[slot]={rows,receipt.master,g_sequence,generation,0,++g_revision,N::NowMs()+15000};
    }
}
void Flush() {
    auto* session=g_session.load();const auto epoch=g_epoch;
    if(!Current(session)||session->role()!=N::Role::Host)return;
    int budget=4;
    for(int scanned=0;scanned<N::kMaxPeers-1&&budget>0;++scanned) {
        const int slot=g_deliveryNext;g_deliveryNext=g_deliveryNext%(N::kMaxPeers-1)+1;
        auto row=g_delivery[slot];if(!row.sequence)continue;
        if(!row.master.Get()||Master()!=row.master.Raw()||N::NowMs()>row.expires||
            row.generation!=session->peerGenerationForSlot(slot)||!session->IsSlotWorldReady(slot)) {
            g_delivery[slot]={};continue;
        }
        N::RoachStatePayload p{};p.seq=row.sequence;p.page=row.page;p.totalCount=static_cast<uint8_t>(row.rows.size());
        p.pageCount=static_cast<uint8_t>(std::max(1,(int(row.rows.size())+N::kRoachEntriesPerPage-1)/N::kRoachEntriesPerPage));
        const size_t base=size_t(row.page)*N::kRoachEntriesPerPage;
        for(size_t i=base;i<row.rows.size()&&i<base+N::kRoachEntriesPerPage;++i) {
            const auto& e=row.rows[i];p.entries[p.entryCount++]={e.loc.x,e.loc.y,e.loc.z,e.scale};
        }
        --budget;const bool sent=session->SendReliableToSlot(slot,N::ReliableKind::RoachState,&p,sizeof(p));
        if(epoch!=g_epoch||!Current(session)||session->role()!=N::Role::Host)return;
        auto& current=g_delivery[slot];
        if(current.revision!=row.revision||session->peerGenerationForSlot(slot)!=row.generation)continue;
        if(sent&&++current.page>=p.pageCount)current={};
    }
}

uint32_t g_asmSeq=0;uint16_t g_pages=0;uint8_t g_pageCount=0,g_total=0;
std::array<SnapEntry,N::kRoachSnapshotCap> g_assembly{};
struct Pending {std::vector<SnapEntry> rows;Ref master;uint64_t revision=0;bool active=false,uncertain=false;};
Pending g_pending;
struct Tracked {Ref comp;Vec3 loc;};
std::vector<Tracked> g_tracked;Ref g_trackedMaster;bool g_haveApplied=false;uint64_t g_tracking=0;

bool SetLocation(const Receipt& receipt,const RoachSlot& slot,const Vec3& location) {
    if(!Bound(receipt,slot)||!g_setLoc.Get())return false;
    const Ref fn=g_setLoc;ue_wrap::ParamFrame f(fn.Get());
    return f.valid()&&f.SetRaw(L"NewLocation",&location,sizeof(location))&&f.Set<bool>(L"bSweep",false)&&
        f.Set<bool>(L"bTeleport",true)&&ue_wrap::Call(slot.comp.Get(),f)&&Bound(receipt,slot)&&fn.Get();
}
bool SetScale(const Receipt& receipt,const RoachSlot& slot,float scale) {
    if(!Bound(receipt,slot)||!g_setScale.Get())return false;
    const Vec3 value{scale,scale,scale};const Ref fn=g_setScale;ue_wrap::ParamFrame f(fn.Get());
    return f.valid()&&f.SetRaw(L"NewScale",&value,sizeof(value))&&ue_wrap::Call(slot.comp.Get(),f)&&Bound(receipt,slot)&&fn.Get();
}
bool DeleteFrame(ue_wrap::ParamFrame& f,int32_t index) {
    return f.valid()&&f.Set<int32_t>(L"IndexToRemove",index)&&f.Set<bool>(L"crush",false);
}
bool AddFrame(ue_wrap::ParamFrame& f,const SnapEntry& entry) {
    const Vec3 size{entry.scale,entry.scale,entry.scale};
    return f.valid()&&f.SetRaw(L"Location",&entry.loc,sizeof(entry.loc))&&f.SetRaw(L"Size",&size,sizeof(size))&&
        f.Set<bool>(L"bypassCheck",true);
}
void ApplyPending() {
    if(g_applying||!g_pending.active||g_pending.uncertain)return;
    void* master=Master();if(!master)return;
    auto pending=g_pending;
    if(pending.master.Raw()&&pending.master.Get()!=master) {g_pending={};return;}
    if(!pending.master.Raw()){g_pending.master.Set(master);pending.master=g_pending.master;}
    const Receipt receipt(master);
    auto current=[&]{return receipt.Current()&&g_pending.active&&g_pending.revision==pending.revision;};
    struct Guard {Guard(){g_applying=true;}~Guard(){g_applying=false;}} guard;
    std::vector<RoachSlot> slots;if(!ReadRoaches(master,slots))return;
    int budget=4;
    while(slots.size()!=pending.rows.size()&&budget-->0) {
        const bool deleting=slots.size()>pending.rows.size();
        const Ref fn=deleting?g_del:g_add;ue_wrap::ParamFrame frame(fn.Get());
        if(deleting) {
            if(!Bound(receipt,slots.back())||!DeleteFrame(frame,slots.back().rawIdx))return;
        } else if(!AddFrame(frame,pending.rows[slots.size()]))return;
        // Once native mutation is admitted, an uncertain result may not be retried.
        // Reconcile an observed population change; otherwise await a fresh host row.
        g_pending.uncertain=true;
        ue_wrap::Call(master,frame);
        if(!current()||!fn.Get())return;
        std::vector<RoachSlot> after;if(!ReadRoaches(master,after))return;
        if(after.size()!=(deleting?slots.size()-1:slots.size()+1))return;
        if(deleting&&slots.back().comp.Get()) {
            for(const auto& row:after)if(row.comp.Get()==slots.back().comp.Get())return;
        }
        g_pending.uncertain=false;slots=std::move(after);
    }
    if(slots.size()!=pending.rows.size())return;
    std::vector<Tracked> tracked;tracked.reserve(slots.size());
    for(size_t i=0;i<slots.size();++i) {
        Vec3 location{},scale{};const auto& want=pending.rows[i];
        if(!ReadVector(receipt,slots[i],g_getLoc,location)||!current())return;
        if(Dist2(location,want.loc)>1.f&&(!SetLocation(receipt,slots[i],want.loc)||!current()))return;
        if(!ReadVector(receipt,slots[i],g_getScale,scale)||!current())return;
        if(std::fabs(scale.x-want.scale)>.05f&&(!SetScale(receipt,slots[i],want.scale)||!current()))return;
        tracked.push_back({slots[i].comp,want.loc});
    }
    if(!current())return;
    g_tracked=std::move(tracked);g_trackedMaster=receipt.master;g_haveApplied=true;++g_tracking;g_pending={};
}
void DetectConsumption(N::Session* session) {
    if(!g_haveApplied||g_pending.active||g_tracked.empty())return;
    void* master=Master();
    if(!g_trackedMaster.Get()||g_trackedMaster.Get()!=master){g_tracked.clear();g_haveApplied=false;return;}
    const Receipt receipt(master);const auto tracking=g_tracking;
    auto rows=g_tracked;std::vector<Tracked> retained;int budget=4;
    for(const auto& row:rows) {
        if(row.comp.Get()){retained.push_back(row);continue;}
        if(budget<=0){retained.push_back(row);continue;}
        --budget;N::RoachConsumedPayload p{row.loc.x,row.loc.y,row.loc.z};
        const bool sent=session->SendReliable(N::ReliableKind::RoachConsumed,&p,sizeof(p));
        if(!receipt.Current()||tracking!=g_tracking||g_pending.active)return;
        if(!sent)retained.push_back(row);
    }
    g_tracked=std::move(retained);
}
bool Ready() {
    auto* session=g_session.load();
    if(!session)return false;
    if(g_world!=ue_wrap::world_identity::Generation())Install(session);
    return Current(session);
}
}

void Install(N::Session* session) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session.load()!=session||g_world!=world){OnDisconnect();g_world=world;g_session.store(session);}
}
void Tick() {
    if(!ue_wrap::game_thread::IsGameThread()||g_ticking||!Ready())return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    auto* session=g_session.load();ResolvePass();if(!Resolved())return;
    if(session->role()==N::Role::Client) {
        ApplyPending();if(!Current(session))return;
        const auto now=N::NowMs();if(now-g_lastPoll<kPollMs)return;g_lastPoll=now;DetectConsumption(session);return;
    }
    if(session->role()!=N::Role::Host)return;
    Flush();if(!Current(session))return;
    const auto now=N::NowMs();if(now-g_lastPoll<kPollMs)return;g_lastPoll=now;
    void* master=Master();if(!master)return;const Receipt receipt(master);
    std::vector<SnapEntry> snap;if(!ReadSnapshot(receipt,snap))return;
    if(g_polledMaster.Get()!=master||Changed(snap)||now-g_lastSend>=kKeepaliveMs) {
        QueueSnapshot(receipt,snap);g_polledMaster=receipt.master;g_lastSample=std::move(snap);g_lastSend=now;
    }
}
void OnState(const N::RoachStatePayload& p) {
    if(!ue_wrap::game_thread::IsGameThread()||!Ready()||g_session.load()->role()!=N::Role::Client||!p.seq)return;
    const int pages=std::max(1,(int(p.totalCount)+N::kRoachEntriesPerPage-1)/N::kRoachEntriesPerPage);
    const int base=int(p.page)*N::kRoachEntriesPerPage;
    if(p.totalCount>N::kRoachSnapshotCap||p.pageCount!=pages||p.page>=pages||
        p.entryCount!=std::max(0,std::min(N::kRoachEntriesPerPage,int(p.totalCount)-base)))return;
    for(int i=0;i<p.entryCount;++i)if(!Valid({{p.entries[i].x,p.entries[i].y,p.entries[i].z},p.entries[i].scale}))return;
    if(p.seq!=g_asmSeq) {
        if(g_asmSeq&&static_cast<int32_t>(p.seq-g_asmSeq)<=0)return;
        g_asmSeq=p.seq;g_pages=0;g_pageCount=p.pageCount;g_total=p.totalCount;
    } else if(p.pageCount!=g_pageCount||p.totalCount!=g_total)return;
    const uint16_t bit=static_cast<uint16_t>(1u<<p.page);if(g_pages&bit)return;
    for(int i=0;i<p.entryCount;++i)g_assembly[base+i]={{p.entries[i].x,p.entries[i].y,p.entries[i].z},p.entries[i].scale};
    g_pages|=bit;if(g_pages!=static_cast<uint16_t>((1u<<g_pageCount)-1))return;
    g_pending={};g_pending.rows.assign(g_assembly.begin(),g_assembly.begin()+g_total);
    g_pending.revision=++g_revision;g_pending.active=true;g_pending.master.Set(Master());
    g_haveApplied=false;++g_tracking;
}
void OnConsumedIntent(const N::RoachConsumedPayload& p,int slot) {
    if(!ue_wrap::game_thread::IsGameThread()||!Ready())return;
    auto* session=g_session.load();const Vec3 at{p.x,p.y,p.z};
    if(session->role()!=N::Role::Host||slot<=0||slot>=N::kMaxPeers||!Finite(at)||!session->IsSlotWorldReady(slot))return;
    const auto generation=session->peerGenerationForSlot(slot);if(!generation)return;
    ResolvePass();if(!Resolved())return;void* master=Master();if(!master)return;const Receipt receipt(master);
    auto current=[&]{return receipt.Current()&&session->IsSlotWorldReady(slot)&&session->peerGenerationForSlot(slot)==generation;};
    std::vector<RoachSlot> rows;if(!ReadRoaches(master,rows))return;
    RoachSlot best{};float bestDistance=200.f*200.f;
    for(const auto& row:rows) {
        Vec3 location{};const bool read=ReadVector(receipt,row,g_getLoc,location);if(!current())return;
        if(!read)continue;const auto distance=Dist2(location,at);
        if(distance<=bestDistance){best=row;bestDistance=distance;}
    }
    if(!best.comp.Get()||!Bound(receipt,best)||!current())return;
    ue_wrap::ParamFrame frame(g_del.Get());if(!DeleteFrame(frame,best.rawIdx)||!Bound(receipt,best)||!current())return;
    ue_wrap::Call(master,frame);
}
void QueueConnectBroadcastForSlot(int slot) {
    if(!ue_wrap::game_thread::IsGameThread()||!Ready())return;
    auto* session=g_session.load();if(session->role()!=N::Role::Host||slot<=0||slot>=N::kMaxPeers||!session->IsSlotWorldReady(slot))return;
    const auto generation=session->peerGenerationForSlot(slot);if(!generation)return;
    ResolvePass();if(!Resolved())return;void* master=Master();if(!master)return;const Receipt receipt(master);
    std::vector<SnapEntry> rows;if(!ReadSnapshot(receipt,rows)||!receipt.Current()||session->peerGenerationForSlot(slot)!=generation)return;
    QueueSnapshot(receipt,rows,slot);
}
void OnDisconnect() {
    ++g_epoch;g_session.store(nullptr);g_world=0;g_nextResolve=g_lastPoll=g_lastSend=0;
    g_sequence=g_asmSeq=0;g_pages=0;g_pageCount=g_total=0;
    g_pending={};g_delivery={};g_deliveryNext=1;g_lastSample.clear();g_polledMaster.Reset();
    g_tracked.clear();g_trackedMaster.Reset();g_haveApplied=false;++g_tracking;
    g_gmClass.Reset();g_masterClass.Reset();g_add.Reset();g_del.Reset();g_getLoc.Reset();g_getScale.Reset();g_setLoc.Reset();g_setScale.Reset();
    g_offMaster=g_offRoaches=-1;
}
}
