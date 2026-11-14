// Tower rows: owned snapshots, generation-bound delivery and guarded application.
#include "coop/world/fuse_tower_rows.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/world/fuse_tower_kind.h"
#include "coop/world/fuse_tower_ops.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/world_identity.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>

namespace coop::fuse_tower_rows {
namespace {
namespace FK=coop::fuse_tower_kind;
using net::FuseTowerPayload;
using net::FuseTowerRow;
using net::kFuseTowers;
constexpr uint64_t kLookupMs=1000,kPanelRetryMs=2000;
constexpr int kDeliveryBudget=8;
std::atomic<net::Session*> g_session{nullptr};
struct Scope {net::Session* session=nullptr;uint32_t serial=0,world=0;uint64_t stopped=0,epoch=0;net::Role role=net::Role::Host;};
Scope g_scope,g_applyScope;
uint64_t g_epoch=0,g_revision=0,g_receiveRevision=0,g_dirtyRevision=0;
bool g_ticking=false,g_sending=false;
unsigned g_applyDepth=0;
uint64_t NowMs(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
uint64_t Elapsed(uint64_t now,uint64_t then){return now>=then?now-then:0;}
Scope Capture(){auto* s=g_session.load();return {s,net::session_serial::Current(),ue_wrap::world_identity::Generation(),s?s->StopCount():0,g_epoch,s?s->role():net::Role::Host};}
bool Same(const Scope& s){auto* installed=g_session.load();return s.session==installed && s.epoch==g_epoch &&
    s.serial==net::session_serial::Current() && s.world==ue_wrap::world_identity::Generation() &&
    (!installed || (s.stopped==installed->StopCount() && s.role==installed->role()));}
bool Current(const Scope& s){return Same(s) && s.session && s.session->running() && s.session->connected();}
bool Host(const Scope& s){return Current(s)&&s.role==net::Role::Host;}
bool Client(const Scope& s){return Current(s)&&s.role==net::Role::Client;}
struct CachedTower {FK::Tower tower;ue_wrap::CachedObjRef actor;};
std::array<CachedTower,kFuseTowers> g_towers{};
int32_t g_towerCount=0;
uint64_t g_classVersion=~0ull,g_lookMs=0,g_waitLookMs=0;
FuseTowerPayload g_lastSent{},g_rows{};
bool g_haveSent=false,g_acksDirty=false,g_haveRows=false,g_rowsWaiting=false;
uint64_t g_broadcasts=0,g_applied=0;
struct Delivery {bool owed=false;uint32_t generation=0;uint64_t revision=0;};
roster_ledger::PerSlotState<Delivery> g_join,g_answer;
unsigned g_joinCursor=0,g_answerCursor=0;
std::array<bool,net::kFuseTowerKinds> g_sizeWarned{};
std::array<Edges,net::kFuseTowerKinds> g_edges{};
bool g_badRowsSaid=false;
struct Kept {
    bool used=false,pending=false,applied=false,firstApplied=false;
    uint8_t appliedFlags=0,firstFlags=0;
    ue_wrap::CachedObjRef tower;
    FuseTowerRow row{};
    uint64_t revision=0,attempts=0,wanted=0,dirty=0,panelAskedMs=0;
};
std::array<Kept,kFuseTowers> g_kept{};
void Clear(){
    ++g_epoch;++g_dirtyRevision;++g_receiveRevision;
    g_towers={};g_towerCount=0;g_classVersion=~0ull;g_lookMs=g_waitLookMs=0;
    g_lastSent={};g_rows={};g_haveSent=g_acksDirty=g_haveRows=g_rowsWaiting=false;
    g_broadcasts=g_applied=0;g_edges={};g_sizeWarned={};g_kept={};g_badRowsSaid=false;
    g_joinCursor=g_answerCursor=0;
    for(int i=0;i<net::kMaxPeers;++i){g_join[i]={};g_answer[i]={};}
    // Admission belongs to the active C++ stack and is never reset by a callback.
}
void Reconcile(){if(!Same(g_scope)){Clear();g_scope=Capture();}}
bool HaveRadio(){for(int i=0;i<g_towerCount;++i)if(g_towers[i].tower.kind==FK::Kind::Radio)return true;return false;}
bool Towers(bool force=false){
    const auto scope=g_scope;if(!Current(scope))return false;
    const auto version=ue_wrap::object_index::ClassSetVersion();
    bool stale=force||version!=g_classVersion;
    for(int i=0;!stale&&i<g_towerCount;++i)stale=!g_towers[i].actor.Get();
    if(!stale&&(!g_towerCount||!HaveRadio()))stale=Elapsed(NowMs(),g_lookMs)>=kLookupMs;
    if(stale){
        std::array<FK::Tower,kFuseTowers> found{};
        const int n=FK::ReadAll(found.data(),kFuseTowers);
        if(!Current(scope))return false;
        std::array<CachedTower,kFuseTowers> next{};int kept=0;
        for(int i=0;i<std::clamp(n,0,kFuseTowers);++i){
            if(!found[i].obj||static_cast<unsigned>(found[i].kind)>=net::kFuseTowerKinds)continue;
            next[kept].tower=found[i];next[kept].actor.Set(found[i].obj);
            if(next[kept].actor.Get())++kept;
        }
        if(!Current(scope))return false;
        g_towers=next;g_towerCount=kept;g_classVersion=version;g_lookMs=NowMs();
        for(auto& k:g_kept)if(k.used){
            void* bound=nullptr;
            for(int i=0;i<kept;++i)if(next[i].tower.kind==static_cast<FK::Kind>(k.row.kind)&&next[i].tower.id==k.row.id)bound=next[i].actor.Get();
            if(!k.tower.Is(bound)){k.pending=true;g_rowsWaiting=true;}
        }
    }
    return g_towerCount>0;
}
void* TowerById(FK::Kind kind,int32_t id){for(int i=0;i<g_towerCount;++i)if(g_towers[i].tower.kind==kind&&g_towers[i].tower.id==id)return g_towers[i].actor.Get();return nullptr;}
bool SameRow(const FuseTowerRow& a,const FuseTowerRow& b){return !std::memcmp(&a,&b,sizeof(a));}
bool SameRows(const FuseTowerPayload& a,const FuseTowerPayload& b){return a.count<=kFuseTowers&&a.count==b.count&&!std::memcmp(a.rows,b.rows,sizeof(FuseTowerRow)*a.count);}
bool RowOk(const FuseTowerRow& r){
    if(r.kind>=net::kFuseTowerKinds||r.fuseCount>net::kFuseTowerMaxFuses||r.lightCount>net::kFuseTowerMaxLights||r.id<0)return false;
    const unsigned allowed=r.kind==net::kFuseTowerKindCoordinate?
        net::kFuseTowerBroken|net::kFuseTowerOpened|net::kFuseTowerAnim|net::kFuseTowerLeverMoving|net::kFuseTowerLeverUp:
        net::kFuseTowerBroken|net::kFuseTowerLeverMoving|net::kFuseTowerLeverUp|net::kFuseTowerResetMoving|net::kFuseTowerAnimating;
    if((r.flags&~allowed)||(r.kind==net::kFuseTowerKindRadio&&r.id!=0))return false;
    if(r.lights&~((1u<<r.lightCount)-1u))return false;
    for(int i=0;i<r.fuseCount;++i)if(r.fuses[i]>2)return false;
    return true;
}
bool ReadRows(FuseTowerPayload& out){
    const auto scope=g_scope;const auto revision=g_dirtyRevision;
    if(!Towers()||!Current(scope)||revision!=g_dirtyRevision)return false;
    const auto towers=g_towers;const int count=g_towerCount;
    FuseTowerPayload p{};p.op=net::kFuseTowerOpRows;
    for(int i=0;i<count;++i){
        if(!Current(scope)||revision!=g_dirtyRevision)return false;
        if(!towers[i].actor.Get())return false;
        FuseTowerRow row{};const bool read=FK::ReadRow(towers[i].tower,row);
        if(!Current(scope)||revision!=g_dirtyRevision||!towers[i].actor.Get())return false;
        if(!read)return false; // incomplete captures remain retry debt, not a smaller full snapshot
        if(!RowOk(row))return false;
        p.rows[p.count++]=row;
    }
    if(!p.count||!Current(scope)||revision!=g_dirtyRevision)return false;
    out=p;return true;
}
bool Peer(const Scope& scope,int slot,uint32_t generation,bool ready=true){return Host(scope)&&slot>0&&slot<net::kMaxPeers&&generation&&
    scope.session->peerGenerationForSlot(slot)==generation&&scope.session->IsSlotConnected(slot)&&(!ready||scope.session->IsSlotWorldReady(slot));}
bool SendTo(const Scope& scope,int slot,uint32_t generation,const FuseTowerPayload& rows){
    if(!Peer(scope,slot,generation))return false;
    FuseTowerPayload p=rows;fuse_tower_ops::FillAcks(p,slot);
    if(!Peer(scope,slot,generation))return false;
    const bool sent=scope.session->SendReliableToSlot(slot,net::ReliableKind::FuseTowerState,&p,sizeof(p));
    if(!Peer(scope,slot,generation)||!sent)return false;
    fuse_tower_ops::RefusedTold(static_cast<uint8_t>(slot),p.refused[slot]);
    return Peer(scope,slot,generation);
}
bool HostSendRows(){
    const auto scope=g_scope;
    if(!Host(scope)||g_sending)return false;
    struct Busy{Busy(){g_sending=true;}~Busy(){g_sending=false;}}busy;
    const auto revision=g_dirtyRevision;
    FuseTowerPayload p{};if(!ReadRows(p)||!Host(scope)||revision!=g_dirtyRevision)return false;
    fuse_tower_ops::FillAcks(p,-1);
    if(!Host(scope)||revision!=g_dirtyRevision)return false;
    if(!g_acksDirty&&g_haveSent&&SameRows(p,g_lastSent))return true;
    const bool sent=scope.session->SendReliable(net::ReliableKind::FuseTowerState,&p,sizeof(p));
    if(!Host(scope)||!sent)return false;
    if(revision!=g_dirtyRevision)return false;
    g_lastSent=p;g_haveSent=true;g_acksDirty=false;++g_broadcasts;return true;
}
void ServeDeliveries(bool answers){
    const auto scope=g_scope;if(!Host(scope)||g_sending)return;
    auto& debts=answers?g_answer:g_join;
    auto& cursor=answers?g_answerCursor:g_joinCursor;
    bool any=false;for(int i=1;i<net::kMaxPeers;++i)any=any||debts[i].owed;
    if(!any)return;
    struct Busy{Busy(){g_sending=true;}~Busy(){g_sending=false;}}busy;
    FuseTowerPayload p{};if(!ReadRows(p)||!Host(scope))return;
    int budget=kDeliveryBudget;
    for(int visited=0;visited<net::kMaxPeers-1&&budget&&Host(scope);++visited){
        const int slot=1+(cursor++%(net::kMaxPeers-1));const Delivery debt=debts[slot];
        if(!debt.owed)continue;
        if(!Peer(scope,slot,debt.generation,false)){if(debts[slot].revision==debt.revision)debts[slot]={};continue;}
        if(!Peer(scope,slot,debt.generation))continue;
        --budget;
        const bool sent=SendTo(scope,slot,debt.generation,p);
        if(!Host(scope))return;
        if(!sent||debts[slot].revision!=debt.revision||!Peer(scope,slot,debt.generation))continue;
        if(!answers||!fuse_tower_ops::RefusedPending(static_cast<uint8_t>(slot)))debts[slot]={};
    }
}
Kept* KeptFor(const FuseTowerRow& row){
    for(auto& k:g_kept)if(k.used&&k.row.kind==row.kind&&k.row.id==row.id)return &k;
    for(auto& k:g_kept)if(!k.used){k={};k.used=true;k.row=row;k.pending=true;k.revision=++g_revision;return &k;}
    return nullptr;
}
bool Pending(){for(const auto& k:g_kept)if(k.used&&k.pending)return true;return false;}
void ApplyOne(size_t index,void* tower){
    const auto scope=g_scope;auto& slot=g_kept[index];
    if(!slot.tower.Is(tower)){
        slot.tower.Set(tower);slot.applied=slot.firstApplied=false;slot.appliedFlags=slot.firstFlags=0;
        slot.attempts=slot.wanted=slot.dirty=0;slot.panelAskedMs=0;slot.revision=++g_revision;
    }
    const Kept note=slot;const auto kind=static_cast<FK::Kind>(note.row.kind);
    const auto current=[&]{const auto& k=g_kept[index];return Client(scope)&&note.tower.Get()&&k.used&&k.revision==note.revision&&
        k.tower.Is(tower)&&TowerById(kind,note.row.id)==tower;};
    FuseTowerRow row=note.row;if(!current())return;
    fuse_tower_ops::Overlay(row);if(!current())return;
    Edges edges=g_edges[note.row.kind];bool warned=g_sizeWarned[note.row.kind];
    FK::ApplyCtx ctx;ctx.live=note.applied;ctx.prevFlags=note.appliedFlags;ctx.edges=&edges;ctx.sizeWarned=&warned;ctx.current=current;
    ctx.firstAttempt=[&](uint64_t bit){if(!current())return false;auto& k=g_kept[index];if(k.attempts&bit)return false;k.attempts|=bit;return true;};
    ctx.effects=[&](uint64_t add)->uint64_t{if(!current())return 0;return g_kept[index].wanted|=add;};
    ctx.writes=[&](uint64_t add,uint64_t clear)->uint64_t{if(!current())return 0;return g_kept[index].dirty=(g_kept[index].dirty|add)&~clear;};
    const bool done=FK::ApplyRow(kind,tower,row,ctx);
    if(!current())return;
    g_edges[note.row.kind]=edges;g_sizeWarned[note.row.kind]=warned;
    if(!done)return;
    auto& k=g_kept[index];k.applied=true;k.pending=false;k.appliedFlags=row.flags;
    if(!k.firstApplied){k.firstApplied=true;k.firstFlags=row.flags;}
    ++g_applied;
}
void ApplyAll(){
    const auto scope=g_scope;if(!Client(scope)||g_applyDepth)return;
    struct ApplyingGuard{ApplyingGuard(){++g_applyDepth;g_applyScope=g_scope;}~ApplyingGuard(){--g_applyDepth;}}guard;
    if(!Towers()||!Client(scope))return;
    for(size_t i=0;i<g_kept.size()&&Client(scope);++i){
        const Kept note=g_kept[i];if(!note.used||!note.pending)continue;
        if(void* actor=TowerById(static_cast<FK::Kind>(note.row.kind),note.row.id))ApplyOne(i,actor);
    }
    if(Client(scope))g_rowsWaiting=Pending();
}
void ConvergePanels(){
    const auto scope=g_scope;if(!Client(scope)||g_applyDepth||!Towers()||!Client(scope))return;
    void* player=players::Registry::Get().Local();ue_wrap::CachedObjRef body;body.Set(player);if(!body.Get())return;
    struct ApplyingGuard{ApplyingGuard(){++g_applyDepth;g_applyScope=g_scope;}~ApplyingGuard(){--g_applyDepth;}}guard;
    const auto now=NowMs();
    for(size_t i=0;i<g_kept.size()&&Client(scope);++i){
        const Kept note=g_kept[i];if(!note.used||note.pending||note.row.kind!=net::kFuseTowerKindCoordinate||Elapsed(now,note.panelAskedMs)<kPanelRetryMs)continue;
        void* tower=note.tower.Get();
        const auto current=[&]{return Client(scope)&&body.Get()&&players::Registry::Get().Local()==player&&note.tower.Get()&&
            g_kept[i].used&&g_kept[i].revision==note.revision&&TowerById(FK::Kind::Coordinate,note.row.id)==tower;};
        if(!tower||!current())continue;
        bool opens=false,ran=false;
        const bool asked=FK::ConvergePanel(FK::Kind::Coordinate,tower,player,note.row,opens,ran,current);
        if(!current())continue;
        if(asked){g_kept[i].panelAskedMs=now;++g_edges[net::kFuseTowerKindCoordinate].panels;}
    }
}
} // namespace
void Install(net::Session* session){g_session.store(session);Reconcile();}
void Tick(){
    Reconcile();const auto scope=g_scope;if(!Current(scope)||g_ticking)return;
    struct Busy{Busy(){g_ticking=true;}~Busy(){g_ticking=false;}}busy;
    if(Host(scope)){ServeDeliveries(false);if(!Host(scope))return;HostSendRows();if(Host(scope))ServeDeliveries(true);return;}
    if(!g_haveRows||fuse_tower_ops::RedOpen())return;
    if(g_rowsWaiting&&Elapsed(NowMs(),g_waitLookMs)>=kLookupMs){g_waitLookMs=NowMs();Towers(true);if(Client(scope))ApplyAll();}
    if(Client(scope))ConvergePanels();
}
void AdmitRows(FuseTowerPayload& p){
    Reconcile();
    if(p.op!=net::kFuseTowerOpRows||p.count>kFuseTowers){p.count=0;return;}
    int kept=0;
    for(int i=0;i<p.count;++i){
        FuseTowerRow row=p.rows[i];bool valid=RowOk(row);
        for(int j=0;j<kept&&valid;++j)if(p.rows[j].kind==row.kind&&p.rows[j].id==row.id)valid=false;
        if(!valid){if(!g_badRowsSaid){g_badRowsSaid=true;UE_LOGW("fuse_tower: malformed or duplicate canonical row refused");}continue;}
        for(int f=row.fuseCount;f<net::kFuseTowerMaxFuses;++f)row.fuses[f]=0;
        p.rows[kept++]=row;
    }
    p.count=static_cast<uint8_t>(kept);
}
void OnRows(const FuseTowerPayload& incoming){
    Reconcile();const auto scope=g_scope;if(!Client(scope))return;
    FuseTowerPayload p=incoming;AdmitRows(p);if(!p.count||!Client(scope))return;
    const auto receive=++g_receiveRevision;const uint8_t me=players::Registry::Get().LocalPeerId();
    if(me<net::kMaxPeers)fuse_tower_ops::OnAcks(p.ack[me],p.refused[me]);
    if(!Client(scope)||receive!=g_receiveRevision)return;
    for(auto& k:g_kept){
        bool found=false;for(int i=0;i<p.count;++i)found=found||(k.used&&k.row.kind==p.rows[i].kind&&k.row.id==p.rows[i].id);
        if(k.used&&!found)k={};
    }
    g_rows=p;g_haveRows=true;
    for(int i=0;i<p.count;++i)if(auto* k=KeptFor(p.rows[i])){
        if(!SameRow(k->row,p.rows[i])){k->row=p.rows[i];k->revision=++g_revision;k->attempts=k->wanted=k->dirty=0;k->pending=true;}
        else{k->revision=++g_revision;k->pending=true;} // ACKs can change the local claim overlay without changing the canonical row.
    }
    g_rowsWaiting=Pending();if(!fuse_tower_ops::RedOpen())ApplyAll();
}
void QueueConnectBroadcastForSlot(int slot){
    Reconcile();const auto scope=g_scope;if(!Host(scope)||slot<=0||slot>=net::kMaxPeers)return;
    const auto generation=scope.session->peerGenerationForSlot(slot);if(!Peer(scope,slot,generation,false))return;
    g_join[slot]={true,generation,++g_revision};ServeDeliveries(false);
}
void HostSendNow(){Reconcile();if(!Host(g_scope))return;g_acksDirty=true;++g_dirtyRevision;HostSendRows();}
void HostAnswerOwed(uint8_t slot){
    Reconcile();const auto scope=g_scope;if(!Host(scope)||!slot||slot>=net::kMaxPeers)return;
    const auto generation=scope.session->peerGenerationForSlot(slot);if(Peer(scope,slot,generation,false))g_answer[slot]={true,generation,++g_revision};
}
bool Applying(){Reconcile();return g_applyDepth&&Current(g_applyScope);}
bool LastRows(FuseTowerPayload& out){Reconcile();if(!Client(g_scope)||!g_haveRows)return false;out=g_rows;return true;}
Edges AppliedEdges(uint8_t kind){Reconcile();return kind<net::kFuseTowerKinds?g_edges[kind]:Edges{};}
bool FirstAppliedFlags(uint8_t kind,int32_t id,uint8_t& flags){Reconcile();for(const auto& k:g_kept)if(k.used&&k.row.kind==kind&&k.row.id==id&&k.firstApplied){flags=k.firstFlags;return true;}return false;}
void ReapplyKept(uint8_t kind,int32_t id){
    Reconcile();if(!Client(g_scope)||!g_haveRows)return;
    for(auto& k:g_kept)if(k.used&&k.row.kind==kind&&k.row.id==id){
        k.pending=true;k.applied=false;k.appliedFlags=0;k.revision=++g_revision;k.attempts=k.wanted=k.dirty=0;g_rowsWaiting=true;g_waitLookMs=0;
    }
}
void OnDisconnect(){g_session.store(nullptr);Clear();g_scope=Capture();}
} // namespace coop::fuse_tower_rows
