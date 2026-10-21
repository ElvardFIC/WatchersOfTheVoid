#include "coop/world/wall_creative_sync.h"
#include "coop/world/placed_save_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/actors/wall_creative.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/world/keyed_objects.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <vector>
namespace coop::wall_creative_sync {
namespace {
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
namespace Native=ue_wrap::wall_creative;
using Packet=net::WallCreativePayload;
using ue_wrap::CachedObjRef;
struct Client {Packet packet{};CachedObjRef wall,tool;uint64_t until=0,next=0;bool sent=false;};
struct Host {Packet packet{};uint64_t until=0,next=0;uint8_t reply=0;bool pending=false,source=false;};
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0;
uint64_t g_epoch=0;
bool g_ticking=false;
std::map<uint32_t,Client> g_client;
std::array<std::map<uint32_t,Host>,net::kMaxPeers> g_host;
std::pair<uint8_t,uint32_t> g_workCursor{};
std::set<std::wstring> g_claimed;
uint16_t g_people[net::kMaxPeers]{};
void* g_building=nullptr;
std::function<bool()> g_nativeCurrent;
bool Current(){return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
bool Current(uint64_t epoch){return Current() && g_epoch==epoch;}
void World() {
    if(g_session && g_world!=ue_wrap::world_identity::Generation()) {
        auto* session=g_session;OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();
    }
}
bool Key(const net::WireKey& key) {
    if(!key.len || key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<33 || uint8_t(key.data[i])>126)return false;
    return net::StringFromWireKey(key)!=L"None";
}
void Reconcile() {
    for(int slot=0;slot<net::kMaxPeers;++slot) {
        const auto person=roster_ledger::Get(slot).playerNo;
        if(g_people[slot]==person)continue;
        g_host[slot].clear();
        if(g_people[slot] && slot==players::Registry::Get().LocalPeerId())g_client.clear();
        g_people[slot]=person;
    }
}
bool Same(const Packet& a,const Packet& b) {
    return a.sequence==b.sequence && a.generation==b.generation && a.playerNo==b.playerNo &&
        a.slot==b.slot && net::StringFromWireKey(a.key)==net::StringFromWireKey(b.key);
}
void Complete(uint8_t slot,uint32_t sequence,uint8_t reply,uint64_t now) {
    const auto it=g_host[slot].find(sequence);if(it==g_host[slot].end())return;
    it->second.reply=reply;it->second.pending=true;it->second.until=now+15000;it->second.next=0;
}
void Process(const Packet& p,uint64_t now) {
    const auto epoch=g_epoch;
    const auto current=[&]{return Current(epoch) && roster_ledger::Get(p.slot).playerNo==p.playerNo &&
        g_session->IsSlotWorldReady(p.slot);};
    const auto key=net::StringFromWireKey(p.key);
    if(!current())return;
    if(g_claimed.count(key)){Complete(p.slot,p.sequence,2,now);return;}
    auto pending=g_host[p.slot].find(p.sequence);if(pending==g_host[p.slot].end())return;
    if(!pending->second.source) {
        const auto generation=hand_item::Generation(p.slot);
        if(generation!=p.generation) {
            if(generation && int32_t(p.generation-generation)<0)Complete(p.slot,p.sequence,2,now);
            return;
        }
        if(!hand_item::HeldClassIs(p.slot,L"prop_wallbuilder_C")){Complete(p.slot,p.sequence,2,now);return;}
        pending->second.source=true;
    }
    void* actor=ue_wrap::keyed_objects::Resolve(key.c_str());if(!actor)return;
    if(ue_wrap::placed_save::Type(actor)!=ue_wrap::placed_save::Kind::WallOutline){Complete(p.slot,p.sequence,2,now);return;}
    CachedObjRef wall;wall.Set(actor);
    if(!wall.Get())return;
    const auto canonical=ue_wrap::wall_build::Key(actor);
    if(!current() || !wall.Get())return;
    if(canonical!=key){Complete(p.slot,p.sequence,2,now);return;}
    if(!placed_save_sync::OwnsClientWall(key,actor,p.slot,p.playerNo))return;
    bool allowed=false;
    if(!Native::Allowed(actor,allowed))return;
    if(!current() || !wall.Get())return;
    if(!allowed || g_claimed.size()>=4096){Complete(p.slot,p.sequence,2,now);return;}
    const bool reachable=bool(element::IntentTarget::ForClientIntent(*g_session,p.slot,500.f).Authorize(actor));
    if(!current() || !wall.Get())return;
    if(!reachable){Complete(p.slot,p.sequence,2,now);return;}
    placed_save_sync::TrackWall(actor);
    if(!current() || !wall.Get())return;
    const auto valid=[&]{return current() && wall.Get();};
    g_claimed.insert(key);
    Native::Build result;
    {
        struct Guard {
            void* previous=g_building;std::function<bool()> prior=g_nativeCurrent;
            Guard(void* actor,const std::function<bool()>& current){g_building=actor;g_nativeCurrent=current;}
            ~Guard(){g_building=previous;g_nativeCurrent=std::move(prior);}
        } guard(actor,valid);
        result=Native::Finish(actor,valid);
    }
    if(!Current(epoch))return;
    if(result==Native::Build::Retry){g_claimed.erase(key);return;}
    if(current())Complete(p.slot,p.sequence,result==Native::Build::Dispatched?1:3,now);
}
}
void Install(net::Session* session) {
    if(g_session!=session || g_world!=ue_wrap::world_identity::Generation()) {
        OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();
    }
}
SG::Verdict BeforeBuild(const SG::Call& call) {
    World();
    if(!Current() || g_session->role()!=net::Role::Client)return SG::Verdict::Cancel;
    if(call.fromOurCode || !call.function || !call.locals || call.callerObject!=hand_item::LocalHandActor() ||
       !Native::IsBuilderCall(call.callerObject,call.callerFunction) ||
       ue_wrap::placed_save::Type(call.object)!=ue_wrap::placed_save::Kind::WallOutline)return SG::Verdict::Cancel;
    int offset=-1;uint8_t mask=0;const auto size=R::FunctionFrameSize(call.function);
    if(!R::FindBoolProperty(call.function,L"isCheat",offset,mask) || offset<0 || offset>=size || !mask ||
       !(call.locals[offset]&mask))return SG::Verdict::Cancel;
    for(const auto& [sequence,row]:g_client)if(row.wall.Is(call.object))return SG::Verdict::Cancel;
    const auto slot=players::Registry::Get().LocalPeerId();if(slot>=net::kMaxPeers || g_client.size()>=16)return SG::Verdict::Cancel;
    Client row{};row.wall.Set(call.object);row.tool.Set(call.callerObject);row.until=active_drive::NowMs()+10000;
    row.packet.slot=slot;row.packet.playerNo=roster_ledger::Get(slot).playerNo;row.packet.generation=hand_item::Generation(slot);
    if(!++g_sequence)++g_sequence;row.packet.sequence=g_sequence;
    g_client.emplace(g_sequence,row);return SG::Verdict::Cancel;
}
void Tick() {
    World();
    if(!Current() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    Reconcile();const auto epoch=g_epoch,now=active_drive::NowMs();
    if(g_session->role()==net::Role::Client) {
        const auto pending=g_client;
        for(const auto& [sequence,captured]:pending) {
            auto it=g_client.find(sequence);if(it==g_client.end())continue;
            auto row=it->second;auto& p=row.packet;
            if(now>row.until || !row.wall.Get() || (!row.sent && (!row.tool.Get() || row.tool.Get()!=hand_item::LocalHandActor())) ||
               (p.playerNo && p.playerNo!=roster_ledger::Get(p.slot).playerNo)) {g_client.erase(it);continue;}
            if(now<row.next)continue;
            if(!row.sent) {
                if(!p.playerNo)p.playerNo=roster_ledger::Get(p.slot).playerNo;
                const auto generation=hand_item::Generation(p.slot);
                if(!p.generation)p.generation=generation;
                if(!p.playerNo || !p.generation || generation!=p.generation)continue;
                const auto key=ue_wrap::wall_build::Key(row.wall.Get());
                if(!Current(epoch))return;
                if(key.empty() || key.size()>sizeof(p.key.data))continue;
                net::WireKeyFromString(key,p.key);if(!Key(p.key))continue;
            }
            row.next=now+500;g_client[sequence]=row;
            const bool sent=g_session->SendReliableToSlot(0,net::ReliableKind::WallCreative,&p,sizeof(p));
            if(!Current(epoch))return;
            it=g_client.find(sequence);if(sent && it!=g_client.end())it->second.sent=true;
        }
        return;
    }
    int work=0;
    std::vector<std::pair<uint8_t,uint32_t>> pending;
    for(uint8_t slot=1;slot<net::kMaxPeers;++slot)
        for(const auto& [sequence,row]:g_host[slot])pending.emplace_back(slot,sequence);
    const auto first=size_t(std::upper_bound(pending.begin(),pending.end(),g_workCursor)-pending.begin());
    for(size_t visited=0;visited<pending.size();++visited) {
        const auto [slot,sequence]=pending[(first+visited)%pending.size()];
        auto it=g_host[slot].find(sequence);if(it==g_host[slot].end())continue;
        const auto row=it->second;
        if(row.reply && now>row.until){g_host[slot].erase(it);continue;}
        if(!row.reply && now>=row.until)Complete(slot,sequence,2,now);
        else if(!row.reply && now>=row.next && work<4) {
            g_workCursor={slot,sequence};
            ++work;it->second.next=now+200;Process(row.packet,now);
            if(!Current(epoch))return;
        }
        it=g_host[slot].find(sequence);if(it==g_host[slot].end() || !it->second.reply || !it->second.pending)continue;
        auto reply=it->second.packet;reply.op=it->second.reply;
        const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::WallCreative,&reply,sizeof(reply));
        if(!Current(epoch))return;
        it=g_host[slot].find(sequence);
        if(sent && it!=g_host[slot].end() && Same(it->second.packet,reply))it->second.pending=false;
    }
}
void Receive(const Packet& p,uint8_t sender) {
    World();
    if(!Current() || p.slot>=net::kMaxPeers || !p.sequence || !p.generation || !p.playerNo || p.op>3 ||
       p.playerNo!=roster_ledger::Get(p.slot).playerNo || !Key(p.key))return;
    Reconcile();
    if(g_session->role()==net::Role::Client) {
        if(sender!=0 || !p.op || p.slot!=players::Registry::Get().LocalPeerId())return;
        const auto it=g_client.find(p.sequence);
        if(it!=g_client.end() && Same(it->second.packet,p))g_client.erase(it);
        return;
    }
    if(sender==0 || sender>=net::kMaxPeers || sender!=p.slot || p.op || !g_session->IsSlotWorldReady(sender))return;
    auto& rows=g_host[sender];const auto it=rows.find(p.sequence);
    if(it!=rows.end()) {if(Same(it->second.packet,p) && it->second.reply)it->second.pending=true;return;}
    if(rows.size()<64) {
        const bool source=hand_item::Generation(sender)==p.generation && hand_item::HeldClassIs(sender,L"prop_wallbuilder_C");
        rows.emplace(p.sequence,Host{p,active_drive::NowMs()+10000,0,0,false,source});
    }
}
void OnDisconnect() {
    ++g_epoch;g_session=nullptr;g_world=g_sequence=0;g_client.clear();g_claimed.clear();g_workCursor={};
    for(auto& rows:g_host)rows.clear();for(auto& person:g_people)person=0;
}
bool NativeCurrent(void* wall) {return wall!=g_building || !g_nativeCurrent || g_nativeCurrent();}
}
