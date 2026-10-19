#include "coop/world/cargo_lift_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/cargo_lift.h"
#include "ue_wrap/engine/world_identity.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>

namespace coop::cargo_lift_sync {
namespace {
namespace L=ue_wrap::cargo_lift;
namespace R=ue_wrap::reflection;
namespace sg=ue_wrap::script_gate;
using net::CargoLiftPayload;
using ue_wrap::CachedObjRef;
struct Row {
    std::wstring key;CachedObjRef actor;
    CargoLiftPayload canonical{};
    bool have=false,applied=false,dirty=false;
    uint64_t at=0,sentAt=0;
    uint8_t seeds=0;
};
struct Request { CargoLiftPayload p; uint64_t at; uint8_t sender; uint64_t occupancy=0; };
std::atomic<net::Session*> g_session{nullptr};
std::array<Row,16> g_rows;
std::deque<Request> g_requests;
uint32_t g_lastRequest[net::kMaxPeers]{};
uint64_t g_occupancy[net::kMaxPeers]{};
uint64_t g_rateAt[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
uint32_t g_world=0,g_revision=0,g_sequence=0;
uint64_t g_epoch=0,g_nextScan=0,g_nextSend=0;
uint8_t g_seed=0;
bool g_ticking=false;
unsigned g_replaying=0; // stack guard; deliberately survives queue/session resets
uint64_t Now(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
net::Session* Connected(){auto* s=g_session.load();return s&&s->connected()?s:nullptr;}
bool Client(){auto* s=Connected();return s&&s->role()==net::Role::Client;}
uint32_t Next(uint32_t& seq){if(!++seq)++seq;return seq;}
bool After(uint32_t a,uint32_t b){return static_cast<int32_t>(a-b)>0;}
void Clear() {
    ++g_epoch;
    for(auto& occupancy:g_occupancy)++occupancy;
    g_requests.clear();std::fill(std::begin(g_lastRequest),std::end(g_lastRequest),0);
    std::fill(std::begin(g_rateAt),std::end(g_rateAt),0);
    std::fill(std::begin(g_rateCount),std::end(g_rateCount),0);
    g_seed=0;g_nextScan=g_nextSend=0;g_revision=g_sequence=0;
    g_rows={}; // native motion/physics keep their own lifetime; no borrowed state
}
void World(){const auto gen=ue_wrap::world_identity::Generation();if(gen!=g_world){Clear();g_world=gen;}}
bool Current(uint64_t epoch){return epoch==g_epoch&&g_world==ue_wrap::world_identity::Generation()&&Connected();}
bool KeyValid(const net::WireKey& key) {
    if(!key.len||key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<32||uint8_t(key.data[i])>126)return false;
    return true;
}
Row* Find(const std::wstring& key,bool create) {
    for(auto& r:g_rows)if(r.key==key)return &r;
    if(create)for(auto& r:g_rows)if(r.key.empty()){r.key=key;return &r;}
    return nullptr;
}
L::State Decode(const CargoLiftPayload& p){return {p.alpha,{p.anchorX,p.anchorY,p.anchorZ},{p.x,p.y,p.z},{p.pitch,p.yaw,p.roll},p.moving!=0,p.direction!=0};}
CargoLiftPayload Encode(const std::wstring& key,const L::State& s) {
    CargoLiftPayload p{};net::WireKeyFromString(key,p.key);p.op=s.moving?2:1;
    p.alpha=s.alpha;p.anchorX=s.anchor.X;p.anchorY=s.anchor.Y;p.anchorZ=s.anchor.Z;
    p.x=s.position.X;p.y=s.position.Y;p.z=s.position.Z;
    p.pitch=s.rotation.Pitch;p.yaw=s.rotation.Yaw;p.roll=s.rotation.Roll;
    p.moving=s.moving;p.direction=s.direction;return p;
}
bool RestChanged(const CargoLiftPayload& a,const CargoLiftPayload& b) {
    const float dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z;
    return a.direction!=b.direction || std::fabs(a.alpha-b.alpha)>.001f ||
           dx*dx+dy*dy+dz*dz>1.f;
}
struct NativeScope { NativeScope(){++g_replaying;} ~NativeScope(){--g_replaying;} };
bool Budget(uint8_t slot,uint64_t now) {
    if(now-g_rateAt[slot]>=1000){g_rateAt[slot]=now;g_rateCount[slot]=0;}
    if(g_rateCount[slot]>=4)return false;++g_rateCount[slot];return true;
}
sg::Verdict OnAction(const sg::Call& c) {
    if(g_replaying || !Connected() || !L::IsLift(c.object) ||
       (Client()&&!net_pump::IsInAnnouncedWorld(c.object)))return sg::Verdict::Run;
    static R::ParamOffset action{L"action"},component{L"lookAtComponent"},player{L"player"};
    const int ao=action.Of(c.function),co=component.Of(c.function),po=player.Of(c.function);
    if(!c.locals||ao<0||co<0||po<0)return sg::Verdict::Cancel;
    if(c.locals[ao]!=4)return sg::Verdict::Run;
    void* comp=nullptr;void* who=nullptr;std::memcpy(&comp,c.locals+co,sizeof(comp));std::memcpy(&who,c.locals+po,sizeof(who));
    const int button=L::ButtonIndex(c.object,comp);
    if(who!=players::Registry::Get().Local()||button<0)return sg::Verdict::Cancel;
    World();const auto key=L::Key(c.object);
    if(key.empty()||key.size()>31||g_requests.size()>=4)return sg::Verdict::Cancel;
    CargoLiftPayload p{};net::WireKeyFromString(key,p.key);p.button=uint8_t(button);p.sequence=Next(g_sequence);
    g_requests.push_back({p,Now(),0});
    return sg::Verdict::Cancel;
}

}
void Install(net::Session* session) {
    if(g_session.load()!=session){Clear();g_world=ue_wrap::world_identity::Generation();}
    g_session.store(session);World();
    sg::WatchClassName(L"cargoLift_C",L"actionOptionIndex",0x4C490001,&OnAction,nullptr);
}
void QueueConnect(int slot){if(slot>0&&slot<net::kMaxPeers){World();g_seed|=uint8_t(1u<<slot);g_nextScan=0;}}
void OnPeerLeft(int slot) {
    if(slot<=0||slot>=net::kMaxPeers)return;
    ++g_occupancy[slot];
    g_lastRequest[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;g_seed&=uint8_t(~(1u<<slot));
    for(auto& r:g_rows)r.seeds&=uint8_t(~(1u<<slot));
    std::erase_if(g_requests,[slot](const Request& r){return r.sender==slot;});
}
void Receive(const CargoLiftPayload& p,uint8_t sender) {
    auto* s=Connected();if(!s||!KeyValid(p.key)||!p.sequence)return;
    World();const uint64_t now=Now();
    if(s->role()==net::Role::Host) {
        if(p.op!=0||sender==0||sender>=net::kMaxPeers||p.button>2||
            (g_lastRequest[sender]&&!After(p.sequence,g_lastRequest[sender])))return;
        if(!Budget(sender,now)||g_requests.size()>=16)return;
        g_lastRequest[sender]=p.sequence;g_requests.push_back({p,now,sender,g_occupancy[sender]});
    } else {
        if(sender!=0||(p.op!=1&&p.op!=2)||p.moving>1||p.direction>1||p.button>2||
            (p.op==2)!=(p.moving!=0)||!L::Valid(Decode(p)))return;
        Row* row=Find(net::StringFromWireKey(p.key),true);if(!row)return;
        if(row->have) {
            if(p.sequence==row->canonical.sequence) {
                if(row->applied)return; // heartbeat must never press the button again
            } else if(!After(p.sequence,row->canonical.sequence))return;
            else row->applied=false;
        }
        row->canonical=p;row->have=true;row->at=now;
    }
}
void Tick() {
    auto* s=Connected();if(!s||g_ticking)return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    World();const uint64_t epoch=g_epoch,now=Now();sg::ResolvePendingNames();
    if(!Current(epoch))return;
    if(!sg::ClassNameWatchLive(L"cargoLift_C",L"actionOptionIndex",0x4C490001))return;
    if(now>=g_nextScan) {
        g_nextScan=now+500;std::vector<CachedObjRef> actors;L::Enumerate(actors);
        for(auto& ref:actors)if(void* a=ref.Get()) {
            // ClientWorldReady is never announced by the host. Applying this
            // client-only join guard there excluded every host lift (b2134/35).
            if(s->role()==net::Role::Client&&!net_pump::IsInAnnouncedWorld(a))continue;
            const auto key=L::Key(a);if(key.empty()||key.size()>31)continue;
            Row* row=Find(key,true);if(!row)continue;
            if(!row->actor.Is(a)) {
                row->actor=ref;row->applied=false;row->sentAt=0;
                if(s->role()==net::Role::Host)row->have=false;
            }
            row->seeds|=g_seed;
        }
        if(!actors.empty())g_seed=0;
    }
    // Pop before dispatch. A native callback can disconnect, travel or queue another input.
    const size_t count=g_requests.size();
    for(size_t i=0;i<count&&!g_requests.empty();++i) {
        Request req=g_requests.front();g_requests.pop_front();
        if(now-req.at>3000)continue;
        if(s->role()==net::Role::Client) {
            if(!s->SendReliable(net::ReliableKind::CargoLift,&req.p,sizeof(req.p)))g_requests.push_back(req);
            continue;
        }
        Row* row=Find(net::StringFromWireKey(req.p.key),false);void* a=row?row->actor.Get():nullptr;
        if(req.sender&&req.occupancy!=g_occupancy[req.sender])continue;
        if(!a){g_requests.push_back(req);continue;}
        CachedObjRef actor=row->actor;ue_wrap::FVector button{};
        const auto requestCurrent=[&]{return Current(epoch)&&actor.Is(a)&&(!req.sender||req.occupancy==g_occupancy[req.sender]);};
        void* pawn=players::Registry::Get().Local();
        if(req.sender) {
            if(!L::ButtonPosition(a,req.p.button,button)){if(!Current(epoch))return;g_requests.push_back(req);continue;}
            if(!requestCurrent())return;
            auto target=element::IntentTarget::ForClientIntent(*s,req.sender,300.f);
            const bool hasBody=target.HasBody();
            if(!requestCurrent())return;
            if(!hasBody){g_requests.push_back(req);continue;}
            const bool authorized=bool(target.AuthorizeSegment(button,button));
            if(!requestCurrent())return;
            if(!authorized)continue;
            auto* puppet=players::Registry::Get().Puppet(req.sender);
            pawn=puppet?puppet->GetActor():nullptr;
        }
        bool activated=false;
        { NativeScope native; if(pawn)activated=L::Activate(a,req.p.button,pawn); }
        if(!requestCurrent())return;
        if(activated){row->have=false;row->canonical.button=req.p.button;g_nextSend=0;}
    }

    if(s->role()==net::Role::Client) {
        for(auto& row:g_rows)if(row.have&&!row.applied)if(void* a=row.actor.Get()) {
            const auto revision=row.canonical.sequence;
            const auto target=Decode(row.canonical);
            const CachedObjRef identity=row.actor;
            const auto valid=[&]{return Current(epoch)&&identity.Is(a)&&row.actor.Is(a)&&row.canonical.sequence==revision;};
            bool ok=false;
            { NativeScope native;
              ok=L::Replay(a,target,row.canonical.button,players::Registry::Get().Local(),valid); }
            if(!Current(epoch))return;
            if(ok&&valid())row.applied=true;
        }
        return;
    }
    if(now<g_nextSend)return;g_nextSend=now+100;
    for(auto& row:g_rows)if(void* a=row.actor.Get()) {
        const CachedObjRef identity=row.actor;L::State state{};
        if(!L::Read(a,state)){if(!Current(epoch))return;continue;}
        if(!Current(epoch)||!identity.Is(a)||!row.actor.Is(a))return;
        auto p=Encode(row.key,state);p.button=row.canonical.button;
        // One revision per accepted start/stop (or settled position change), never
        // one per moving frame. Heartbeats update late joiners without replay loops.
        const bool changed=!row.have || p.moving!=row.canonical.moving ||
            (!p.moving&&RestChanged(p,row.canonical));
        if(changed) {
            p.sequence=Next(g_revision);row.canonical=p;row.have=true;row.dirty=true;
            UE_LOGI("cargo_lift: native %s '%ls' rev=%u dir=%u",p.moving?"start":"rest",row.key.c_str(),p.sequence,p.direction);
        } else p.sequence=row.canonical.sequence;
        if(row.dirty||now-row.sentAt>=2000||!row.sentAt) {
            if(s->SendReliable(net::ReliableKind::CargoLift,&p,sizeof(p))) {
                row.dirty=false;row.sentAt=now;
            }
        }
        for(int slot=1;slot<net::kMaxPeers;++slot)if(row.seeds&(1u<<slot))
            if(s->SendReliableToSlot(slot,net::ReliableKind::CargoLift,&p,sizeof(p)))row.seeds&=uint8_t(~(1u<<slot));
    }
}
void OnDisconnect(){g_session.store(nullptr);Clear();if(!g_session.load())g_world=0;}
}
