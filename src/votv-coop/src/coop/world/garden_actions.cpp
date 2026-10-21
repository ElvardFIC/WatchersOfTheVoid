#include "coop/world/garden_actions.h"
#include "coop/world/placed_save_sync.h"
#include "coop/items/save_record_wire.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/keyed_objects.h"
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <set>

namespace coop::garden_actions {
namespace {
namespace P=ue_wrap::placed_save;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
namespace W=save_record_wire;
constexpr uint8_t kOp=9, kToggle=1, kDetach=2;
constexpr int kTag=0x47415244;
struct Intent { uint8_t kind=0,slot=0; uint32_t seq=0,playerNo=0; std::wstring key, endpoint; ue_wrap::CachedObjRef actor; uint64_t expires=0,peerEpoch=0; };
struct Seen { uint32_t seq=0, playerNo=0; uint64_t at=0; unsigned count=0; };
net::Session* g_session=nullptr;
std::deque<Intent> g_pending;
std::deque<Intent> g_host;
std::array<Seen,players::kMaxPeers> g_seen{};
std::array<uint64_t,players::kMaxPeers> g_peerEpoch{};
uint32_t g_world=0, g_seq=0;
uint64_t g_epoch=0;
bool g_installed=false, g_busy=false,g_processing=false;
const std::deque<Intent>* g_activeHost=nullptr;
uint64_t g_activeHostEpoch=0;
int g_native=0;
uint64_t Now() { return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
bool Active() { return g_session && g_session->running() && g_session->connected() && ue_wrap::game_thread::IsGameThread(); }
bool Host() { return g_session && g_session->role()==net::Role::Host; }
void Reset() { ++g_epoch;g_world=ue_wrap::world_identity::Generation();g_pending.clear();g_host.clear();g_seen={}; }
void World() { if(g_world!=ue_wrap::world_identity::Generation())Reset(); }
bool Key(const std::wstring& key) {
    if(key.empty()||key==L"None"||key.size()>128)return false;
    for(auto c:key)if(c<32||c>126)return false;
    return true;
}
struct Native { Native(){++g_native;} ~Native(){--g_native;} };
bool Enqueue(void* actor,uint8_t kind,const std::wstring& endpoint={}) {
    if(g_pending.size()>=32)return false;
    const auto epoch=g_epoch;ue_wrap::CachedObjRef ref;ref.Set(actor);
    const auto slot=players::Registry::Get().LocalPeerId();if(slot>=players::kMaxPeers)return false;
    const auto person=roster_ledger::Get(slot).playerNo;if(!person)return false;
    const auto key=P::ExistingKey(actor);
    if(epoch!=g_epoch || !Active() || Host() || !ref.Get() || !Key(key) || roster_ledger::Get(slot).playerNo!=person)return false;
    // Repeated overlap/temporary grab callbacks name the same detachment. A
    // deliberate faucet click remains a separate ordered toggle.
    if(kind==kDetach)for(const auto& p:g_pending)if(p.kind==kind&&p.actor.Is(actor)&&p.endpoint==endpoint)return true;
    Intent p; p.kind=kind;p.key=key;p.endpoint=endpoint;p.actor=ref;p.seq=++g_seq;p.slot=slot;p.playerNo=person;p.expires=Now()+5000;
    if(!p.seq)p.seq=++g_seq;
    g_pending.push_back(std::move(p));return true;
}
SG::Verdict FaucetAction(const SG::Call& c) {
    if(!Active()||Host()||c.fromOurCode||g_native||P::Type(c.object)!=P::Kind::Faucet||!c.function||!c.locals)return SG::Verdict::Run;
    const int action=R::FindParamOffset(c.function,L"action"), player=R::FindParamOffset(c.function,L"player"), size=R::FunctionFrameSize(c.function);
    if(action<0||action>=size||c.locals[action]!=5||player<0||player>size||size-player<int(sizeof(void*)))return SG::Verdict::Run;
    void* source=nullptr;std::memcpy(&source,c.locals+player,sizeof(source));
    if(!source||source!=players::Registry::Get().Local())return SG::Verdict::Run;
    World();Enqueue(c.object,kToggle);return SG::Verdict::Cancel;
}
void FaucetChanged(const SG::Call& c) {
    if(Active()&&Host()&&P::Type(c.object)==P::Kind::Faucet)placed_save_sync::MarkChanged(c.object);
}
SG::Verdict HoseDetach(const SG::Call& c) {
    if(!Active()||c.fromOurCode||g_native||P::Type(c.object)!=P::Kind::Hose)return SG::Verdict::Run;
    World();const auto epoch=g_epoch;const auto world=g_world;
    ue_wrap::CachedObjRef ref;ref.Set(c.object);
    ue_wrap::save_record::SaveRecord state;
    if(!Host()&&!P::Ready(c.object))return SG::Verdict::Run; // the final route owns this local edit
    const bool captured=P::Capture(c.object,state);
    if(epoch!=g_epoch||world!=ue_wrap::world_identity::Generation()||!Active()||!ref.Get())return SG::Verdict::Cancel;
    if(!captured||state.names.empty()||state.names[0].size()!=2||!Key(state.names[0][1]))return SG::Verdict::Run;
    if(!Host()){Enqueue(c.object,kDetach,state.names[0][1]);return SG::Verdict::Cancel;}
    Native native;
    P::DetachSprinkler(c.object,state.names[0][1]);
    if(epoch==g_epoch&&world==ue_wrap::world_identity::Generation()&&Active()&&Host()&&ref.Get())placed_save_sync::MarkChanged(c.object);
    return SG::Verdict::Cancel;
}
void ProcessHost() {
    if(g_processing || !Active() || !Host())return;
    g_processing=true;struct Busy{~Busy(){g_processing=false;}}busy;
    const auto epoch=g_epoch,now=Now();const auto world=g_world;
    std::deque<Intent> batch,keep;batch.swap(g_host);std::set<std::wstring> blocked;unsigned work=8;
    struct Scope {const std::deque<Intent>* old=g_activeHost;uint64_t oldEpoch=g_activeHostEpoch;
        Scope(const std::deque<Intent>& batch){g_activeHost=&batch;g_activeHostEpoch=g_epoch;}
        ~Scope(){g_activeHost=old;g_activeHostEpoch=oldEpoch;}}scope(batch);
    const auto current=[&]{return epoch==g_epoch && world==ue_wrap::world_identity::Generation() && Active() && Host();};
    for(auto& stored:batch){auto p=stored;
        if(!current())return;
        const auto person=[&]{return current() && g_peerEpoch[p.slot]==p.peerEpoch &&
            roster_ledger::Get(p.slot).playerNo==p.playerNo && g_session->IsSlotWorldReady(p.slot);};
        if(!person() || now>p.expires)continue;
        if(blocked.count(p.key) || !work){keep.push_back(p);blocked.insert(p.key);continue;}
        void* actor=p.actor.Get();if(p.actor.Raw() && !actor)continue;
        if(!actor){actor=ue_wrap::keyed_objects::Resolve(p.key.c_str());if(!current())return;if(!person())continue;if(actor)p.actor.Set(actor);}
        stored.actor=p.actor;
        if(!actor){keep.push_back(p);blocked.insert(p.key);continue;}
        const auto live=[&]{return person() && p.actor.Get()==actor;};
        const auto kind=P::Type(actor);
        if(!live() || kind!=(p.kind==kToggle?P::Kind::Faucet:P::Kind::Hose))continue;
        const bool ready=P::Ready(actor);if(!current())return;if(!live())continue;
        if(!ready){keep.push_back(p);blocked.insert(p.key);continue;}
        const auto key=P::ExistingKey(actor);if(!current())return;if(!live() || key!=p.key)continue;
        const auto target=element::IntentTarget::ForClientIntent(*g_session,p.slot,450.f);bool reachable=false;
        if(p.kind==kToggle)reachable=bool(target.Authorize(actor));
        else {
            ue_wrap::save_record::SaveRecord state;const bool captured=P::Capture(actor,state);
            if(!current())return;if(!live())continue;
            if(!captured){keep.push_back(p);blocked.insert(p.key);continue;}
            if(state.names.empty() || state.names[0].size()!=2 || state.names[0][1]!=p.endpoint ||
               state.vectors.empty() || state.vectors[0].empty())continue;
            const auto& point=state.vectors[0].back();const ue_wrap::FVector endpoint{point[0],point[1],point[2]};
            reachable=bool(target.AuthorizeSegment(endpoint,endpoint));
        }
        if(!current())return;if(!live())continue;
        // Reserve terminal work before native dispatch. A partial native failure
        // cannot safely be repeated; the canonical state repairs the observer.
        --work;
        if(reachable){Native native;if(p.kind==kToggle)P::ToggleFaucet(actor);else P::DetachSprinkler(actor,p.endpoint);}
        if(!current())return;if(live())placed_save_sync::MarkChanged(actor);
    }
    if(current())g_host.insert(g_host.begin(),keep.begin(),keep.end());
}
}
void Install(net::Session* session) {
    if(g_session!=session){g_session=session;Reset();}
    if(!g_installed)g_installed=SG::WatchClassName(L"faucet_C",L"actionOptionIndex",kTag,&FaucetAction,&FaucetChanged)&&
        SG::WatchClassName(L"groundHose_C",L"detachSprinkler",kTag,&HoseDetach,nullptr);
}
void Tick() {
    if(g_busy||!Active())return;
    g_busy=true;struct Busy{~Busy(){g_busy=false;}}busy;
    World();if(Host()){ProcessHost();return;}const auto epoch=g_epoch;
    for(int work=0;work<4&&!g_pending.empty();++work){
        const auto p=g_pending.front();
        if(!p.actor.Get() || Now()>p.expires || p.slot!=players::Registry::Get().LocalPeerId() ||
           roster_ledger::Get(p.slot).playerNo!=p.playerNo){g_pending.pop_front();continue;}
        std::vector<uint8_t> bytes;W::AppU8(bytes,kOp);W::AppU8(bytes,p.kind);W::AppU32(bytes,p.seq);
        W::AppU32(bytes,p.playerNo);
        W::AppWStr(bytes,p.key);W::AppWStr(bytes,p.endpoint);
        const auto current=[&]{return epoch==g_epoch && g_world==ue_wrap::world_identity::Generation() && Active() && !Host() && p.actor.Get() &&
            p.slot==players::Registry::Get().LocalPeerId() && roster_ledger::Get(p.slot).playerNo==p.playerNo;};
        const bool sent=placed_save_sync::SendGardenAction(bytes,current);
        if(epoch!=g_epoch||!Active()||Host())return;
        if(!sent)break;
        if(!g_pending.empty() && g_pending.front().seq==p.seq)g_pending.pop_front();
    }
}
bool Receive(const std::vector<uint8_t>& bytes,uint8_t slot) {
    if(bytes.empty()||bytes[0]!=kOp)return false;
    if(!Active()||!Host()||slot==0||slot>=players::kMaxPeers)return true;
    World();size_t offset=1;Intent p;
    if(!W::RdU8(bytes,offset,p.kind)||(p.kind!=kToggle&&p.kind!=kDetach)||!W::RdU32(bytes,offset,p.seq)||!p.seq||
        !W::RdU32(bytes,offset,p.playerNo)||!p.playerNo||p.playerNo>65535||
        !W::RdWStr(bytes,offset,p.key)||!Key(p.key)||!W::RdWStr(bytes,offset,p.endpoint)||offset!=bytes.size()||
        (p.kind==kToggle?!p.endpoint.empty():!Key(p.endpoint)))return true;
    if(roster_ledger::Get(slot).playerNo!=p.playerNo || !g_session->IsSlotWorldReady(slot))return true;
    auto prior=g_seen[slot];const auto now=Now();
    if(prior.playerNo==p.playerNo && int32_t(p.seq-prior.seq)<=0)return true;
    if(prior.playerNo!=p.playerNo || now-prior.at>=1000)prior.at=now,prior.count=0;
    size_t count=0;for(const auto& row:g_host)if(row.slot==slot)++count;
    if(prior.count>=32 || count>=32 || g_host.size()>=128)return true;
    for(const auto& pending:g_host)if(pending.key==p.key && pending.actor.Raw()){p.actor=pending.actor;break;}
    if(g_activeHost && g_activeHostEpoch==g_epoch)for(const auto& active:*g_activeHost)if(active.key==p.key && active.actor.Raw()){p.actor=active.actor;break;}
    g_seen[slot]={p.seq,p.playerNo,prior.at,prior.count+1};
    p.slot=slot;p.peerEpoch=g_peerEpoch[slot];p.expires=now+5000;g_host.push_back(p);ProcessHost();
    return true;
}
void OnPeerLeftSlot(int slot) { if(slot>=0&&slot<players::kMaxPeers){g_seen[slot]={};++g_peerEpoch[slot];
    std::erase_if(g_host,[slot](const Intent& p){return p.slot==slot;});std::erase_if(g_pending,[slot](const Intent& p){return p.slot==slot;});} }
void OnDisconnect(){g_session=nullptr;Reset();}
}
