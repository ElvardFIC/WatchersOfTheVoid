// coop/world/explosion_request_client.cpp -- the client's half of the explosion request lane: see
// coop/world/explosion_request.h. A client's launcher fire bodies are watched to tell a rocket or sticky bomb born
// inside one (the player's shot) from one the world made; each such shot is sent when its body ends, and the
// dissolved birth of an explosion its own player caused is sent as a request that names it. Game thread.

#include "coop/world/explosion_request.h"

#include "explosion_request_internal.h"  // co-located private header (src tree, not include/)

#include "coop/net/net_clock.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/world/explosion_recipe.h"
#include "coop/world/spawn_authority.h"

#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/explosion_source.h"

#include <cstdint>

namespace coop::explosion_request {
namespace {

namespace RC = coop::explosion_recipe;
namespace XS = ue_wrap::explosion_source;
namespace SG = ue_wrap::script_gate;
namespace R  = ue_wrap::reflection;
namespace E  = ue_wrap::engine;
namespace OI = ue_wrap::object_index;
using ue_wrap::FVector;

UE_FACT_FN(kRowFireRpg,    "explosion_request", L"prop_funGun_rpg_C",            L"fireBulletPrimary", Declared);
UE_FACT_FN(kRowFireSticky, "explosion_request", L"prop_funGun_stickyLauncher_C", L"fireBulletPrimary", Declared);
UE_FACT_FN(kRowFireWitch,  "explosion_request", L"car1_witch_C", L"InpActEvt_LeftMouseButton_K2Node_InputKeyEvent_0", Declared);

constexpr int kTagFire = 0x45584652;  // 'EXFR'

// The launchers are RC::Launcher's values 1..3; their arrays are indexed by one less.
constexpr int kLaunchers = 3;
const ue_wrap::facts::Function* const kFireRows[kLaunchers] = {&kRowFireRpg, &kRowFireSticky, &kRowFireWitch};

// The install runs off the pump every tick, so a watch that is refused is retried on a one-second throttle, at
// most 30 tries, then said once; never a per-tick retry.
constexpr uint64_t kResolveRetryMs  = 1000;
constexpr int      kMaxResolveTries = 30;

struct FireWatch {
    bool registered = false;
    bool live       = false;
    bool gaveUp     = false;
    int  tries      = 0;
};
FireWatch g_watch[kLaunchers];
uint64_t  g_nextTryMs = 0;
bool      g_listenerTried = false;

// Each launcher's fire body, as its watch's PRE saw it.
ue_wrap::CachedObjRef g_body[kLaunchers];

// A rocket or sticky bomb born inside a launcher's fire body, kept by its object index: an actor still inside its
// own spawn may not yet read as alive.
struct Pending {
    void*   actor;
    int32_t index;
    int     launcher;
};
constexpr size_t kPendingCap = 16;
Pending g_pending[kPendingCap];
size_t  g_pendingCount = 0;

// A shot sent, until the blast that names it: the projectile by pointer, index and slot serial.
struct SentShot {
    void*   actor;
    int32_t index;
    int32_t serial;
    uint32_t shotId;
    uint8_t projClass;
};
constexpr size_t kShotMapCap = 64;
SentShot g_shots[kShotMapCap];
size_t   g_shotCount = 0;

uint32_t g_nextShotId = 1;

bool g_deadLogged   = false;
bool g_sendLogged   = false;
bool g_unreadLogged = false;

void EraseShot(size_t i) {
    for (size_t j = i + 1; j < g_shotCount; ++j) g_shots[j - 1] = g_shots[j];
    --g_shotCount;
}

// The object index announced a death: the object may be freed, so only its pointer and index are compared.
void OnDeleted(void*, void* obj, void*, int32_t idx) {
    for (size_t i = 0; i < g_shotCount; ++i) {
        if (g_shots[i].actor == obj && g_shots[i].index == idx) {
            EraseShot(i);
            return;
        }
    }
}

void AddShot(const SentShot& s) {
    if (g_shotCount == kShotMapCap) EraseShot(0);
    g_shots[g_shotCount++] = s;
}

// The entry of `actor`, whose slot still holds the serial it was sent under; -1 for none.
int FindShot(void* actor) {
    for (size_t i = 0; i < g_shotCount; ++i)
        if (g_shots[i].actor == actor && R::SlotSerial(g_shots[i].index) == g_shots[i].serial)
            return static_cast<int>(i);
    return -1;
}

// A message that could not leave: said once a session, never retried.
void NoteSendFailed(const char* what) {
    if (g_sendLogged) return;
    g_sendLogged = true;
    UE_LOGW("[EXPLOSION] %s was not sent: the session took no reliable message", what);
}

void NoteUnread(const char* what) {
    if (g_unreadLogged) return;
    g_unreadLogged = true;
    UE_LOGW("[EXPLOSION] a message was not sent: %s did not read", what);
}

const char* ProjName(uint8_t projClass) {
    return projClass == static_cast<uint8_t>(RC::ProjClass::Rocket) ? "rocket" : "sticky";
}

// The launcher's shot, sent once its body has finished the projectile's spawn.
void SendShot(coop::net::Session& s, const Pending& e) {
    if (!R::IsLiveByIndex(e.actor, e.index)) {
        ++detail::MutCounters().pendingDead;
        if (!g_deadLogged) {
            g_deadLogged = true;
            UE_LOGW("[EXPLOSION] shot dropped: its projectile died inside the fire body");
        }
        return;
    }
    FVector loc{};
    if (!E::TryGetActorLocation(e.actor, loc)) return NoteUnread("the projectile's location");
    const XS::Kind kind = XS::KindOfClass(R::ClassOf(e.actor));
    const uint8_t projClass = static_cast<uint8_t>(kind == XS::Kind::Rocket ? RC::ProjClass::Rocket
                                                                            : RC::ProjClass::Sticky);
    coop::net::ProjectileFiredPayload p{};
    p.projClass = projClass;
    p.launcher  = static_cast<uint8_t>(e.launcher);
    p.shotId    = g_nextShotId;
    p.origin[0] = loc.X;
    p.origin[1] = loc.Y;
    p.origin[2] = loc.Z;
    if (!s.SendReliable(coop::net::ReliableKind::ProjectileFired, &p, sizeof(p))) return NoteSendFailed("a shot");
    ++g_nextShotId;
    AddShot(SentShot{e.actor, e.index, R::AllocateSlotSerial(e.index), p.shotId, projClass});
    ++detail::MutCounters().shotsSent;
    UE_LOGI("[EXPLOSION] shot sent id=%u class=%s launcher=%d", p.shotId, ProjName(projClass), e.launcher);
}

// Launcher `l`'s fire body has ended: its pending births are sent, or counted dead, and dropped.
void FlushLauncher(int l) {
    coop::net::Session* s = detail::SessionPtr();
    size_t kept = 0;
    for (size_t i = 0; i < g_pendingCount; ++i) {
        const Pending e = g_pending[i];
        if (e.launcher != l) {
            g_pending[kept++] = e;
        } else if (s) {
            SendShot(*s, e);
        }
    }
    g_pendingCount = kept;
}

template <int L>
SG::Verdict OnFirePre(const SG::Call& call) {
    if (!g_body[L - 1].Is(call.function)) g_body[L - 1].Set(call.function);
    // A body that ended without its POST left real shots pending: they go now, before this shot's birth.
    if (coop::spawn_authority::IsActiveClientSession()) FlushLauncher(L);
    return SG::Verdict::Run;
}

template <int L>
void OnFirePost(const SG::Call&) {
    if (!coop::spawn_authority::IsActiveClientSession()) return;
    FlushLauncher(L);
}

constexpr SG::PreFn  kFirePre[kLaunchers]  = {&OnFirePre<1>, &OnFirePre<2>, &OnFirePre<3>};
constexpr SG::PostFn kFirePost[kLaunchers] = {&OnFirePost<1>, &OnFirePost<2>, &OnFirePost<3>};

bool AllSettled() {
    for (const FireWatch& w : g_watch)
        if (!w.live && !w.gaveUp) return false;
    return true;
}

void Send(coop::net::Session& s, const coop::net::ExplosionRequestPayload& p) {
    if (!s.SendReliable(coop::net::ReliableKind::ExplosionRequest, &p, sizeof(p))) return NoteSendFailed("a request");
    ++detail::MutCounters().requestsSent;
    UE_LOGI("[EXPLOSION] request sent kind=%u class=%u shot=%u", static_cast<unsigned>(p.kind),
            static_cast<unsigned>(p.srcClass), p.shotId);
}

}  // namespace

namespace detail {

void InstallClient() {
    if (!g_listenerTried) {
        g_listenerTried = true;
        if (!OI::AddListener(OI::Listener{nullptr, nullptr, &OnDeleted}))
            UE_LOGE("[EXPLOSION] the object index took no death listener; a sent shot leaves the map only when "
                    "its blast or the cap takes it");
    }
    if (AllSettled()) return;
    const uint64_t now = coop::net::NowMs();
    if (now < g_nextTryMs) return;
    g_nextTryMs = now + kResolveRetryMs;
    bool anyRegistered = false;
    for (int i = 0; i < kLaunchers; ++i) {
        FireWatch& w = g_watch[i];
        if (w.live || w.gaveUp) continue;
        if (!w.registered) {
            if (SG::WatchName(*kFireRows[i], kTagFire, kFirePre[i], kFirePost[i])) {
                w.registered = true;
            } else if (++w.tries >= kMaxResolveTries) {
                w.gaveUp = true;
                UE_LOGE("[EXPLOSION] fire watch %ls gave up: the script gate refused it %d times",
                        RC::LauncherClassName(static_cast<uint8_t>(i + 1)), kMaxResolveTries);
            }
        }
        anyRegistered = anyRegistered || w.registered;
    }
    // WatchName returns at registration; the watch intercepts only once its name resolved.
    if (!anyRegistered) return;
    SG::ResolvePendingNames();
    for (int i = 0; i < kLaunchers; ++i) {
        FireWatch& w = g_watch[i];
        if (!w.registered || w.live || w.gaveUp) continue;
        const wchar_t* cls = RC::LauncherClassName(static_cast<uint8_t>(i + 1));
        if (SG::NameWatchLive(*kFireRows[i], kTagFire)) {
            w.live = true;
            UE_LOGI("[EXPLOSION] fire watch %ls live", cls);
        } else if (SG::NameWatchSettled(*kFireRows[i], kTagFire)) {
            w.gaveUp = true;
            UE_LOGE("[EXPLOSION] fire watch %ls gave up: it settled dead in the script gate", cls);
        }
    }
}

void ClearClient() {
    g_pendingCount = 0;
    g_shotCount    = 0;
    g_nextShotId   = 1;
    g_deadLogged   = false;
    g_sendLogged   = false;
    g_unreadLogged = false;
}

}  // namespace detail

void OnClientProjectileBirth(void* actor) {
    if (!actor || !coop::spawn_authority::IsActiveClientSession()) return;
    const XS::Kind kind = XS::KindOfClass(R::ClassOf(actor));
    if (kind != XS::Kind::Rocket && kind != XS::Kind::Sticky) return;
    // A mirror's projectile is born outside every fire body, so it finds no launcher.
    int launcher = 0;
    for (int i = 0; i < kLaunchers; ++i) {
        void* body = g_body[i].Get();
        if (body && SG::IsBodyActive(body)) {
            launcher = i + 1;
            break;
        }
    }
    if (launcher == 0) return;
    if (g_pendingCount == kPendingCap) {
        for (size_t j = 1; j < g_pendingCount; ++j) g_pending[j - 1] = g_pending[j];
        --g_pendingCount;
        ++detail::MutCounters().pendingDropped;
    }
    g_pending[g_pendingCount++] = Pending{actor, R::InternalIndexOf(actor), launcher};
}

void OnClientBirth(void* sourceObject, void* /*explosionActor*/) {
    coop::net::Session* s = detail::SessionPtr();
    if (!s || !sourceObject) return;
    const XS::Kind kind = XS::KindOfClass(R::ClassOf(sourceObject));
    coop::net::ExplosionRequestPayload p{};
    p.srcClass = RC::SrcClassOf(kind);
    bool atSource = true;  // the blast's place is the source's; a pilled burn sends none
    switch (kind) {
        case XS::Kind::Rocket:
        case XS::Kind::Sticky: {
            const int at = FindShot(sourceObject);
            if (at < 0) return;
            p.shotId = g_shots[at].shotId;
            EraseShot(static_cast<size_t>(at));
            p.kind = static_cast<uint8_t>(RC::RequestKind::Projectile);
            break;
        }
        case XS::Kind::Grenade:
        case XS::Kind::Pipebomb:
        case XS::Kind::PoopGrenade:
        case XS::Kind::SoapGrenade:
            if (!coop::hand_item::IsLocalHandViewActor(sourceObject)) return;
            p.kind = static_cast<uint8_t>(RC::RequestKind::Hand);
            break;
        case XS::Kind::Player: {
            if (!coop::players::Registry::Get().IsLocal(sourceObject)) return;
            // The Blueprint zeroes the level after the spawn, so it is read here, at the birth.
            float level = 0.0f;
            if (!ue_wrap::vitals::Read(ue_wrap::vitals::Field::GasolinePilled, &level))
                return NoteUnread("the pill level");
            p.pillLevel = level;
            p.kind   = static_cast<uint8_t>(RC::RequestKind::Self);
            atSource = false;
            break;
        }
        case XS::Kind::None:
            return;
    }
    if (atSource) {
        FVector loc{};
        if (!E::TryGetActorLocation(sourceObject, loc)) return NoteUnread("the source's location");
        p.loc[0] = loc.X;
        p.loc[1] = loc.Y;
        p.loc[2] = loc.Z;
    }
    Send(*s, p);
}

}  // namespace coop::explosion_request
