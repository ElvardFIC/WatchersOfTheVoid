#include "coop/interactables/grill_edits.h"
#include "coop/interactables/kitchen_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_save_data.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/devices/grill.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <deque>
#include <set>
#include <vector>

namespace coop::grill_edits {
namespace {
namespace G=ue_wrap::grill;
using Packet=net::GrillEditPayload;
using ue_wrap::CachedObjRef;
struct Capture {CachedObjRef actor;Packet packet{};void* stack=nullptr;void* function=nullptr;int depth=0;uint64_t epoch=0,invocation=0,peerEpoch=0;};
struct Local {Packet packet{};uint64_t next=0;};
struct Host {Packet packet{};CachedObjRef actor;uint64_t until=0,next=0;bool bound=false,processing=false,owed=false,kitchen=false,record=false;};
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0;
uint64_t g_epoch=0;
bool g_ticking=false;
size_t g_cursor=0;
std::vector<Capture> g_captures;
std::deque<Local> g_local;
std::deque<Host> g_host;
std::array<uint16_t,net::kMaxPeers> g_people{};
std::array<uint32_t,net::kMaxPeers> g_high{};
std::array<std::bitset<256>,net::kMaxPeers> g_seen{};
std::array<uint64_t,net::kMaxPeers> g_rateAt{};
std::array<unsigned,net::kMaxPeers> g_rate{};
std::array<uint64_t,net::kMaxPeers> g_peerEpoch{};
uint64_t Now(){using namespace std::chrono;return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());}
bool Current(uint64_t epoch){return g_session && g_session->connected() && g_epoch==epoch && g_world==ue_wrap::world_identity::Generation();}
bool Current(){return Current(g_epoch);}
void PruneCaptures(){std::erase_if(g_captures,[](const Capture& row){return !Current(row.epoch) ||
    !row.actor.Get() || !ue_wrap::script_gate::IsInvocationActive(row.invocation);});}
void World(){if(g_session && g_world!=ue_wrap::world_identity::Generation()){
    auto* session=g_session;OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();}}
bool Key(const net::WireKey& key){
    if(!key.len || key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<33 || uint8_t(key.data[i])>126)return false;
    return net::StringFromWireKey(key)!=L"None";
}
bool Valid(const Packet& p){
    if(!p.sequence || !p.playerNo || !p.slot || p.slot>=net::kMaxPeers || !Key(p.key) || p.result>3 ||
       p.reserved[0] || p.reserved[1] || p.reserved[2])return false;
    return p.operation==1?(p.before>=0 && p.before<16 && p.after==p.before+1):
        (p.operation==2 && (p.before==0 || p.before==1) && (p.after==0 || p.after==1) && p.before!=p.after);
}
bool Same(const Packet& a,const Packet& b){return a.sequence==b.sequence && a.playerNo==b.playerNo && a.slot==b.slot &&
    a.operation==b.operation && a.before==b.before && a.after==b.after && net::StringFromWireKey(a.key)==net::StringFromWireKey(b.key);}
Host* Find(const Packet& p){for(auto& row:g_host)if(row.packet.slot==p.slot && row.packet.playerNo==p.playerNo && row.packet.sequence==p.sequence)return &row;return nullptr;}
bool Unseen(uint8_t slot,uint32_t sequence){const auto high=g_high[slot];if(!high || int32_t(sequence-high)>0)return true;
    const auto distance=high-sequence;return distance<256 && !g_seen[slot][distance];}
void Remember(uint8_t slot,uint32_t sequence){auto& high=g_high[slot];auto& seen=g_seen[slot];
    if(!high){seen.reset();high=sequence;}else if(int32_t(sequence-high)>0){const auto distance=sequence-high;if(distance>=256)seen.reset();else seen<<=distance;high=sequence;}
    seen.set(high-sequence);
}
bool Reconcile(){const auto epoch=g_epoch;for(int slot=0;slot<net::kMaxPeers;++slot){
    const auto person=roster_ledger::Get(slot).playerNo;if(g_people[slot]==person)continue;
    if(g_people[slot])OnPeerLeft(slot);if(!Current(epoch))return false;g_people[slot]=person;}return true;}
bool Shared(void* actor){return actor && G::IsGrill(actor) && !hand_item::IsHandAxisActor(actor) && net_pump::IsInAnnouncedWorld(actor);}
void Finish(const Packet& p,uint8_t result,uint64_t now){if(auto* row=Find(p)){
    // One retained publication per key, always sampled from the latest actor.
    for(auto& older:g_host)if(!Same(older.packet,p) && net::StringFromWireKey(older.packet.key)==net::StringFromWireKey(p.key))older.kitchen=older.record=false;
    row->packet.result=result;row->processing=false;row->owed=true;row->until=now+15000;row->next=0;
    row->kitchen=row->record=row->bound;
}}
bool Earlier(const Packet& p){for(const auto& row:g_host){if(Same(row.packet,p))return false;
    if(!row.packet.result && net::StringFromWireKey(row.packet.key)==net::StringFromWireKey(p.key))return true;}return false;}
void Process(const Packet& p,uint64_t now){
    const auto epoch=g_epoch;auto* session=g_session;
    const auto peerEpoch=g_peerEpoch[p.slot];
    const auto person=[&]{return Current(epoch) && session==g_session && session->role()==net::Role::Host &&
        g_peerEpoch[p.slot]==peerEpoch && roster_ledger::Get(p.slot).playerNo==p.playerNo && session->IsSlotWorldReady(p.slot);};
    if(!person())return;
    const auto key=net::StringFromWireKey(p.key);auto* row=Find(p);if(!row || Earlier(p))return;
    if(!row->bound){
        void* actor=prop_element_tracker::FindLiveActorByKey(key);
        if(!person())return;
        row=Find(p);if(!row)return;
        if(!actor)return;row->actor.Set(actor);row->bound=true;
    }
    const auto identity=row->actor;void* actor=identity.Get();
    const auto current=[&]{
        if(!person() || !actor || identity.Get()!=actor || !Find(p) || Find(p)->packet.result ||
           hand_item::IsHandAxisActor(actor) || !net_pump::IsInAnnouncedWorld(actor))return false;
        const bool sameKey=ue_wrap::prop::GetKeyString(actor)==key;
        return sameKey && person() && identity.Get()==actor && Find(p) && !Find(p)->packet.result;
    };
    if(!actor){Finish(p,2,now);return;}
    const bool shared=Shared(actor);if(!current())return;
    const auto actual=ue_wrap::prop::GetKeyString(actor);if(!current())return;
    if(!shared || actual!=key){Finish(p,2,now);return;}
    const bool prepared=prop_save_data::PrepareForMutation(actor,key);if(!current())return;if(!prepared)return;
    const bool reach=bool(element::IntentTarget::ForClientIntent(*session,p.slot,500.f).Authorize(actor));
    if(!current())return;if(!reach){Finish(p,2,now);return;}
    G::Stock stock{};const bool read=G::ReadStock(actor,stock,current);if(!current())return;if(!read)return;
    if(p.operation==1 && stock.coals>=16){Finish(p,2,now);return;}
    if(p.operation==2 && stock.lid==bool(p.after)){Finish(p,1,now);return;}
    // The queue owns this attempt before native updLid can re-enter receive/tick.
    row=Find(p);if(!row)return;row->processing=true;
    const auto edited=G::ChangeStock(actor,p.operation,p.operation==1?stock.coals+1:p.after,current);
    if(!person())return;
    row=Find(p);if(!row)return;
    if(edited==G::Edit::Retry){row->processing=false;return;}
    Finish(p,edited==G::Edit::Applied?1:3,now);
}
}

void Install(net::Session* session){if(g_session!=session || g_world!=ue_wrap::world_identity::Generation()){
    OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();}}
void BeforeChange(const ue_wrap::script_gate::Call& call,uint8_t operation){
    World();if(!Current() || g_session->role()!=net::Role::Client || call.fromOurCode || !Reconcile() || !Shared(call.object))return;
    PruneCaptures();if(!call.invocation)return;
    const auto epoch=g_epoch;const auto slot=players::Registry::Get().LocalPeerId();
    if(!slot || slot>=net::kMaxPeers || !roster_ledger::Get(slot).playerNo || g_captures.size()>=32)return;
    Capture row{};row.actor.Set(call.object);row.stack=call.stack;row.function=call.function;row.depth=call.depth;row.epoch=epoch;
    row.invocation=call.invocation;row.peerEpoch=g_peerEpoch[slot];row.packet.slot=slot;row.packet.playerNo=roster_ledger::Get(slot).playerNo;
    const auto current=[&]{return Current(epoch) && g_session->role()==net::Role::Client && row.actor.Get() &&
        g_peerEpoch[slot]==row.peerEpoch && players::Registry::Get().LocalPeerId()==slot &&
        roster_ledger::Get(slot).playerNo==row.packet.playerNo && ue_wrap::script_gate::IsInvocationActive(row.invocation) &&
        !hand_item::IsHandAxisActor(row.actor.Get());};
    G::Stock stock{};if(!G::ReadStock(call.object,stock,current) || !current())return;
    const auto key=ue_wrap::prop::GetKeyString(call.object);if(!current() || key.empty() || key.size()>31)return;
    auto& p=row.packet;net::WireKeyFromString(key,p.key);if(!Key(p.key))return;
    p.operation=operation;p.before=operation==1?stock.coals:int32_t(stock.lid);
    g_captures.push_back(row);
}
void AfterChange(const ue_wrap::script_gate::Call& call){
    auto it=std::find_if(g_captures.rbegin(),g_captures.rend(),[&](const Capture& row){return row.invocation==call.invocation && row.actor.Raw()==call.object && row.stack==call.stack && row.function==call.function && row.depth==call.depth;});
    if(it==g_captures.rend())return;auto row=*it;g_captures.erase(std::next(it).base());
    auto& p=row.packet;const auto current=[&]{return Current(row.epoch) && row.actor.Get() && g_session->role()==net::Role::Client &&
        g_peerEpoch[p.slot]==row.peerEpoch && ue_wrap::script_gate::IsInvocationActive(row.invocation) &&
        p.playerNo==roster_ledger::Get(p.slot).playerNo && p.slot==players::Registry::Get().LocalPeerId();};
    if(!current())return;
    G::Stock stock{};if(!G::ReadStock(call.object,stock,current) || !current())return;
    const auto key=ue_wrap::prop::GetKeyString(call.object);if(!current() || key!=net::StringFromWireKey(p.key))return;
    p.after=p.operation==1?stock.coals:int32_t(stock.lid);if(p.before==p.after)return;
    if(!++g_sequence)++g_sequence;p.sequence=g_sequence;if(!Valid(p))return;
    if(g_local.size()>=128){UE_LOGW("grill: edit queue full; native personal use is not refunded");return;}
    g_local.push_back({p,0});
}
void Tick(){
    World();if(!Current() || g_ticking)return;
    PruneCaptures();
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;
    if(!Reconcile())return;
    const auto epoch=g_epoch,now=Now();auto* session=g_session;
    if(session->role()==net::Role::Client){
        std::set<std::wstring> keys;const auto pending=g_local;
        for(const auto& copy:pending){const auto p=copy.packet;const auto key=net::StringFromWireKey(p.key);
            if(!keys.insert(key).second)continue;
            auto it=std::find_if(g_local.begin(),g_local.end(),[&](const Local& row){return Same(row.packet,p);});
            if(it==g_local.end() || now<it->next || p.playerNo!=roster_ledger::Get(p.slot).playerNo)continue;
            it->next=now+500;session->SendReliableToSlot(0,net::ReliableKind::GrillEdit,&p,sizeof(p));
            if(!Current(epoch) || session!=g_session)return;
        }
        return;
    }
    const std::vector<Host> pending(g_host.begin(),g_host.end());unsigned work=8;
    const auto first=pending.empty()?0:g_cursor%pending.size();
    for(size_t visited=0;visited<pending.size();++visited){
        const auto index=(first+visited)%pending.size();const auto p=pending[index].packet;
        auto* row=Find(p);if(!row || row->processing)continue;
        if(!row->packet.result && now>=row->until)Finish(p,2,now);
        row=Find(p);if(!row)continue;
        if(!row->packet.result && now>=row->next && work && !Earlier(p)){
            --work;g_cursor=(index+1)%pending.size();row->next=now+200;Process(p,now);if(!Current(epoch) || session!=g_session)return;
        }
        row=Find(p);if(!row || !row->packet.result)continue;
        if((row->kitchen || row->record) && now>=row->next && work){
            --work;g_cursor=(index+1)%pending.size();row->next=now+250;
            const auto identity=row->actor;void* actor=identity.Get();
            if(!actor){row->kitchen=row->record=false;}
            else{
                const bool kitchen=row->kitchen,record=row->record;const auto key=net::StringFromWireKey(p.key);
                const auto peerEpoch=g_peerEpoch[p.slot];
                const auto bound=[&]{return Current(epoch) && session==g_session && session->role()==net::Role::Host && identity.Get()==actor &&
                    g_peerEpoch[p.slot]==peerEpoch && roster_ledger::Get(p.slot).playerNo==p.playerNo &&
                    !hand_item::IsHandAxisActor(actor) && net_pump::IsInAnnouncedWorld(actor);};
                const auto current=[&]{return bound() && ue_wrap::prop::GetKeyString(actor)==key && bound();};
                const auto actual=ue_wrap::prop::GetKeyString(actor);if(!bound())return;
                if(actual!=key){row=Find(p);if(row)row->kitchen=row->record=false;}
                else{
                    const bool painted=!kitchen || kitchen_sync::PublishCurrent(actor);if(!current())return;
                    const bool saved=!record || prop_save_data::Publish(session,actor,key);if(!current())return;
                    row=Find(p);if(!row)continue;if(painted)row->kitchen=false;if(saved)row->record=false;
                }
            }
        }
        row=Find(p);if(!row)continue;
        if(row->owed){const auto reply=row->packet;const auto person=reply.playerNo;const auto peerEpoch=g_peerEpoch[p.slot];
            const bool sent=session->SendReliableToSlot(p.slot,net::ReliableKind::GrillEdit,&reply,sizeof(reply));
            if(!Current(epoch) || session!=g_session)return;
            row=Find(p);if(row && sent && g_peerEpoch[p.slot]==peerEpoch && roster_ledger::Get(p.slot).playerNo==person)row->owed=false;
        }
        row=Find(p);if(row && now>=row->until && !row->owed && !row->kitchen && !row->record){
            std::erase_if(g_host,[&](const Host& old){return Same(old.packet,p);});
        }
    }
}
void Receive(const Packet& p,uint8_t sender){
    World();if(!Current() || !Valid(p) || p.playerNo!=roster_ledger::Get(p.slot).playerNo || !Reconcile())return;
    if(g_session->role()==net::Role::Client){
        if(sender || !p.result || p.slot!=players::Registry::Get().LocalPeerId())return;
        std::erase_if(g_local,[&](const Local& row){return Same(row.packet,p);});return;
    }
    if(sender!=p.slot || !sender || p.result || !g_session->IsSlotWorldReady(sender))return;
    if(auto* row=Find(p)){if(Same(row->packet,p) && row->packet.result)row->owed=true;return;}
    if(g_host.size()>=128)return;
    const auto count=std::count_if(g_host.begin(),g_host.end(),[&](const Host& row){return row.packet.slot==sender;});
    if(count>=32)return;
    const auto now=Now();if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rate[sender]=0;}
    if(g_rate[sender]>=32)return;++g_rate[sender];
    Host row{};row.packet=p;row.until=now+5000;
    if(!Unseen(sender,p.sequence)){row.packet.result=2;row.owed=true;row.until=now+15000;}
    else {
        const auto epoch=g_epoch,peerEpoch=g_peerEpoch[sender];
        void* actor=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key));
        if(!Current(epoch) || g_peerEpoch[sender]!=peerEpoch || roster_ledger::Get(sender).playerNo!=p.playerNo)return;
        if(actor){row.actor.Set(actor);row.bound=true;}
        Remember(sender,p.sequence);
    }
    g_host.push_back(row);
}
void OnPeerLeft(int slot){
    if(slot<0 || slot>=net::kMaxPeers)return;
    ++g_peerEpoch[slot];
    std::erase_if(g_host,[slot](const Host& row){return row.packet.slot==slot;});
    std::erase_if(g_local,[slot](const Local& row){return row.packet.slot==slot;});
    std::erase_if(g_captures,[slot](const Capture& row){return row.packet.slot==slot;});
    g_people[slot]=0;g_high[slot]=0;g_seen[slot].reset();g_rate[slot]=0;g_rateAt[slot]=0;
}
void OnDisconnect(){++g_epoch;g_session=nullptr;g_world=0;g_local.clear();g_host.clear();g_captures.clear();
    g_people={};g_high={};g_seen={};g_rate={};g_rateAt={};g_cursor=0;}
}
