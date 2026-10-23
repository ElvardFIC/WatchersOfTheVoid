#include "coop/world/ariral_ship_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/ariral_ship.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace coop::ariral_ship_sync {
namespace {
namespace A=ue_wrap::ariral_ship;
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
using Packet=net::AriralShipStatePayload;
using Clock=std::chrono::steady_clock;
using ue_wrap::CachedObjRef;
constexpr uint8_t Visible=1, Quiet=2, Retired=4;
constexpr int Tag=0x41534950;
constexpr size_t Limit=32;
net::Session* g_session=nullptr;
uint64_t g_epoch=1,g_world=0;
uint32_t g_serial=0,g_incarnation=0;
net::Role g_role=net::Role::Host;
Clock::time_point g_sample{};
bool g_ticking=false;
struct Delivery {uint32_t peer=0,revision=0;};
struct Row {
    Packet state{};
    CachedObjRef actor;
    std::array<Delivery,net::kMaxPeers> delivered{};
    uint32_t applied=0,handledCue=0;
    Clock::time_point cueUntil{};
};
std::map<std::string,Row> g_rows;
std::vector<CachedObjRef> g_quiet;
size_t g_sendCursor=0;
std::array<uint32_t,net::kMaxPeers> g_replays{};
// A stack-owned admission survives a reentrant disconnect; its epoch then fails.
struct Admission {CachedObjRef actor;uint64_t epoch=0;};
Admission g_binding;
bool Running() {return g_session && g_session->running();}
bool Client() {return Running() && g_session->role()==net::Role::Client;}
void Clear() {++g_epoch;g_rows.clear();g_quiet.clear();g_sample={};g_sendCursor=0;g_replays={};}
void Context() {
    const auto world=ue_wrap::world_identity::Generation();
    const auto serial=net::session_serial::Current();
    const auto role=g_session?g_session->role():net::Role::Host;
    if(world!=g_world || serial!=g_serial || role!=g_role) {
        Clear();g_world=world;g_serial=serial;g_role=role;
    }
}
bool Current(uint64_t epoch) {
    return epoch==g_epoch && Running() && g_serial==net::session_serial::Current() &&
        g_world==ue_wrap::world_identity::Generation() && g_role==g_session->role();
}
bool Newer(uint32_t a,uint32_t b) {return static_cast<int32_t>(a-b)>0;}
uint32_t Next(uint32_t& n) {if(!++n)++n;return n;}
bool NameChar(unsigned c) {return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';}
std::string Name(const Packet& p) {return {p.name.data,p.name.len};}
bool Name(const std::wstring& text,net::WireClassName& out) {
    if(text.empty() || text.size()>63)return false;
    out={};out.len=static_cast<uint8_t>(text.size());
    for(size_t i=0;i<text.size();++i) {if(!NameChar(text[i]))return false;out.data[i]=static_cast<char>(text[i]);}
    return true;
}
bool Valid(const Packet& p) {
    if(!p.name.len || p.name.len>63 || !p.generation || !p.revision || (p.flags&~7) ||
       p.reserved[0] || p.reserved[1] || p.reserved[2])return false;
    for(size_t i=0;i<63;++i)if(i<p.name.len ? !NameChar(static_cast<unsigned char>(p.name.data[i])) : p.name.data[i]!=0)return false;
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.alpha)&&
        std::abs(p.x)<=2000000 && std::abs(p.y)<=2000000 && std::abs(p.z)<=2000000 && p.alpha>=0 && p.alpha<=1 &&
        (!(p.flags&Retired) || (!(p.flags&Visible) && p.alpha==0));
}
bool Binding(void* actor) {return g_binding.epoch==g_epoch && Current(g_binding.epoch) && g_binding.actor.Is(actor);}
bool CanPhoto(void* actor) {
    for(const auto& [name,row]:g_rows)if(row.actor.Is(actor))return row.applied==row.state.revision &&
        (row.state.flags&Visible) && !(row.state.flags&Retired);
    return false;
}
SG::Verdict BlockClient(const SG::Call&) {return Client()?SG::Verdict::Cancel:SG::Verdict::Run;}
SG::Verdict Trigger(const SG::Call& c) {
    Context();return !Client() || Binding(c.object)?SG::Verdict::Run:SG::Verdict::Cancel;
}
SG::Verdict Photo(const SG::Call& c) {
    Context();return !Client() || CanPhoto(c.object)?SG::Verdict::Run:SG::Verdict::Cancel;
}
SG::Verdict Graph(const SG::Call& c) {
    if(!Client())return SG::Verdict::Run;
    Context();
    if(!c.function || !c.locals)return SG::Verdict::Cancel;
    const int off=R::FindParamOffset(c.function,L"EntryPoint"),size=R::FunctionFrameSize(c.function);
    if(off<0 || off>size || size-off<int(sizeof(int32_t)))return SG::Verdict::Cancel;
    int32_t entry=0;std::memcpy(&entry,c.locals+off,sizeof(entry));
    // 0.9.0n native entrypoints and Delay resumes, checked against the cooked graph.
    switch(entry) {
    case 15: case 31: case 383: case 604: case 908: case 1546: case 2585: case 2631: case 3075:
        return SG::Verdict::Cancel;
    case 1398:return Binding(c.object)?SG::Verdict::Run:SG::Verdict::Cancel;
    case 2686:return CanPhoto(c.object)?SG::Verdict::Run:SG::Verdict::Cancel;
    default:return SG::Verdict::Run; // BeginPlay/material initialization stays native.
    }
}
Row* Capture(void* actor,bool cue) {
    const auto epoch=g_epoch;
    A::Appearance value;
    if(!A::Read(actor,value) || !Current(epoch))return nullptr;
    Packet p{};if(!Name(value.name,p.name))return nullptr;
    const auto key=Name(p);auto it=g_rows.find(key);
    if(it==g_rows.end()) {
        if(g_rows.size()>=Limit)return nullptr;
        it=g_rows.emplace(key,Row{}).first;
    }
    auto& row=it->second;
    const bool birth=!row.actor.Is(actor);
    p.generation=birth?Next(g_incarnation):row.state.generation;
    p.revision=row.state.revision;p.cue=row.state.cue;
    if(cue)Next(p.cue);
    p.x=value.at.X;p.y=value.at.Y;p.z=value.at.Z;p.alpha=value.alpha;p.flags=value.visible?Visible:0;
    if(!p.revision || birth || cue || (row.state.flags&Retired) || p.flags!=row.state.flags ||
       std::abs(p.alpha-row.state.alpha)>=.002f || (p.alpha==0&&row.state.alpha!=0) || (p.alpha==1&&row.state.alpha!=1) ||
       p.x!=row.state.x || p.y!=row.state.y || p.z!=row.state.z) {
        Next(p.revision);row.state=p;row.actor.Set(actor);
    }
    return &row;
}
void Appeared(const SG::Call& c) {
    if(!Running() || g_session->role()!=net::Role::Host)return;
    Context();Capture(c.object,true);
}
struct Watch {const wchar_t* name;SG::PreFn pre;SG::PostFn post;bool registered=false;};
Watch g_watches[]={
    {L"appear",BlockClient,Appeared},{L"runTrigger",Trigger,nullptr},
    {L"ReceiveTick",BlockClient,nullptr},{L"a__UpdateFunc",BlockClient,nullptr},
    {L"a__FinishedFunc",BlockClient,nullptr},{L"photoTaken",Photo,nullptr},
    {L"ExecuteUbergraph_arirShipAppear",Graph,nullptr}
};
bool Watches() {
    bool ready=true;
    for(auto& w:g_watches) {
        if(!w.registered)w.registered=SG::WatchClassName(L"arirShipAppear_C",w.name,Tag,w.pre,w.post);
        ready &= w.registered && SG::ClassNameWatchLive(L"arirShipAppear_C",w.name,Tag);
    }
    return ready;
}
bool Subscribe(void* actor,bool active,const std::function<bool()>& current) {
    struct Scope {
        Admission saved=g_binding;
        Scope(void* actor) {g_binding.actor.Set(actor);g_binding.epoch=g_epoch;}
        ~Scope() {g_binding=saved;}
    } scope(actor);
    return A::PhotoSubscription(actor,active,current);
}
bool Used(void* actor,const std::string& except) {
    for(const auto& [name,row]:g_rows)if(name!=except && row.actor.Is(actor))return true;
    return false;
}
bool Near(const A::Appearance& a,const Packet& p) {
    const double x=a.at.X-p.x,y=a.at.Y-p.y,z=a.at.Z-p.z;return x*x+y*y+z*z<=100.;
}
void Apply(const std::string& key,const std::vector<CachedObjRef>& actors) {
    auto it=g_rows.find(key);if(it==g_rows.end())return;
    const auto epoch=g_epoch;const Packet state=it->second.state;
    CachedObjRef actor=it->second.actor;
    if(!actor.Get()) {
        CachedObjRef match;size_t count=0;
        for(const auto& ref:actors)if(void* obj=ref.Get()) {
            A::Appearance value;
            if(!A::Read(obj,value) || !Current(epoch))return;
            if(Used(obj,key) || !Near(value,state))continue;
            net::WireClassName n{};
            if(Name(value.name,n) && n.len==state.name.len && !std::memcmp(n.data,state.name.data,n.len)) {match=ref;count=1;break;}
            match=ref;++count;
        }
        if(count!=1 || !match.Get())return;
        it=g_rows.find(key);if(it==g_rows.end() || it->second.state.generation!=state.generation ||
            it->second.state.revision!=state.revision || !Current(epoch))return;
        it->second.actor=match;it->second.applied=0;actor=match;
    }
    const auto current=[=] {
        auto found=g_rows.find(key);return Current(epoch) && actor.Get() && found!=g_rows.end() &&
            found->second.actor.Is(actor.Get()) && found->second.state.generation==state.generation && found->second.state.revision==state.revision;
    };
    it=g_rows.find(key);
    if(it->second.applied!=state.revision) {
        A::Appearance value;value.alpha=state.alpha;value.visible=(state.flags&Visible)!=0;
        if(!A::Paint(actor.Get(),value,current) || !current() || !Subscribe(actor.Get(),value.visible,current) || !current() ||
           !A::Activity(actor.Get(),value.visible,current) || !current())return;
        g_rows.at(key).applied=state.revision;
    }
    it=g_rows.find(key);
    if(it->second.handledCue==state.cue)return;
    const auto until=it->second.cueUntil;
    it->second.handledCue=state.cue; // Claim before native dispatch; a reentrant sample cannot replay it.
    if(Clock::now()>until)return;
    const auto life=[=] {
        auto found=g_rows.find(key);return Current(epoch) && actor.Get() && found!=g_rows.end() &&
            found->second.actor.Is(actor.Get()) && found->second.state.generation==state.generation;
    };
    if(!A::Air(actor.Get(),life) && life()) {
        auto& row=g_rows.at(key);
        if(row.state.cue==state.cue)row.handledCue=state.cue-1; // Retry only this live cue, within its original deadline.
    }
}
void ClientTick() {
    const auto epoch=g_epoch;
    std::vector<CachedObjRef> actors;
    for(void* obj:A::Actors()) {CachedObjRef ref;ref.Set(obj);actors.push_back(ref);}
    std::vector<std::string> keys;for(const auto& [key,row]:g_rows)keys.push_back(key);
    for(const auto& key:keys) {Apply(key,actors);if(!Current(epoch))return;}
    for(const auto& ref:actors)if(void* actor=ref.Get()) {
        if(Used(actor,{}))continue;
        bool quiet=false;for(const auto& q:g_quiet)quiet|=q.Is(actor);
        A::Appearance value;if(!A::Read(actor,value) || !Current(epoch))return;
        if(quiet && !value.visible && value.alpha==0)continue;
        const auto current=[=]{return Current(epoch) && ref.Get() && !Used(ref.Get(),{});};
        value.alpha=0;value.visible=false;
        if(A::Paint(actor,value,current) && current() && Subscribe(actor,false,current) && current() &&
           A::Activity(actor,false,current) && current() && !quiet) {
            // Replace dead references rather than growing across scene reloads.
            bool stored=false;for(auto& q:g_quiet)if(!q.Get()){q=ref;stored=true;break;}
            if(!stored && g_quiet.size()<Limit)g_quiet.push_back(ref);
        }
        if(!Current(epoch))return;
    }
}
void HostTick() {
    const auto epoch=g_epoch;
    if(g_sample==Clock::time_point{} || Clock::now()-g_sample>=std::chrono::milliseconds(50)) {
        g_sample=Clock::now();
        std::vector<CachedObjRef> actors;
        for(void* actor:A::Actors()) {CachedObjRef ref;ref.Set(actor);actors.push_back(ref);}
        for(const auto& ref:actors)if(void* actor=ref.Get()) {
            Capture(actor,false);if(!Current(epoch))return;
        }
        for(auto& [key,row]:g_rows)if(!row.actor.Get() && !(row.state.flags&Retired)) {
            row.state.flags=Retired;row.state.alpha=0;Next(row.state.revision);
        }
    }
    // A rotating bounded queue retains the newest state after queue refusal.
    std::vector<std::pair<std::string,int>> work;
    for(const auto& [key,row]:g_rows)for(int slot=1;slot<net::kMaxPeers;++slot)
        if(g_session->HasPeerConn(slot) && g_session->IsSlotWorldReady(slot))work.emplace_back(key,slot);
    if(work.empty())return;
    const size_t start=g_sendCursor%work.size();size_t sent=0;
    for(size_t n=0;n<work.size() && sent<16;++n) {
        const auto& [key,slot]=work[(start+n)%work.size()];g_sendCursor=(start+n+1)%work.size();
        auto it=g_rows.find(key);if(it==g_rows.end())continue;
        const auto peer=g_session->peerGenerationForSlot(slot),replay=g_replays[slot];const auto debt=it->second.delivered[slot];
        Packet p=it->second.state;
        if(!p.revision || (debt.peer==peer && debt.revision==p.revision))continue;
        if(debt.peer!=peer || !debt.revision)p.flags|=Quiet;
        ++sent;
        const bool accepted=g_session->SendReliableToSlot(slot,net::ReliableKind::AriralShipState,&p,sizeof(p));
        if(!Current(epoch))return;
        it=g_rows.find(key);
        if(accepted && it!=g_rows.end() && g_replays[slot]==replay && it->second.state.revision==p.revision && it->second.state.generation==p.generation &&
           g_session->HasPeerConn(slot) && g_session->IsSlotWorldReady(slot) && g_session->peerGenerationForSlot(slot)==peer)
            it->second.delivered[slot]={peer,p.revision};
    }
}
}
void Install(net::Session* session) {
    if(session!=g_session) {Clear();g_session=session;}
    Context();Watches();
}
void Tick() {
    if(!Running() || g_ticking)return;
    struct Scope {Scope(){g_ticking=true;}~Scope(){g_ticking=false;}} scope;
    Context();const auto epoch=g_epoch;
    SG::ResolvePendingNames();if(!Current(epoch) || !Watches())return;
    if(Client())ClientTick();else HostTick();
}
void Receive(const Packet& p,int sender) {
    if(sender!=0 || !Client() || !Valid(p))return;
    Context();const auto key=Name(p);auto it=g_rows.find(key);
    const bool first=it==g_rows.end();
    if(first) {if(g_rows.size()>=Limit)return;it=g_rows.emplace(key,Row{}).first;}
    auto& row=it->second;
    if(!first && (p.generation!=row.state.generation ? !Newer(p.generation,row.state.generation) : !Newer(p.revision,row.state.revision)))return;
    const bool birth=first || p.generation!=row.state.generation;
    if(birth) {row.actor.Reset();row.applied=0;}
    if(birth || (p.flags&Quiet)) {row.handledCue=p.cue;row.cueUntil={};}
    else if(p.cue!=row.state.cue)row.cueUntil=Clock::now()+std::chrono::seconds(1);
    row.state=p;
}
void QueueConnectBroadcastForSlot(int slot) {
    if(!Running() || g_session->role()!=net::Role::Host || slot<=0 || slot>=net::kMaxPeers)return;
    Context();Next(g_replays[slot]);for(auto& [key,row]:g_rows)row.delivered[slot]={};
}
void OnDisconnect() {Clear();g_session=nullptr;}
}
