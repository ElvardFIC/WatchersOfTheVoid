// coop/player/item_activate.cpp -- flashlight sync (the item-activation lane). See
// coop/player/item_activate.h. Install resolves mainPlayer_C's flashlight UFunctions and registers
// POST observers (retried every pump tick); the observer is the sender, reading the flashlight
// state after the BP ran and sending an ItemActivate; ApplyToPuppet is the receiver, called from
// event_feed's drain with the puppet.

#include "coop/player/item_activate.h"

#include "coop/element/element.h"
#include "coop/player/flashlight_click_sound.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/roster_ledger.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/config/config.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/facts/function_row.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <bit>

namespace coop::item_activate {
namespace {

namespace P = ue_wrap::profile;
namespace R = ue_wrap::reflection;
namespace E = ue_wrap::engine;
namespace GT = ue_wrap::game_thread;

// The flashlight: `updateFlashlight` is BP-inlined into `Flashlight Update` and never dispatches through
// ProcessEvent, so a POST observer is registered on both; the outer name has a literal space and the
// FName lookup matches that exact string. The F input events (_13 press, _14 release; recook-fragile
// ordinals) are always dispatched as UFunctions; both are observed and the sender dedups on last-sent
// state. timerHoldFlashlight fires when F is held long enough to switch the beam mode (spread <->
// focused); its body mutates `flashlightMode` and light_R's cone and intensity, which a POST observer
// reads to send the cone shape.
UE_FACT_FN(kRowUpdateFlashlight, "item_activate", L"mainPlayer_C", L"updateFlashlight", Declared);
UE_FACT_FN(kRowFlashlightUpdate, "item_activate", L"mainPlayer_C", L"Flashlight Update", Declared);
UE_FACT_FN(kRowFlashlightInput13, "item_activate", L"mainPlayer_C",
           L"InpActEvt_flashlight_K2Node_InputActionEvent_13", Declared);
UE_FACT_FN(kRowFlashlightInput14, "item_activate", L"mainPlayer_C",
           L"InpActEvt_flashlight_K2Node_InputActionEvent_14", Declared);
UE_FACT_FN(kRowTimerHoldFlashlight, "item_activate", L"mainPlayer_C", L"timerHoldFlashlight", Declared);

bool g_installed = false;
uint64_t g_nextStatePoll = 0;
uint32_t g_pollWorld = 0;
ue_wrap::CachedObjRef g_pollPlayer;

// The candidate UFunctions of the flashlight chain, resolved once. updateFlashlight is
// BP-inlined into Flashlight Update and never reaches ProcessEvent, so every candidate gets an
// observer and whichever the BP dispatches wins; the dedupe below absorbs a double fire.
// timerHoldFlashlight is the hold-F mode change (spread to focused), whose observer reads the
// post-mutation mode and cone shape so the receiver mirrors it.
void* g_updateFlashlightFn = nullptr;
void* g_flashlightUpdateFn = nullptr;
void* g_flashlightInput13Fn = nullptr;
void* g_flashlightInput14Fn = nullptr;
void* g_timerHoldFlashlightFn = nullptr;

// The last-sent signature (state, mode, intensity, cone angles): the hold-F mode change keeps
// state on but changes the mode and cones, so a state-only dedupe would drop it. kNoSendYet
// means never sent. One atomic, so the load cannot see a torn flag-plus-signature (a split
// pair let DebugForceToggle on a worker thread update the flag before the signature and an
// observer dedupe a fresh packet).
inline constexpr uint64_t kNoSendYet = 0xFFFFFFFFFFFFFFFFULL;
std::atomic<uint64_t> g_lastSentSig{kNoSendYet};

// The latched "on" intensity, sampled from the local light_R the first time it reads above
// the off-state value; the receiver drives the puppet's light_R with it. VOTV's light_R uses
// ELightUnits::Unitless, where Intensity is a small multiplier (about 0.2 off, roughly 5-10
// on), not the stock 5000 lumens; the fallback is a safe Unitless value used until the latch
// sees a real on value. 0 = not sampled. Both peers run the same BP defaults, so the latch
// reaches the same value on each once its player toggles once.
std::atomic<float> g_latchedOnIntensity{0.f};
inline constexpr float kIntensityOnFallback = 5.f;  // Unitless safe default
inline constexpr float kIntensityOffDefault = 0.f;  // turn light fully off

// The hash of prop_equipment_flashlight_C, the packet's itemClassHash; the _a and _b variants
// both toggle the puppet's light_R and share it.
uint32_t g_flashlightClassHash = 0;

// Atomic: the observer's read races the setter on another thread.
std::atomic<coop::net::Session*> g_session{nullptr};

// Pending receiver-side applies per peer slot: the puppet is created on the peer's first
// PoseSnapshot, and a tight connect edge can race the reliable ItemActivate ahead of that
// unreliable packet, so the latest payload waits here until TickConnect finds the puppet in the
// registry. Latest wins. Game thread.
bool g_pendingApplyValid[coop::players::kMaxPeers] = {};
uint64_t g_pendingApplyRevision[coop::players::kMaxPeers] = {};
coop::net::ItemActivatePayload g_pendingApplyPayload[coop::players::kMaxPeers] = {};

// The host's per-client cache of the last item state applied for that peer, not cleared on
// apply: ItemActivate is edge-triggered, so a joiner would otherwise see a flashlight that was
// already on stay dark until its owner toggles. Replayed to a new client on its connect edge.
// Slot 0 is not cached; the host's own state goes through QueueConnectBroadcastForSlot. Host
// only, game thread.
bool g_peerActivateValid[coop::players::kMaxPeers] = {};
coop::net::ItemActivatePayload g_peerActivateCache[coop::players::kMaxPeers] = {};
// Echo suppression around the receiver's apply, so a UFunction invoked on the puppet cannot
// bounce back as a wire packet. Atomic, as the session pointer.
std::atomic<bool> g_echoSuppress{false};

struct SessionScope {
    coop::net::Session* session=nullptr;
    uint32_t serial=0,world=0;
    coop::net::Role role=coop::net::Role::Host;
    bool running=false,connected=false;
};
SessionScope g_scopeState;
uint64_t g_epoch=0,g_sendRevision=0;
bool g_publishing=false;
uint64_t g_inflightSig=kNoSendYet;
struct PeerBinding { bool known=false; uint16_t person=0; uint32_t generation=0; };
PeerBinding g_peerBinding[coop::players::kMaxPeers]{};

void RetireState() {
    ++g_epoch;++g_sendRevision;
    g_nextStatePoll=0;g_pollPlayer.Reset();g_publishing=false;g_inflightSig=kNoSendYet;
    g_lastSentSig.store(kNoSendYet);g_latchedOnIntensity.store(0.f);
    for(int slot=0;slot<coop::players::kMaxPeers;++slot)OnDisconnectForSlot(slot);
}

void Reconcile() {
    auto* s=g_session.load();
    const SessionScope now{s,coop::net::session_serial::Current(),ue_wrap::world_identity::Generation(),
        s?s->role():coop::net::Role::Host,s&&s->running(),s&&s->connected()};
    if(now.session!=g_scopeState.session || now.serial!=g_scopeState.serial || now.world!=g_scopeState.world ||
       now.role!=g_scopeState.role || now.running!=g_scopeState.running || now.connected!=g_scopeState.connected){
        RetireState();g_scopeState=now;
    }
}

bool Current(uint64_t epoch) {
    auto* s=g_session.load();
    return epoch==g_epoch && s && s==g_scopeState.session && s->running() && s->connected() &&
        s->role()==g_scopeState.role && coop::net::session_serial::Current()==g_scopeState.serial &&
        ue_wrap::world_identity::Generation()==g_scopeState.world;
}

void BindPeer(uint8_t slot) {
    auto* s=g_session.load();
    const PeerBinding now{true,coop::roster_ledger::Get(slot).playerNo,s?s->peerGenerationForSlot(slot):0};
    auto& old=g_peerBinding[slot];
    if(old.known && (old.person!=now.person || old.generation!=now.generation))OnDisconnectForSlot(slot);
    old=now;
}

bool PeerCurrent(uint8_t slot,const PeerBinding& peer,uint64_t epoch) {
    return Current(epoch) && g_peerBinding[slot].known &&
        peer.person==coop::roster_ledger::Get(slot).playerNo &&
        peer.generation==g_session.load()->peerGenerationForSlot(slot);
}

bool ProbeLogEnabled() {
    // Read once per process: the observer is hot.
    static const bool s_enabled = ::coop::config::ResolveFlag(::coop::config_registry::rows::flashlight_log);
    return s_enabled;
}

bool ValidLight(const coop::net::ItemActivatePayload& p) {
    return p.state <= 1 && std::isfinite(p.intensity) && p.intensity >= 0.f &&
        std::isfinite(p.outerConeAngle) && p.outerConeAngle > 0.f && p.outerConeAngle < 90.f &&
        std::isfinite(p.innerConeAngle) && p.innerConeAngle >= 0.f && p.innerConeAngle <= p.outerConeAngle;
}

// The local player's flashlight state and cone shape as a payload; direct memory reads on the
// calling thread (the observer on the game thread, DebugForceToggle inside its posted lambda).
// False if light_R is dead.
bool BuildPayloadFromLocal(void* mp, coop::net::ItemActivatePayload& out, coop::net::Session* session) {
    if (!mp || !session || mp != coop::players::Registry::Get().Local()) return false;
    const auto epoch=g_epoch;
    ue_wrap::CachedObjRef pawnRef, lightRef;
    pawnRef.Set(mp);
    const auto world = ue_wrap::world_identity::Generation();
    void* light_R = ue_wrap::engine::GetMainPlayerLightR(mp);
    if (!light_R) return false;
    lightRef.Set(light_R);
    const auto current = [&] {
        if(!Current(epoch) || session!=g_session.load() || world!=ue_wrap::world_identity::Generation() ||
            !pawnRef.Get() || !lightRef.Get() || mp!=coop::players::Registry::Get().Local())return false;
        const bool sameLight=E::GetMainPlayerLightR(mp)==light_R;
        return sameLight && Current(epoch) && pawnRef.Get() && lightRef.Get() && mp==coop::players::Registry::Get().Local();
    };

    ue_wrap::engine::MainPlayerFlashlightState fl{};
    if (!current() || !ue_wrap::engine::ReadMainPlayerFlashlightState(mp, fl) || !current()) return false;
    ue_wrap::engine::FlashlightSnapshot snap{};
    if (!ue_wrap::engine::ReadFlashlightSnapshot(light_R, snap) || !current()) return false;

    out = {};
    out.itemClassHash   = g_flashlightClassHash;
    // The sender's own Player element id; 0 before it is allocated (the boot window), and the
    // receiver then falls back to the sender slot. Ids are unique across the host and peer ranges.
    {
        const coop::element::ElementId selfEid =
            coop::players::Registry::Get().LocalPlayerElementId();
        out.senderElementId =
            (selfEid == coop::element::kInvalidId) ? 0u : selfEid;
    }
    out.state           = fl.flashlight && fl.hasFlashlight && !fl.crankFlashlight && snap.visible ? 1 : 0;
    out.flags           = 0;  // no actor key
    out.mode            = fl.mode;
    out.actorKeyHash    = 0;
    out.intensity       = snap.intensity;
    out.outerConeAngle  = snap.outerConeAngle;
    out.innerConeAngle  = snap.innerConeAngle;
    return ValidLight(out) && current();
}

// FNV1a over the mutable fields, to dedupe the observer fires of one logical change (a press
// fires the input event, Flashlight Update and updateFlashlight in a chain; hold-F fires
// timerHoldFlashlight and maybe some of the others).
uint64_t SignaturePayload(const coop::net::ItemActivatePayload& p) {
    uint64_t h = 0xcbf29ce484222325ULL;
    auto mix = [&](uint64_t v) { h ^= v; h *= 0x100000001b3ULL; };
    mix(p.state);
    mix(p.mode);
    mix(std::bit_cast<uint32_t>(p.intensity));
    mix(std::bit_cast<uint32_t>(p.outerConeAngle));
    mix(std::bit_cast<uint32_t>(p.innerConeAngle));
    return h;
}

bool PublishState(void* self) {
    Reconcile();
    const auto epoch=g_epoch;
    auto* session=g_session.load();
    if(!Current(epoch) || !self || self!=coop::players::Registry::Get().Local() || g_echoSuppress.load())return false;
    ue_wrap::CachedObjRef publisher;publisher.Set(self);
    const auto observedRevision=g_sendRevision;
    coop::net::ItemActivatePayload p{};
    if(!BuildPayloadFromLocal(self,p,session) || !Current(epoch) || observedRevision!=g_sendRevision)return false;
    const auto hash=SignaturePayload(p),sig=hash==kNoSendYet?kNoSendYet-1:hash;
    if(g_publishing && g_inflightSig==sig)return false;
    if(g_lastSentSig.load()==sig)return true;
    if(p.state && p.intensity>1.f)g_latchedOnIntensity=p.intensity;
    const auto revision=++g_sendRevision;
    const bool previousBusy=g_publishing;const auto previousSig=g_inflightSig;
    g_publishing=true;g_inflightSig=sig;
    struct Publishing {
        uint64_t epoch;bool previousBusy;uint64_t previousSig;
        ~Publishing(){if(epoch==g_epoch){g_publishing=previousBusy;g_inflightSig=previousSig;}}
    } publishing{epoch,previousBusy,previousSig};
    // A nested return to the previously admitted state still owes a correction
    // while a different state is in flight. Completion owns this exact revision.
    g_lastSentSig=kNoSendYet;
    const bool sent=session->SendReliable(coop::net::ReliableKind::ItemActivate,&p,sizeof(p));
    if(!Current(epoch) || revision!=g_sendRevision || !publisher.Is(coop::players::Registry::Get().Local()))return false;
    if(sent)g_lastSentSig=sig;
    if(ProbeLogEnabled())UE_LOGI("flashlight: publish state=%u mode=%u sent=%d",p.state,p.mode,sent?1:0);
    return sent;
}

void OnUpdateFlashlightPost(void* self,void*,void*) {
    PublishState(self);
}
}  // namespace

bool DebugForceToggle(void* mp) {
    if(!mp)return false;
    auto done=std::make_shared<std::atomic<int>>(0);
    auto result=std::make_shared<std::atomic<bool>>(false);
    auto work=[mp,done,result]{
        Reconcile();const auto epoch=g_epoch;
        ue_wrap::CachedObjRef actor;
        if(!Current(epoch) || mp!=coop::players::Registry::Get().Local()){done->store(1);return;}
        actor.Set(mp);
        const auto current=[&]{return Current(epoch) && actor.Get() && mp==coop::players::Registry::Get().Local();};
        ue_wrap::engine::MainPlayerFlashlightState state{};
        if(!E::ReadMainPlayerFlashlightState(mp,state) || !current()){done->store(1);return;}
        const bool enabled=!state.flashlight;
        if(!E::WriteMainPlayerFlashlight(mp,enabled) || !current()){done->store(1);return;}
        void* light=E::GetMainPlayerLightR(mp);
        if(current() && light){
            float intensity=enabled?g_latchedOnIntensity.load():0.f;
            if(enabled && intensity==0.f)intensity=kIntensityOnFallback;
            E::SetLightIntensity(light,intensity);
        }
        if(current()){PublishState(mp);result->store(enabled);}
        done->store(1);
    };
    if(GT::IsGameThread())work();else {GT::Post(work);while(!done->load())::Sleep(1);}
    return result->load();
}
uint32_t HashClassName(const wchar_t* s) {
    // FNV-1a 32-bit over the UTF-16 bytes; the same encoding on every peer.
    uint32_t h = 0x811c9dc5u;
    if (!s) return h;
    for (; *s; ++s) {
        const wchar_t w = *s;
        h ^= static_cast<uint8_t>(w & 0xFF);
        h *= 0x01000193u;
        h ^= static_cast<uint8_t>((w >> 8) & 0xFF);
        h *= 0x01000193u;
    }
    return h;
}

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    Reconcile();
    // The state sampler also covers native writes that do not dispatch an input event.
    // Its identity must exist even while the player class or observer slots are unavailable.
    g_flashlightClassHash = HashClassName(L"prop_equipment_flashlight_C");
    if (g_installed) return;

    void* playerCls = R::FindClass(P::name::MainPlayerClass);
    if (!playerCls) {
        // mainPlayer_C is not loaded yet (the menu); retry next pump tick.
        return;
    }

    // POST observers on every candidate of the chain (the two input events, Flashlight Update,
    // updateFlashlight, timerHoldFlashlight): the inner functions are BP-inlined into the input
    // events, which the engine's input system dispatches through ProcessEvent, and registering all
    // of them survives a recook that inlines differently. The signature dedupes a double fire.
    struct Candidate { const ue_wrap::facts::Function* row; void** outPtr; };
    Candidate cs[] = {
        { &kRowUpdateFlashlight,    &g_updateFlashlightFn    },
        { &kRowFlashlightUpdate,    &g_flashlightUpdateFn    },
        { &kRowFlashlightInput13,   &g_flashlightInput13Fn   },
        { &kRowFlashlightInput14,   &g_flashlightInput14Fn   },
        { &kRowTimerHoldFlashlight, &g_timerHoldFlashlightFn },
    };
    int registered = 0;
    for (auto& c : cs) {
        const ue_wrap::facts::Resolved fn = ue_wrap::facts::Resolve(*c.row);
        if (!fn) {
            UE_LOGW("flashlight: UFunction '%ls' not found on %ls", c.row->Name(), c.row->Class());
            continue;
        }
        if (!GT::RegisterPostObserver(fn, &OnUpdateFlashlightPost)) {
            UE_LOGW("flashlight: RegisterPostObserver(%ls) failed", c.row->Name());
            continue;
        }
        *c.outPtr = fn.Fn();
        ++registered;
    }
    if (registered == 0) {
        UE_LOGW("flashlight: no candidate observers registered -- aborting install");
        return;
    }

    g_flashlightClassHash = HashClassName(L"prop_equipment_flashlight_C");
    g_installed = true;
    UE_LOGI("flashlight: %d POST observer(s) installed (updateFlashlight=%p, "
            "'Flashlight Update'=%p, InpActEvt_13=%p, InpActEvt_14=%p, "
            "timerHoldFlashlight=%p, classHash=0x%08X, probe_log=%d)",
            registered, g_updateFlashlightFn, g_flashlightUpdateFn,
            g_flashlightInput13Fn, g_flashlightInput14Fn,
            g_timerHoldFlashlightFn, g_flashlightClassHash,
            ProbeLogEnabled() ? 1 : 0);
}

bool ApplyToPuppet(void* puppetActor, const coop::net::ItemActivatePayload& incoming,
                   uint8_t senderPeerSlot) {
    const auto payload=incoming;
    Reconcile();
    if (senderPeerSlot >= coop::players::kMaxPeers || !ValidLight(payload)) return false;
    if(!Current(g_epoch))return false;
    BindPeer(senderPeerSlot);
    const auto epoch=g_epoch;
    const auto peer=g_peerBinding[senderPeerSlot];
    const auto revision = g_pendingApplyRevision[senderPeerSlot];
    if (!puppetActor || !R::IsLive(puppetActor)) {
        UE_LOGW("flashlight: ApplyToPuppet called with invalid puppet=%p", puppetActor);
        return false;
    }
    if (payload.itemClassHash != g_flashlightClassHash) {
        UE_LOGW("flashlight: ApplyToPuppet classHash mismatch (got 0x%08X, "
                "expected 0x%08X = flashlight) -- dropping", payload.itemClassHash,
                g_flashlightClassHash);
        return false;
    }

    const bool newState = (payload.state != 0);

    const auto belongs=[&]{return !coop::players::Registry::Get().IsLocal(puppetActor) &&
        coop::players::Registry::Get().PeerIdOfActor(puppetActor)==senderPeerSlot;};
    if(!belongs() || !PeerCurrent(senderPeerSlot,peer,epoch))return false;
    ue_wrap::CachedObjRef pawnRef, lightRef;
    pawnRef.Set(puppetActor);

    void* light_R = ue_wrap::engine::GetMainPlayerLightR(puppetActor);
    if (!light_R) {
        UE_LOGW("flashlight: puppet light_R missing or dead -- light not toggled "
                "(puppet=%p)", puppetActor);
        return false;
    }
    lightRef.Set(light_R);
    const auto world = ue_wrap::world_identity::Generation();
    const auto current = [&] {
        if(!PeerCurrent(senderPeerSlot,peer,epoch) || world!=ue_wrap::world_identity::Generation() ||
            !pawnRef.Get() || !lightRef.Get() || !belongs() || revision!=g_pendingApplyRevision[senderPeerSlot])return false;
        const bool sameLight=ue_wrap::engine::GetMainPlayerLightR(puppetActor)==light_R;
        return sameLight && PeerCurrent(senderPeerSlot,peer,epoch) && pawnRef.Get() && lightRef.Get() &&
            revision==g_pendingApplyRevision[senderPeerSlot] && belongs();
    };

    // The puppet's existing light_R is driven through SetVisibility and SetIntensity only. The
    // puppet's flashlight bool is not written (it triggers the updateFlashlight ubergraph, an
    // "Invalid prop" emitter), no component is added (component-attached BP listeners), and
    // flashlightStateChanged is not broadcast (no subscribers). SetVisibility is the key: a
    // USceneComponent UFunction whose body sets the bit and calls the OnVisibilityChanged virtual,
    // which for a light calls MarkRenderStateDirty (native, so no BP observer fires) and schedules
    // the render-state recreation that builds the scene proxy; a direct bit write does none of
    // that and produced no visible change.
    const bool previousSuppress = g_echoSuppress.exchange(true, std::memory_order_acq_rel);
    struct EchoGuard { bool previous; ~EchoGuard() { g_echoSuppress.store(previous, std::memory_order_release); } } echo{previousSuppress};
    bool applied = true;
    if(!current())return false;

    // The four dispatches go through the cached engine wrappers.

    // 1) SetVisibility: marks the render state dirty on the first transition, which creates the
    // proxy on the orphan puppet.
    if (!ue_wrap::engine::SetSceneComponentVisibility(light_R, newState, false)) {
        applied = false;
        UE_LOGW("flashlight: SetSceneComponentVisibility failed (UFunction unresolved or light_R dead)");
    }
    if (!current()) return false;

    // 2) SetIntensity: the sender's exact brightness; off is 0.
    const float targetIntensity = newState ? payload.intensity : 0.f;
    if (!ue_wrap::engine::SetLightIntensity(light_R, targetIntensity)) {
        applied = false;
        UE_LOGW("flashlight: SetLightIntensity failed (UFunction unresolved or light_R dead)");
    }
    if (!current()) return false;

    // 3) The cone angles (spread versus focused); both mark the render state dirty. Only when on: a
    // cone on an off light is invisible.
    if (newState) {
        if (payload.outerConeAngle > 0.f) {
            applied = ue_wrap::engine::SetSpotLightOuterConeAngle(light_R, payload.outerConeAngle) && applied;
            if (!current()) return false;
        }
        if (payload.innerConeAngle >= 0.f) {
            applied = ue_wrap::engine::SetSpotLightInnerConeAngle(light_R, payload.innerConeAngle) && applied;
        }
    }

    if (!applied || !current()) return false;
    UE_LOGI("flashlight: applied to puppet=%p state=%d Intensity=%.2f outerCone=%.1f innerCone=%.1f mode=%u "
            "(senderPeerSlot=%u senderElementId=0x%08x)",
            puppetActor, newState ? 1 : 0, targetIntensity,
            payload.outerConeAngle, payload.innerConeAngle, payload.mode,
            static_cast<unsigned>(senderPeerSlot), payload.senderElementId);

    // The positional click at the puppet (coop/flashlight_click_sound), on a state change only; a
    // mode-change packet does not click.
    coop::flashlight_click_sound::PlayIfStateChanged(
        puppetActor, senderPeerSlot, newState);
    return true;
}

void ApplyToPuppetOrDefer(uint8_t senderPeerSlot, void* puppetActor,
                          const coop::net::ItemActivatePayload& incoming) {
    const auto p=incoming;
    Reconcile();
    if (!ValidLight(p) || !g_flashlightClassHash || p.itemClassHash != g_flashlightClassHash) return;
    if (senderPeerSlot >= coop::players::kMaxPeers) {
        UE_LOGW("flashlight: ApplyToPuppetOrDefer senderPeerSlot=%u out of range -- dropping",
                static_cast<unsigned>(senderPeerSlot));
        return;
    }
    if(!Current(g_epoch))return;
    BindPeer(senderPeerSlot);
    // The host remembers each client's latest item state, applied or deferred, for a later joiner;
    // the host's own state goes through QueueConnectBroadcastForSlot.
    if (senderPeerSlot >= 1) {
        if (auto* s = g_session.load(std::memory_order_acquire)) {
            if (s->role() == coop::net::Role::Host) {
                g_peerActivateCache[senderPeerSlot] = p;
                g_peerActivateValid[senderPeerSlot] = true;
            }
        }
    }
    // A live pawn can precede its light component and reflected setters. Keep the
    // latest state until the complete apply succeeds, rather than dropping it at birth.
    g_pendingApplyPayload[senderPeerSlot] = p;
    g_pendingApplyValid[senderPeerSlot] = true;
    const auto revision = ++g_pendingApplyRevision[senderPeerSlot];
    if (puppetActor && R::IsLive(puppetActor) && ApplyToPuppet(puppetActor, p, senderPeerSlot)) {
        if (revision == g_pendingApplyRevision[senderPeerSlot]) g_pendingApplyValid[senderPeerSlot] = false;
        return;
    }
    UE_LOGI("flashlight: ApplyToPuppetOrDefer puppet not ready for peerSlot=%u "
            "(senderElementId=0x%08x) -- stashing (state=%d Intensity=%.2f)",
            static_cast<unsigned>(senderPeerSlot), p.senderElementId,
            p.state, p.intensity);
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    Reconcile();
    // A targeted seed cannot satisfy the regular broadcast. In particular a refused
    // seed must leave a fresh publication owed when the pawn or transport becomes ready.
    g_lastSentSig.store(kNoSendYet, std::memory_order_release);
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) {
        UE_LOGW("flashlight: QueueConnectBroadcastForSlot called with no session");
        return;
    }
    if (peerSlot < 0 || peerSlot >= static_cast<int>(coop::players::kMaxPeers)) {
        UE_LOGW("flashlight: QueueConnectBroadcastForSlot peerSlot=%d out of range", peerSlot);
        return;
    }
    void* mp = coop::players::Registry::Get().Local();
    if (!mp) {
        // No local player yet (the menu): nothing to broadcast; the player spawns in the loading
        // splash before the session connects, so this is rare.
        UE_LOGI("flashlight: QueueConnectBroadcastForSlot(slot=%d) no local mp yet -- no broadcast",
                peerSlot);
        return;
    }
    coop::net::ItemActivatePayload p{};
    if (!BuildPayloadFromLocal(mp, p, s)) {
        UE_LOGW("flashlight: QueueConnectBroadcastForSlot(slot=%d) BuildPayloadFromLocal failed",
                peerSlot);
        return;
    }
    // Off is not sent: the puppet spawns off, and the observer ships a later toggle.
    if (p.state == 0) {
        UE_LOGI("flashlight: QueueConnectBroadcastForSlot(slot=%d) local state is OFF -- "
                "skipping (puppet default is OFF; no replay needed)", peerSlot);
        return;
    }
    // To the one slot: the connected peers got the state on the toggle.
    s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::ItemActivate, &p, sizeof(p));
    UE_LOGI("flashlight: connect-broadcast slot=%d sent state=%d Intensity=%.2f "
            "outerCone=%.1f mode=%u senderElementId=0x%08x",
            peerSlot, p.state, p.intensity, p.outerConeAngle, p.mode, p.senderElementId);
}

void ReplayPeerStatesToSlot(int newSlot) {
    Reconcile();
    // Every other client's cached state, replayed to a new client so it converges; ItemActivate is
    // edge-triggered, and the joiner cannot learn these otherwise. Host only.
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;
    if (newSlot < 1 || newSlot >= static_cast<int>(coop::players::kMaxPeers)) return;
    for (int peer = 1; peer < static_cast<int>(coop::players::kMaxPeers); ++peer) {
        if (peer == newSlot) continue;
        BindPeer(static_cast<uint8_t>(peer));
        if (!g_peerActivateValid[peer]) continue;
        const coop::net::ItemActivatePayload& p = g_peerActivateCache[peer];
        // Off is skipped: the joiner's puppet for that peer spawns off.
        if (p.state == 0) continue;
        // The sender slot is the origin peer, so the joiner routes the action to that peer's puppet
        // and the eid-range trust check sees a peer-range id from a peer slot.
        s->SendReliableToSlot(newSlot, coop::net::ReliableKind::ItemActivate,
                              &p, sizeof(p), static_cast<uint8_t>(peer));
        UE_LOGI("flashlight: T2-4 replayed peer %d state=%d (Intensity=%.2f "
                "senderElementId=0x%08x) to late joiner slot %d",
                peer, p.state, p.intensity, p.senderElementId, newSlot);
    }
}

void TickConnect() {
    Reconcile();
    // Battery exhaustion clears flashlight/light_R directly inside the native Tick,
    // bypassing the input UFunctions observed above. Four small reads per second;
    // the existing signature sends only changes and retries a refused send.
    auto* session = g_session.load(std::memory_order_acquire);
    const uint64_t now = GetTickCount64();
    const uint32_t world = ue_wrap::world_identity::Generation();
    void* local = coop::players::Registry::Get().Local();
    if (world != g_pollWorld || !g_pollPlayer.Is(local)) {
        g_pollPlayer.Set(local);
        g_pollWorld = world; g_nextStatePoll = 0;
        g_lastSentSig.store(kNoSendYet, std::memory_order_release);
        g_latchedOnIntensity.store(0.f, std::memory_order_release);
    }
    if (session && session->connected() && g_flashlightClassHash && now >= g_nextStatePoll) {
        g_nextStatePoll = now + 250;
        OnUpdateFlashlightPost(coop::players::Registry::Get().Local(), nullptr, nullptr);
    }

    // The pending applies drain once the registry has a live puppet for the slot.
    for (uint8_t peer = 0; peer < coop::players::kMaxPeers; ++peer) {
        BindPeer(peer);
        if (!g_pendingApplyValid[peer]) continue;
        auto* rp = coop::players::Registry::Get().Puppet(peer);
        if (!rp || !rp->valid()) continue;
        void* puppet = rp->GetActor();
        if (!puppet || !R::IsLive(puppet)) continue;
        coop::net::ItemActivatePayload p = g_pendingApplyPayload[peer];
        const auto revision = g_pendingApplyRevision[peer];
        UE_LOGI("flashlight: draining deferred apply for peerSlot=%u "
                "(senderElementId=0x%08x state=%d Intensity=%.2f)",
                static_cast<unsigned>(peer), p.senderElementId,
                p.state, p.intensity);
        if (ApplyToPuppet(puppet, p, peer) && revision == g_pendingApplyRevision[peer])
            g_pendingApplyValid[peer] = false;
    }
}

void OnDisconnect() {
    RetireState();g_scopeState={};
    g_session.store(nullptr,std::memory_order_release);
    g_pollWorld=0;
}
void OnDisconnectForSlot(int peerSlot) {
    if (peerSlot < 0 || peerSlot >= static_cast<int>(coop::players::kMaxPeers)) return;
    g_peerBinding[peerSlot]={};
    // A leaving peer's cached state goes, so a peer reusing the slot does not inherit its
    // flashlight.
    g_peerActivateValid[peerSlot] = false;
    ++g_pendingApplyRevision[peerSlot];
    g_peerActivateCache[peerSlot] = {};
    if (!g_pendingApplyValid[peerSlot]) return;
    UE_LOGI("flashlight: peer slot %d disconnected -- clearing pending apply (state=%d intensity=%.2f)",
            peerSlot, g_pendingApplyPayload[peerSlot].state,
            g_pendingApplyPayload[peerSlot].intensity);
    g_pendingApplyValid[peerSlot] = false;
    g_pendingApplyPayload[peerSlot] = {};
}

}  // namespace coop::item_activate
