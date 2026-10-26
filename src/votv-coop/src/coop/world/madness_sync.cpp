#include "coop/world/madness_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/props/active_drive.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/world/madness.h"
#include "ue_wrap/engine/world_identity.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace coop::madness_sync {
namespace {
namespace M=ue_wrap::madness;
using Packet=net::MadnessStatePayload;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0,g_event=0,g_pending=0,g_appliedEvent=0;
uint64_t g_epoch=0,g_pollAt=0,g_sentAt=0,g_receivedAt=0,g_lastNow=0,g_debtRevision=0;
float g_transit=0;
bool g_have=false,g_ticking=false;
ue_wrap::CachedObjRef g_master,g_masterClass;
Packet g_state{};M::View g_view;
struct Scope {
    net::Session* session=nullptr;net::Role role{};uint32_t serial=0,generation=0;void* world=nullptr;bool active=false;
    bool operator==(const Scope&)const=default;
};
Scope g_scope;
struct Recipient {uint16_t person=0;uint32_t connection=0;uint64_t revision=0;};
std::array<Recipient,net::kMaxPeers> g_recipient;
Scope Capture(net::Session* s){
    Scope scope;scope.session=s;scope.serial=net::session_serial::Current();
    scope.generation=ue_wrap::world_identity::Generation();scope.world=ue_wrap::world_identity::CurrentWorld();
    if(s){scope.role=s->role();scope.active=s->running()&&s->connected()&&scope.world;}return scope;
}
bool Current(){return g_scope.active&&g_scope==Capture(g_session);}
bool Refresh(){if(!(g_scope==Capture(g_session)))Install(g_session);return Current();}
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
bool Valid(const M::State& s){return s.kills>=0&&std::isfinite(s.elapsed)&&s.elapsed>=0&&s.elapsed<=86400&&
    (s.active||(!s.hud&&!s.playing&&!s.kills));}
float Age(float elapsed,uint64_t now,uint64_t sampled,bool playing,float transit=0){
    if(!playing)return elapsed;
    const double delta=now>=sampled?double(now-sampled)/1000.0:0;
    return float((std::min)(86400.0,double(elapsed)+delta+transit));
}
void Queue(int slot){
    if(slot<=0||slot>=net::kMaxPeers||!Current()||g_session->role()!=net::Role::Host)return;
    const auto person=roster_ledger::Get(slot).playerNo;const auto link=g_session->peerGenerationForSlot(slot);
    if(!person||!link||!g_session->IsSlotConnected(slot))return;
    g_recipient[slot]={person,link,++g_debtRevision};g_pending|=1u<<slot;
}
bool Belongs(int slot,const Recipient& r){return r.person&&r.person==roster_ledger::Get(slot).playerNo&&
    r.connection==g_session->peerGenerationForSlot(slot)&&g_session->IsSlotConnected(slot);}
bool ViewEmpty(const M::View& v){return !v.audio.Raw()&&!v.widget.Raw();}
}
void Install(net::Session* s){
    const auto scope=Capture(s);
    if(g_scope==scope&&g_session==s)return;
    const auto epoch=g_epoch;OnDisconnect();
    if(g_epoch!=epoch+1||g_session)return;
    g_session=s;g_scope=scope;g_world=scope.generation;
}
void Tick(){
    if(g_ticking||!Refresh())return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;
    const uint64_t now=active_drive::NowMs(),epoch=g_epoch;
    if(now<g_lastNow){g_pollAt=0;g_sentAt=g_receivedAt=now;}
    g_lastNow=now;
    if(g_session->role()==net::Role::Host){
        if(now>=g_pollAt){
            g_pollAt=now+100;
            void* actor=g_master.Get();
            if(actor&&(!g_masterClass.Get()||ue_wrap::reflection::ClassOf(actor)!=g_masterClass.Get()))actor=nullptr;
            if(!actor)actor=M::FindHost();
            const bool newEvent=actor&&!g_master.Is(actor);
            ue_wrap::CachedObjRef live,cls;live.Set(actor);if(actor)cls.Set(ue_wrap::reflection::ClassOf(actor));
            M::State state{};
            const bool readable=!actor||M::Read(actor,state);
            const bool sourceCurrent=!actor||(live.Get()&&cls.Get()&&ue_wrap::reflection::ClassOf(actor)==cls.Get());
            if(epoch!=g_epoch||!Current())return;
            if(readable&&sourceCurrent&&Valid(state)){
                if(newEvent)Next(g_event);
                g_master=live;g_masterClass=cls;
                if(!g_have||newEvent||state.active!=bool(g_state.active)||state.hud!=bool(g_state.hud)||
                   state.playing!=bool(g_state.playing)||state.kills!=g_state.kills||now-g_sentAt>=1000){
                    g_state={Next(g_sequence),g_event,state.kills,state.elapsed,uint8_t(state.active),uint8_t(state.hud),uint8_t(state.playing),0};
                    g_pending=0;g_recipient={};g_have=true;g_sentAt=now;
                    for(int slot=1;slot<net::kMaxPeers;++slot)Queue(slot);
                }
            }
        }
        if(!g_have)return;
        const Packet sampled=g_state;const auto pending=g_pending;
        for(int slot=1;slot<net::kMaxPeers;++slot)if(pending&(1u<<slot)){
            const auto recipient=g_recipient[slot];
            if(!Belongs(slot,recipient)){g_pending&=~(1u<<slot);continue;}
            if(!g_session->IsSlotWorldReady(slot)||!g_session->IsSlotReady(slot))continue;
            Packet outgoing=sampled;
            outgoing.elapsed=Age(sampled.elapsed,now,g_sentAt,sampled.active&&sampled.playing);
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::MadnessState,&outgoing,sizeof(outgoing));
            if(epoch!=g_epoch||!Current())return;
            if(sent&&g_state.sequence==sampled.sequence&&Belongs(slot,recipient)&&g_recipient[slot].revision==recipient.revision)g_pending&=~(1u<<slot);
            if(g_state.sequence!=sampled.sequence)return;
        }
        return;
    }
    if(!g_have||now<g_pollAt)return;
    g_pollAt=now+100;
    auto view=g_view;g_view={};const auto state=g_state;
    const auto valid=[&]{return epoch==g_epoch&&Current()&&g_state.sequence==state.sequence;};
    if(g_appliedEvent!=state.eventId){
        M::Clear(view);
        if(!valid()||!ViewEmpty(view)){
            if(epoch==g_epoch&&Current())g_view=view;else M::Clear(view);
            return;
        }
        // Event ownership precedes partial preparation, so HUD retries cannot restart live music.
        g_appliedEvent=state.eventId;
    }
    M::State desired{state.active!=0,state.hud!=0,state.playing!=0,state.kills,
        Age(state.elapsed,now,g_receivedAt,state.active&&state.playing,g_transit)};
    M::Apply(view,desired,valid);
    if(epoch!=g_epoch||!Current()){M::Clear(view);return;}
    g_view=view;
}
void Receive(const Packet& incoming,uint8_t sender){
    const auto p=incoming;
    if(!Refresh()||g_session->role()!=net::Role::Client||sender!=0||!p.sequence||p.active>1||p.hud>1||p.playing>1||p.reserved||
       !Valid({p.active!=0,p.hud!=0,p.playing!=0,p.kills,p.elapsed})||(p.active&&!p.eventId)||(g_have&&!After(p.sequence,g_state.sequence)))return;
    g_state=p;g_have=true;g_receivedAt=active_drive::NowMs();g_pollAt=0;
    const int rtt=g_session->rttMsForSlot(0);g_transit=rtt>=0&&rtt<=10000?float(rtt)/2000.f:0.f;
}
void QueueConnect(int slot){if(Refresh()&&g_session->role()==net::Role::Host){Queue(slot);g_pollAt=0;}}
void OnPeerLeft(int slot){if(slot>0&&slot<net::kMaxPeers){g_pending&=~(1u<<slot);g_recipient[slot]={};++g_debtRevision;}}
void OnDisconnect(){
    auto view=g_view;g_view={};++g_epoch;g_session=nullptr;g_scope={};g_world=0;
    g_sequence=g_event=g_pending=g_appliedEvent=0;g_pollAt=g_sentAt=g_receivedAt=g_lastNow=0;
    g_transit=0;g_have=false;g_master={};g_masterClass={};g_state={};g_recipient={};M::Clear(view);
}
}
