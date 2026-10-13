// Anchored hook retrieval: the host creates one ordinary recovered prop, then
// retires the deployed hook everywhere. No inventory grant runs on a puppet.
#include "coop/items/hook_retrieve.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/protocol.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "ue_wrap/actors/hook.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "hook_sync_detail.h"
#include <array>
#include <map>
#include <unordered_map>
#include <cstring>
#include <vector>

namespace coop::hook_retrieve {
namespace {
namespace H = ue_wrap::hook;
namespace R = ue_wrap::reflection;
namespace E = ue_wrap::engine;
namespace sg = ue_wrap::script_gate;
namespace D = coop::hook_sync::detail;
using Packet = coop::net::HookRetrievePayload;
constexpr int kTag = 0x48524554;
constexpr size_t kMaxHooks = 1024, kMaxRetired = 2048;
coop::net::Session* g_session = nullptr;
uint32_t g_world = 0, g_seq = 0;
uint64_t g_epoch = 0;
uint64_t g_tickAt = 0;
struct Hook {ue_wrap::CachedObjRef ref; std::wstring key; bool query = false;};
struct Retired {uint32_t mask = 0; bool busy = false;};
struct Request {Packet packet{}; uint64_t expires = 0, retry = 0;};
std::unordered_map<void*, Hook> g_hooks;
std::map<std::wstring, Retired> g_retired;
std::map<std::wstring, Request> g_pending;
std::array<uint64_t, coop::players::kMaxPeers> g_nextRequest{};

bool Host() {return g_session && g_session->role() == coop::net::Role::Host;}
bool OwnerPhase(void* actor) {
    if(H::ActiveHookOf(coop::players::Registry::Get().Local())==actor)return true;
    for(const auto& row:D::OwnedHooks())if(row.ref.Is(actor))return true;
    for(const auto& [key,row]:D::Mirrors())if(!row.anchored && row.ref.Is(actor))return true;
    return false;
}
bool AnchoredMirror(void* actor) {
    for(const auto& [key,row]:D::Mirrors())if(row.anchored && row.ref.Is(actor))return true;
    return false;
}
void Clear() {
    ++g_epoch;
    g_hooks.clear();g_retired.clear();g_pending.clear();g_nextRequest.fill(0);g_tickAt=0;
}
bool Current() {
    if (!g_session || !g_session->connected()) return false;
    const uint32_t world=ue_wrap::world_identity::Generation();
    if(world!=g_world){Clear();g_world=world;}
    return true;
}
bool KeyValid(const std::wstring& key) {
    if(key.empty() || key==L"None" || key.size()>96)return false;
    for(wchar_t c:key)if(c<0x21 || c>0x7e)return false;
    return true;
}
Packet Make(const std::wstring& key,uint8_t op,uint32_t seq=0) {
    Packet p{};p.seq=seq;p.op=op;p.keyLen=uint8_t(key.size());
    for(size_t i=0;i<key.size();++i)p.key[i]=char(key[i]);return p;
}
uint32_t PeerMask() {
    uint32_t mask=0;
    for(int s=1;s<coop::players::kMaxPeers;++s)
        if(g_session->IsSlotWorldReady(s))mask|=1u<<s;
    return mask;
}
void RemoveClientHook(const std::wstring& key) {
    std::vector<ue_wrap::CachedObjRef> actors;
    for(auto it=g_hooks.begin();it!=g_hooks.end();) {
        if(it->second.key!=key){++it;continue;}
        actors.push_back(it->second.ref);it=g_hooks.erase(it);
    }
    // No table iterator survives native destruction or a disconnect callback.
    const uint64_t epoch=g_epoch;
    for(const auto& ref:actors) {
        if(epoch!=g_epoch)return;
        if(void* actor=ref.Get())E::DestroyActor(actor);
    }
}
void Retire(const std::wstring& key) {
    auto [it,inserted]=g_retired.try_emplace(key);
    it->second.busy=false;
    if(inserted || Host())it->second.mask=Host()?PeerMask():0;
    g_pending.erase(key);
    if(!Host())RemoveClientHook(key);
}
sg::Verdict OnVerb(const sg::Call& c) {
    if(!Current() || !c.object || H::KindOf(c.object)==H::Kind::Count)return sg::Verdict::Run;
    ue_wrap::CachedObjRef live;live.Set(c.object);
    const uint64_t epoch=g_epoch;
    Note(c.object);
    if(epoch!=g_epoch || !live.Get())return sg::Verdict::Cancel;
    auto it=g_hooks.find(c.object);
    if(it==g_hooks.end() || !it->second.ref.Is(c.object)) {
        H::State state{};
        // A saved client anchor with an unresolved key is not a local inventory grant.
        if(!Host() && H::ReadState(c.object,state) && state.attachedA && state.attachedB)
            return sg::Verdict::Cancel;
        return sg::Verdict::Run;
    }
    const std::wstring key=it->second.key;
    if(g_retired.count(key))return sg::Verdict::Cancel;
    if(Host())return sg::Verdict::Run; // host's native pickup; death watch publishes retirement
    // Client physics/unhook callbacks may not mint a local duplicate either.
    if(!c.function || !R::NameEquals(R::NameOf(c.function),L"actionOptionIndex"))return sg::Verdict::Cancel;
    if(!c.locals)return sg::Verdict::Cancel;
    const int off=R::FindParamOffset(c.function,L"player"),size=R::FunctionFrameSize(c.function);
    if(off<0 || off>size || sizeof(void*)>size-off)return sg::Verdict::Cancel;
    void* player=nullptr;std::memcpy(&player,c.locals+off,sizeof(player));
    if(!coop::players::Registry::Get().IsLocal(player))return sg::Verdict::Cancel;
    if(!g_pending.count(key) && g_pending.size()<8) {
        if(++g_seq==0)++g_seq;
        g_pending.emplace(key,Request{Make(key,0,g_seq),D::NowMs()+8000,0});
    }
    return sg::Verdict::Cancel;
}
}

void Install(coop::net::Session* session) {
    g_session=session;
    sg::WatchName(L"actionOptionIndex",kTag,&OnVerb,nullptr);
    sg::WatchName(L"unhook_",kTag,&OnVerb,nullptr);
    sg::ResolvePendingNames();
}

void Note(void* actor,bool knownAnchor) {
    if(!Current() || !actor || H::KindOf(actor)==H::Kind::Count)return;
    if(!knownAnchor && OwnerPhase(actor))return;
    if(auto it=g_hooks.find(actor);it!=g_hooks.end() && it->second.ref.Is(actor))return;
    if(g_hooks.size()>=kMaxHooks)return;
    H::State state{};
    if(!H::ReadState(actor,state))return;
    ue_wrap::CachedObjRef live;live.Set(actor);
    const uint64_t epoch=g_epoch;
    ue_wrap::save_record::SaveRecord record;
    if(!H::CaptureRecord(actor,record) || epoch!=g_epoch || !live.Get() || !KeyValid(record.key))return;
    // Native loadData omits attached_a/b. Walls/landscape also have no save key,
    // so None is a valid endpoint. Exclude live owner-phase hooks above instead
    // of requiring two keyed props (which would miss wall-to-wall cables).
    const auto endpoint=[](const std::wstring& k){return k.empty() || k==L"None" || KeyValid(k);};
    const bool savedAnchor=!state.thrown && !state.playerHooked &&
        record.names.size()>1 && record.names[1].size()>=2 &&
        endpoint(record.names[1][0]) && endpoint(record.names[1][1]);
    if(!knownAnchor && !(state.attachedA && state.attachedB) && !savedAnchor)return;
    if(g_retired.count(record.key)) {if(!Host())E::DestroyActor(actor);return;}
    g_hooks[actor]=Hook{live,record.key,false};
}

void Receive(const Packet& p,uint8_t sender) {
    if(!Current() || sender>=coop::players::kMaxPeers || p.op>2 || p.reserved[0] || p.reserved[1] ||
       p.keyLen==0 || p.keyLen>sizeof(p.key))return;
    const uint64_t requestEpoch=g_epoch;
    std::wstring key;
    for(uint8_t i=0;i<p.keyLen;++i)key.push_back(static_cast<unsigned char>(p.key[i]));
    if(!KeyValid(key))return;
    if(!Host()) {
        if(sender!=0 || p.op==0)return;
        if(p.op==1) {if(g_retired.count(key) || g_retired.size()<kMaxRetired)Retire(key);}
        else if(auto it=g_pending.find(key);it!=g_pending.end() && it->second.packet.seq==p.seq)g_pending.erase(it);
        return;
    }
    if(sender==0 || p.op!=0 || p.seq==0 || !g_session->IsSlotWorldReady(sender))return;
    auto reply=[&](uint8_t op){if(requestEpoch!=g_epoch || !g_session || !Host())return;
        const Packet answer=Make(key,op,p.seq);
        g_session->SendReliableToSlot(sender,coop::net::ReliableKind::HookRetrieve,&answer,sizeof(answer));};
    if(auto it=g_retired.find(key);it!=g_retired.end()) {if(!it->second.busy)reply(1);return;}
    const auto now=D::NowMs();
    if(now<g_nextRequest[sender])return; // sender retries; do not consume a denied rate slot
    g_nextRequest[sender]=now+200;
    if(g_retired.size()>=kMaxRetired){reply(2);return;}
    Hook hook;
    for(const auto& [ptr,row]:g_hooks)if(row.key==key && row.ref.Get()){hook=row;break;}
    void* actor=hook.ref.Get();
    if(!actor)return; // scan/adoption may not have caught up yet; bounded client retry
    // Only anchored births/validated saved endpoints enter this table. Do not
    // demand attached_a/b again: the native save loader omits these flags.
    const auto auth=coop::element::IntentTarget::ForClientIntent(*g_session,sender,200.f);
    // Root location can still be at the original thrower. Either traceable end
    // is a legitimate native interaction point, even on a long cable.
    ue_wrap::FVector head{},tail{};
    if(!H::ReadAnchorEnds(actor,head,tail) ||
       (!auth.AuthorizeSegment(head,head) && !auth.AuthorizeSegment(tail,tail))){reply(2);return;}
    if(requestEpoch!=g_epoch)return;
    auto* puppet=coop::players::Registry::Get().Puppet(sender);
    void* body=puppet?puppet->GetActor():nullptr;
    if(!body || !hook.ref.Get()){reply(2);return;}
    const uint64_t epoch=g_epoch;
    g_retired[key]={0,true}; // reserve before any native spawn/destruction callback
    const bool ok=H::RecoverToWorld(actor,body);
    if(epoch!=g_epoch)return;
    if(!ok){g_retired.erase(key);reply(2);return;}
    Retire(key);reply(1);
}

void Tick() {
    if(!Current())return;
    const uint64_t now=D::NowMs();if(now<g_tickAt)return;g_tickAt=now+250;
    if(!Host()) {
        for(auto it=g_pending.begin();it!=g_pending.end();) {
            if(now>=it->second.expires){it=g_pending.erase(it);continue;}
            if(now>=it->second.retry) {
                g_session->SendReliable(coop::net::ReliableKind::HookRetrieve,&it->second.packet,sizeof(Packet));
                it->second.retry=now+500;
            }
            ++it;
        }
    }
    // Own refs across callbacks rather than iterators into the live table.
    std::vector<Hook> rows;rows.reserve(g_hooks.size());
    for(const auto& [ptr,row]:g_hooks)rows.push_back(row);
    const uint64_t epoch=g_epoch;
    for(const Hook& row:rows) {
        if(epoch!=g_epoch)return;
        void* actor=row.ref.Get();
        if(!actor) {
            if(auto it=g_hooks.find(row.ref.Raw());it!=g_hooks.end() && !it->second.ref.Get())g_hooks.erase(it);
            if(Host() && !g_retired.count(row.key) && g_retired.size()<kMaxRetired)Retire(row.key);
        } else if(!Host() && !row.query && sg::NameWatchLive(L"actionOptionIndex",kTag) &&
                  sg::NameWatchLive(L"unhook_",kTag)) {
            // Only our spawned mirrors were parked with collision disabled.
            // Save-loaded native hooks already have interactive endpoints; do
            // not modify their tick/collision settings across disconnect.
            const bool ok=!AnchoredMirror(actor) || H::EnableAnchorQueries(actor);
            if(epoch!=g_epoch)return;
            if(row.ref.Is(actor))
                if(auto it=g_hooks.find(actor);it!=g_hooks.end() && it->second.ref.Is(actor))it->second.query=ok;
        }
    }
    if(!Host())return;
    int budget=8;
    for(auto& [key,row]:g_retired) {
        if(row.busy)continue;
        for(int slot=1;slot<coop::players::kMaxPeers;++slot) {
            const uint32_t bit=1u<<slot;if(!(row.mask&bit))continue;
            if(!g_session->IsSlotWorldReady(slot))continue;
            const Packet p=Make(key,1);
            if(g_session->SendReliableToSlot(slot,coop::net::ReliableKind::HookRetrieve,&p,sizeof(p)))row.mask&=~bit;
            if(--budget==0)return;
        }
    }
}

void SendAllToSlot(int slot) {
    if(!Current() || !Host() || slot<=0 || slot>=coop::players::kMaxPeers)return;
    for(auto& [key,row]:g_retired)row.mask|=1u<<slot;
}
void OnPeerLeftSlot(int slot) {
    if(slot<0 || slot>=coop::players::kMaxPeers)return;
    g_nextRequest[slot]=0;
    for(auto& [key,row]:g_retired)row.mask&=~(1u<<slot);
}
void OnDisconnect() {Clear();g_session=nullptr;g_world=0;}
}
