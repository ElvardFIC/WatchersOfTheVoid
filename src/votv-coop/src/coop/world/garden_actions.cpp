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

namespace coop::garden_actions {
namespace {
namespace P=ue_wrap::placed_save;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
namespace W=save_record_wire;
constexpr uint8_t kOp=9, kToggle=1, kDetach=2;
constexpr int kTag=0x47415244;
struct Intent { uint8_t kind=0; uint32_t seq=0; std::wstring key, endpoint; ue_wrap::CachedObjRef actor; };
struct Seen { uint32_t seq=0, playerNo=0; uint64_t at=0; };
net::Session* g_session=nullptr;
std::deque<Intent> g_pending;
std::array<Seen,players::kMaxPeers> g_seen{};
std::array<uint64_t,players::kMaxPeers> g_peerEpoch{};
uint32_t g_world=0, g_seq=0;
uint64_t g_epoch=0;
bool g_installed=false, g_busy=false;
int g_native=0;
uint64_t Now() { return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
bool Active() { return g_session && g_session->running() && g_session->connected() && ue_wrap::game_thread::IsGameThread(); }
bool Host() { return g_session && g_session->role()==net::Role::Host; }
void Reset() { ++g_epoch;g_world=ue_wrap::world_identity::Generation();g_pending.clear();g_seen={}; }
void World() { if(g_world!=ue_wrap::world_identity::Generation())Reset(); }
bool Key(const std::wstring& key) {
    if(key.empty()||key==L"None"||key.size()>128)return false;
    for(auto c:key)if(c<32||c>126)return false;
    return true;
}
struct Native { Native(){++g_native;} ~Native(){--g_native;} };
bool Enqueue(void* actor,uint8_t kind,const std::wstring& endpoint={}) {
    if(g_pending.size()>=32)return false;
    const auto key=P::ExistingKey(actor);
    if(!Key(key))return false;
    // Repeated overlap/temporary grab callbacks name the same detachment. A
    // deliberate faucet click remains a separate ordered toggle.
    if(kind==kDetach)for(const auto& p:g_pending)if(p.kind==kind&&p.actor.Is(actor)&&p.endpoint==endpoint)return true;
    Intent p; p.kind=kind;p.key=key;p.endpoint=endpoint;p.actor.Set(actor);p.seq=++g_seq;
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
}
void Install(net::Session* session) {
    if(g_session!=session){g_session=session;Reset();}
    if(!g_installed)g_installed=SG::WatchClassName(L"faucet_C",L"actionOptionIndex",kTag,&FaucetAction,&FaucetChanged)&&
        SG::WatchClassName(L"groundHose_C",L"detachSprinkler",kTag,&HoseDetach,nullptr);
}
void Tick() {
    if(g_busy||!Active())return;
    g_busy=true;struct Busy{~Busy(){g_busy=false;}}busy;
    World();if(Host())return;const auto epoch=g_epoch;
    for(int work=0;work<4&&!g_pending.empty();++work){
        const auto p=g_pending.front();
        if(!p.actor.Get()){g_pending.pop_front();continue;}
        std::vector<uint8_t> bytes;W::AppU8(bytes,kOp);W::AppU8(bytes,p.kind);W::AppU32(bytes,p.seq);
        W::AppWStr(bytes,p.key);W::AppWStr(bytes,p.endpoint);
        const bool sent=placed_save_sync::SendGardenAction(bytes);
        if(epoch!=g_epoch||!Active()||Host())return;
        if(!sent)break;
        g_pending.pop_front();
    }
}
bool Receive(const std::vector<uint8_t>& bytes,uint8_t slot) {
    if(bytes.empty()||bytes[0]!=kOp)return false;
    if(!Active()||!Host()||slot==0||slot>=players::kMaxPeers)return true;
    World();size_t offset=1;Intent p;
    if(!W::RdU8(bytes,offset,p.kind)||(p.kind!=kToggle&&p.kind!=kDetach)||!W::RdU32(bytes,offset,p.seq)||!p.seq||
        !W::RdWStr(bytes,offset,p.key)||!Key(p.key)||!W::RdWStr(bytes,offset,p.endpoint)||offset!=bytes.size()||
        (p.kind==kToggle?!p.endpoint.empty():!Key(p.endpoint)))return true;
    const auto epoch=g_epoch, peerEpoch=g_peerEpoch[slot];const auto world=g_world;
    const auto playerNo=uint32_t(roster_ledger::Get(slot).playerNo);if(!playerNo)return true;
    void* actor=ue_wrap::keyed_objects::Resolve(p.key.c_str());
    const auto kind=actor?P::Type(actor):P::Kind::None;
    ue_wrap::CachedObjRef ref;if(actor)ref.Set(actor);
    const auto current=[&]{return epoch==g_epoch&&world==ue_wrap::world_identity::Generation()&&peerEpoch==g_peerEpoch[slot]&&
        Active()&&Host()&&ref.Get()&&uint32_t(roster_ledger::Get(slot).playerNo)==playerNo;};
    if(!current()||kind!=(p.kind==kToggle?P::Kind::Faucet:P::Kind::Hose))return true;
    // Rejections publish the real state too, repairing the grabber's delayed
    // endpoint view without replaying a gameplay verb on its replica.
    placed_save_sync::MarkChanged(actor);
    const auto prior=g_seen[slot];const auto now=Now();
    if(prior.playerNo==playerNo&&int32_t(p.seq-prior.seq)<=0)return true;
    g_seen[slot]={p.seq,playerNo,now};
    if(!P::Ready(actor))return true;
    const auto target=element::IntentTarget::ForClientIntent(*g_session,slot,450.f);
    bool reachable=false;
    if(p.kind==kToggle)reachable=bool(target.Authorize(actor));
    else {
        ue_wrap::save_record::SaveRecord state;
        if(!P::Capture(actor,state)||!current()||state.names.empty()||state.names[0].size()!=2||state.names[0][1]!=p.endpoint||
            state.vectors.empty()||state.vectors[0].empty())return true;
        const auto& pnt=state.vectors[0].back();const ue_wrap::FVector endpoint{pnt[0],pnt[1],pnt[2]};
        reachable=bool(target.AuthorizeSegment(endpoint,endpoint));
    }
    if(!reachable||!current())return true;
    Native native;
    if(p.kind==kToggle)P::ToggleFaucet(actor);
    else P::DetachSprinkler(actor,p.endpoint);
    if(current())placed_save_sync::MarkChanged(actor);
    return true;
}
void OnPeerLeftSlot(int slot) { if(slot>=0&&slot<players::kMaxPeers){g_seen[slot]={};++g_peerEpoch[slot];} }
void OnDisconnect(){g_session=nullptr;Reset();}
}
