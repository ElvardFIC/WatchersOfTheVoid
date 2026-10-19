// coop/player/pvp.cpp -- see coop/player/pvp.h.

#include "coop/player/pvp.h"

#include "coop/config/config.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/session/player_handshake.h"

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/actors/inventory.h"  // ReadHeldItemName: a shovel is a hotbar row, not always a world prop
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_pawn.h"  // GetController / GetControlRotation: the swing faces the crosshair
#include "ue_wrap/world/weapon_catalog.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace coop::pvp {
namespace {
UE_FACT_FN(kFireEvent58, "pvp", L"mainPlayer_C", L"InpActEvt_fire_K2Node_InputActionEvent_58", Declared);
UE_FACT_FN(kFireEvent59, "pvp", L"mainPlayer_C", L"InpActEvt_fire_K2Node_InputActionEvent_59", Declared);
UE_FACT_FN(kAttackVerb, "pvp", L"mainPlayer_C", L"attack", Declared);
UE_FACT_FN(kSwingMeleeVerb, "pvp", L"mainPlayer_C", L"swingMelee", Declared);
UE_FACT_FN(kFireEvent, "pvp", L"mainPlayer_C", L"InpActEvt_fire_K2Node_InputActionEvent", Declared);

namespace R = ue_wrap::reflection;
namespace E = ue_wrap::engine;
namespace GT = ue_wrap::game_thread;

std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<bool> g_local{false};
std::atomic<bool> g_slot[coop::players::kMaxPeers]{};
std::atomic<void*> g_localPawn{nullptr};
std::atomic<bool> g_canSwing{false};
std::atomic<uint64_t> g_swingUntil{0};

struct ThrowNote {
    void* actor = nullptr;
    int32_t idx = -1;
    uint8_t slot = 0xFF;
    bool explosive = false;
    uint16_t nonce = 0;
    uint64_t ms = 0;
};
struct SpentThrow {
    uint16_t nonce = 0;
    uint64_t ms = 0;
};
std::mutex g_throwMu;
std::vector<ThrowNote> g_throws;
std::vector<SpentThrow> g_spent;

struct Rate {
    float tokens = 4.f;
    uint64_t ms = 0;
};
Rate g_rate[coop::players::kMaxPeers];

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

bool ExplosiveClass(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    std::wstring name = R::ToString(R::NameOf(R::ClassOf(actor)));
    for (wchar_t& c : name) if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    return name.find(L"bomb") != std::wstring::npos ||
           name.find(L"explos") != std::wstring::npos ||
           name.find(L"gascan") != std::wstring::npos ||
           name.find(L"pipebomb") != std::wstring::npos ||
           name.find(L"pipe_bomb") != std::wstring::npos ||
           name.find(L"canister") != std::wstring::npos ||
           name.find(L"bego") != std::wstring::npos ||
           name.find(L"dynamite") != std::wstring::npos;
}

void* ParamActor(void* function, void* params) {
    if (!function || !params) return nullptr;
    static const wchar_t* kNames[] = { L"OtherActor", L"Actor", L"HitActor", L"DamageCauser" };
    auto* base = static_cast<uint8_t*>(params);
    for (const wchar_t* name : kNames) {
        const int32_t off = R::FindParamOffset(function, name);
        if (off < 0) continue;
        void* actor = *reinterpret_cast<void**>(base + off);
        if (actor && R::IsLive(actor)) return actor;
    }
    static bool said = false;
    if (!said && function) {
        said = true;
        UE_LOGW("pvp: an impact carried none of OtherActor/Actor/HitActor/DamageCauser");
    }
    return nullptr;
}

// Caller holds g_throwMu.
const ThrowNote* FindThrowLocked(void* actor) {
    if (!actor) return nullptr;
    const int32_t idx = R::InternalIndexOf(actor);
    const uint64_t now = NowMs();
    for (auto it = g_throws.begin(); it != g_throws.end();) {
        if (now - it->ms > 8000) { it = g_throws.erase(it); continue; }
        if (it->actor == actor && it->idx == idx) return &*it;
        ++it;
    }
    return nullptr;
}

bool BothOn(uint8_t a, uint8_t b) {
    if (a >= coop::players::kMaxPeers || b >= coop::players::kMaxPeers || a == b) return false;
    const bool ao = (a == coop::players::Registry::Get().LocalPeerId()) ? g_local.load() : g_slot[a].load();
    const bool bo = (b == coop::players::Registry::Get().LocalPeerId()) ? g_local.load() : g_slot[b].load();
    return ao && bo;
}

bool RateOk(uint8_t slot) {
    if (slot >= coop::players::kMaxPeers) return false;
    const uint64_t now = NowMs();
    Rate& r = g_rate[slot];
    if (r.ms != 0) {
        const float dt = static_cast<float>(now - r.ms) * 0.001f;
        r.tokens += dt * 4.f;
        if (r.tokens > 4.f) r.tokens = 4.f;
    }
    r.ms = now;
    if (r.tokens < 1.f) return false;
    r.tokens -= 1.f;
    return true;
}

bool Near(uint8_t a, uint8_t b, float reachCm) {
    auto pos = [](uint8_t slot, ue_wrap::FVector& out) {
        void* actor = nullptr;
        if (slot == coop::players::Registry::Get().LocalPeerId())
            actor = coop::players::Registry::Get().Local();
        else if (coop::RemotePlayer* p = coop::players::Registry::Get().Puppet(slot))
            actor = p->GetActor();
        return actor && E::TryGetActorLocation(actor, out);
    };
    ue_wrap::FVector pa{}, pb{};
    if (!pos(a, pa) || !pos(b, pb)) return false;
    const float dx = pa.X - pb.X, dy = pa.Y - pb.Y, dz = pa.Z - pb.Z;
    return dx * dx + dy * dy + dz * dz < reachCm * reachCm;
}

float SwingDamage() {
    const uint8_t slot = coop::players::Registry::Get().LocalPeerId();
    std::wstring item = coop::hand_item::HeldItem(slot);
    // The hotbar row is filled for a world prop in the hand. A shovel that never enters that
    // table still has its name on the save slot, which is the catalog's key.
    if (item.empty()) item = ue_wrap::inventory::ReadHeldItemName();
    ue_wrap::weapon_catalog::Swing swing;
    if (ue_wrap::weapon_catalog::Ready() && ue_wrap::weapon_catalog::Lookup(item, swing) && swing.maxDamage > 0.f)
        return swing.maxDamage;
    return 15.f;
}

// True when this toss was already applied here. A swing's nonce is 0 and is never listed.
bool ThrowAlreadySpent(uint16_t nonce) {
    if (nonce == 0) return false;
    std::lock_guard<std::mutex> lk(g_throwMu);
    const uint64_t now = NowMs();
    for (auto it = g_spent.begin(); it != g_spent.end();) {
        if (now - it->ms > 8000) { it = g_spent.erase(it); continue; }
        if (it->nonce == nonce) return true;
        ++it;
    }
    return false;
}

// True the first time this toss is applied on this machine. A swing passes nonce 0 and is not
// tracked: the sender already collapsed the two hit entries of one click.
bool SpendThrow(uint16_t nonce) {
    if (nonce == 0) return true;
    std::lock_guard<std::mutex> lk(g_throwMu);
    const uint64_t now = NowMs();
    for (auto it = g_spent.begin(); it != g_spent.end();) {
        if (now - it->ms > 8000) { it = g_spent.erase(it); continue; }
        if (it->nonce == nonce) return false;
        ++it;
    }
    g_spent.push_back(SpentThrow{ nonce, now });
    return true;
}

void ApplyHit(float damage) {
    if (!g_local.load()) return;
    void* mp = coop::players::Registry::Get().Local();
    if (!mp || !R::IsLive(mp)) return;
    bool ragdoll = false, dead = false;
    if (E::ReadMainPlayerRagdollState(mp, ragdoll, dead) && dead) return;
    // One player hit, then a pause. A ragdoll used to eat every following swing, and a few
    // of them in one second were a kill. Explosions do not come through here.
    const uint64_t now = NowMs();
    static uint64_t s_hurtUntil = 0;
    if (now < s_hurtUntil) return;
    if (damage < 1.f) damage = 1.f;
    if (damage > 100.f) damage = 100.f;
    s_hurtUntil = now + 3000;
    E::InvokeAddPlayerDamage(mp, damage);
    if (!ragdoll) E::SetMainPlayerRagdollMode(mp, true, false, false);
    UE_LOGI("pvp: took %.0f and ragdolled", damage);
}

void SendHit(uint8_t attacker, uint8_t victim, float damage, float reachCm, uint16_t throwNonce) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    if (attacker >= coop::players::kMaxPeers || victim >= coop::players::kMaxPeers || attacker == victim)
        return;
    const uint8_t me = coop::players::Registry::Get().LocalPeerId();
    // A client only reports its own swing or its own throw. The host also reports a throw it
    // simulated for whichever peer let go, and the check below is that peer's PVP, not the host's.
    if (s->role() != coop::net::Role::Host && attacker != me) return;
    if (!BothOn(attacker, victim) || !Near(attacker, victim, reachCm)) return;
    // impactDamage and addDamage both fire for one click, in the same dispatch. A later click is
    // a new hit; the rate gate is what spaces those out. Two peers hitting the same person in
    // that same instant each still count.
    static uint8_t s_lastAttacker = 0xFF;
    static uint8_t s_lastVictim = 0xFF;
    static uint64_t s_lastMs = 0;
    const uint64_t now = NowMs();
    if (s_lastAttacker == attacker && s_lastVictim == victim && s_lastMs != 0 && now - s_lastMs < 40)
        return;
    if (!RateOk(attacker)) return;
    s_lastAttacker = attacker;
    s_lastVictim = victim;
    s_lastMs = now;
    if (victim == me) {
        if (throwNonce == 0 || SpendThrow(throwNonce)) ApplyHit(damage);
        return;
    }
    coop::net::PvpHitPayload p{};
    p.targetSlot = victim;
    p.damage = damage;
    if (throwNonce != 0) {
        p.throwNonce[0] = static_cast<uint8_t>(throwNonce & 0xFF);
        p.throwNonce[1] = static_cast<uint8_t>(throwNonce >> 8);
        p.isThrow = 1;
    }
    if (s->role() == coop::net::Role::Host) {
        s->SendReliableToSlot(victim, coop::net::ReliableKind::PvpHit, &p, sizeof(p));
        return;
    }
    s->SendReliable(coop::net::ReliableKind::PvpHit, &p, sizeof(p));
}

// The shovel trace often never reaches the puppet, so the impact hook stays silent and the
// nickname still shows PVP. A swing in front of a peer is the hit.
void TryMelee() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !g_local.load()) return;
    const uint8_t me = coop::players::Registry::Get().LocalPeerId();
    void* self = coop::players::Registry::Get().Local();
    ue_wrap::FVector origin{};
    if (!self || !E::TryGetActorLocation(self, origin)) return;
    const ue_wrap::FVector actorFwd = E::GetActorForwardVector(self);
    ue_wrap::FVector look = actorFwd;
    if (void* ctrl = ue_wrap::engine::GetController(self)) {
        const ue_wrap::FRotator view = ue_wrap::engine::GetControlRotation(ctrl);
        const float yaw = view.Yaw * 0.01745329252f;
        const float pitch = view.Pitch * 0.01745329252f;
        const float cp = std::cos(pitch);
        look = { std::cos(yaw) * cp, std::sin(yaw) * cp, std::sin(pitch) };
    }
    const float lookLen = std::sqrt(look.X * look.X + look.Y * look.Y + look.Z * look.Z);
    const bool haveLook = lookLen > 0.1f;
    constexpr float kReach = 320.f;
    for (uint8_t slot = 0; slot < coop::players::kMaxPeers; ++slot) {
        if (slot == me || !BothOn(me, slot)) continue;
        coop::RemotePlayer* pup = coop::players::Registry::Get().Puppet(slot);
        void* actor = pup ? pup->GetActor() : nullptr;
        ue_wrap::FVector at{};
        if (!actor || !E::TryGetActorLocation(actor, at)) continue;
        const float dx = at.X - origin.X, dy = at.Y - origin.Y, dz = at.Z - origin.Z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > kReach * kReach || d2 < 1.f) continue;
        if (haveLook) {
            const float inv = 1.f / std::sqrt(d2);
            const float dot = (dx * inv) * look.X + (dy * inv) * look.Y + (dz * inv) * look.Z;
            if (dot < 0.25f) continue;
        }
        SendHit(me, slot, SwingDamage(), kReach + 80.f, 0);
    }
}

}  // namespace

void SetInitialLocal(bool on) { g_local.store(on); }
bool LocalOn() { return g_local.load(); }

void RequestLocal(bool on) {
    coop::config::SetValue(coop::config_registry::rows::pvp, on ? "1" : "0");
    GT::Post([on] {
        g_local.store(on);
        const uint8_t me = coop::players::Registry::Get().LocalPeerId();
        if (me < coop::players::kMaxPeers) g_slot[me].store(on);
        UE_LOGI("pvp: local -> %s", on ? "ON" : "OFF");
        if (coop::net::Session* s = g_session.load(std::memory_order_acquire))
            coop::player_handshake::AnnounceLocalPvp(*s, on);
    });
}

void StoreForSlot(int slot, bool on) {
    if (slot < 0 || slot >= static_cast<int>(coop::players::kMaxPeers)) return;
    g_slot[slot].store(on);
}

bool OnForSlot(int slot) {
    if (slot < 0 || slot >= static_cast<int>(coop::players::kMaxPeers)) return false;
    if (static_cast<uint8_t>(slot) == coop::players::Registry::Get().LocalPeerId())
        return g_local.load();
    return g_slot[slot].load();
}

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    const uint8_t me = coop::players::Registry::Get().LocalPeerId();
    if (me < coop::players::kMaxPeers) g_slot[me].store(g_local.load());
}

void OnSlotDisconnected(int slot) {
    if (slot >= 0 && slot < static_cast<int>(coop::players::kMaxPeers)) g_slot[slot].store(false);
}

uint16_t MintThrowNonce() {
    static std::atomic<uint16_t> n{1};
    uint16_t v = n.fetch_add(1, std::memory_order_relaxed);
    if (v == 0) v = n.fetch_add(1, std::memory_order_relaxed);
    return v;
}

void NoteThrow(void* actor, uint8_t slot, uint16_t nonce) {
    if (!actor || slot >= coop::players::kMaxPeers || nonce == 0) return;
    const bool explosive = ExplosiveClass(actor);
    std::lock_guard<std::mutex> lk(g_throwMu);
    const uint64_t now = NowMs();
    for (auto it = g_throws.begin(); it != g_throws.end();) {
        if (now - it->ms > 8000 || it->actor == actor) it = g_throws.erase(it);
        else ++it;
    }
    g_throws.push_back(ThrowNote{ actor, R::InternalIndexOf(actor), slot, explosive, nonce, now });
}

void NoteBodies(void* localPawn, void* held) {
    g_localPawn.store(localPawn, std::memory_order_release);
    // A shovel lives in the hotbar row. Its holding actor is often not a world prop, so the prop
    // pointer alone would make every click look like an empty hand.
    const bool named = !ue_wrap::inventory::ReadHeldItemName().empty();
    g_canSwing.store(held != nullptr || named, std::memory_order_release);
}

void OnFirePressed(void* self, void*, void*) {
    void* local = g_localPawn.load(std::memory_order_acquire);
    if (!local || self != local) return;
    g_swingUntil.store(NowMs() + 800, std::memory_order_release);
    GT::Post([] { TryMelee(); });
}

void EnsureFireWatch() {
    static bool done = false;
    if (done) return;
    void* cls = R::FindClass(ue_wrap::profile::name::MainPlayerClass);
    if (!cls) return;
    // The shovel and the crowbar swing through attack(), not only through one numbered fire
    // node. A build whose fire event is not _58 still arms the window from attack or the
    // unsuffixed fire event.
    const ue_wrap::facts::Function* rows[] = {&kFireEvent58, &kFireEvent59, &kFireEvent, &kAttackVerb, &kSwingMeleeVerb};
    int n = 0;
    for (const auto* row : rows) {
        const auto fn = ue_wrap::facts::ResolveOn(*row, cls);
        if (fn && GT::RegisterPreObserver(fn, &OnFirePressed)) ++n;
    }
    if (n == 0) return;  // the class was up but the swing functions were not; try again next tick
    done = true;
    UE_LOGI("pvp: a swing counts for 0.8 s after fire or attack (%d watch%s)",
            n, n == 1 ? "" : "es");
}

void Tick() {
    EnsureFireWatch();
    // A toggle that happened before the other peer was ready never left this machine. Say the
    // current flag again once the session is up; a later toggle still announces itself.
    static bool said = false;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) { said = false; return; }
    if (said) return;
    said = true;
    coop::player_handshake::AnnounceLocalPvp(*s, g_local.load());
}

bool CancelLocalImpact(void* params, void* function) {
    void* other = ParamActor(function, params);
    if (!other) return false;
    uint8_t from = 0xFF;
    uint16_t nonce = 0;
    bool explosive = false;
    {
        std::lock_guard<std::mutex> lk(g_throwMu);
        const ThrowNote* note = FindThrowLocked(other);
        if (!note) return false;
        explosive = note->explosive;
        from = note->slot;
        nonce = note->nonce;
    }
    if (explosive) return false;
    const uint8_t me = coop::players::Registry::Get().LocalPeerId();
    if (!BothOn(me, from)) return true;
    // The thrown prop's own impact does not write this player's health. Apply it here when this
    // machine is the one the prop actually hits, and swallow the body. The toss's nonce makes the
    // copy that arrives from the other machine a no-op. SpendThrow takes the same mutex, so the
    // note lookup has to let go of it first.
    if (!SpendThrow(nonce)) return true;
    GT::Post([] { ApplyHit(25.f); });
    return true;
}

void OnPuppetImpact(void* puppet, void* params, void* function) {
    if (!puppet) return;
    void* other = ParamActor(function, params);
    // A shovel swing's impact names the puppet, not the shovel mesh. Any hit on a puppet in the
    // short window after this player's own fire or attack, while they are holding something, is
    // that swing.
    const bool swinging = g_canSwing.load(std::memory_order_acquire) &&
                          NowMs() <= g_swingUntil.load(std::memory_order_acquire);
    bool thrown = false;
    bool explosive = false;
    uint16_t nonce = 0;
    uint8_t fromSlot = 0xFF;
    {
        std::lock_guard<std::mutex> lk(g_throwMu);
        if (const ThrowNote* note = FindThrowLocked(other)) {
            thrown = true;
            explosive = note->explosive;
            nonce = note->nonce;
            fromSlot = note->slot;
        }
    }
    if (explosive) return;  // the explosion still hits everyone through the game
    if (!swinging && !thrown) return;
    GT::Post([puppet, thrown, nonce, fromSlot] {
        if (!R::IsLive(puppet)) return;
        const uint8_t victim = coop::players::Registry::Get().PeerIdOfActor(puppet);
        if (victim == coop::players::kPeerIdUnknown) return;
        const uint8_t me = coop::players::Registry::Get().LocalPeerId();
        const uint8_t attacker = thrown ? fromSlot : me;
        const float damage = thrown ? 25.f : SwingDamage();
        const float reach = thrown ? 2500.f : 600.f;
        SendHit(attacker, victim, damage, reach, thrown ? nonce : 0);
    });
}

void OnHit(const coop::net::PvpHitPayload& payload, uint8_t senderSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    if (!std::isfinite(payload.damage) || payload.damage <= 0.f) return;
    const uint8_t victim = payload.targetSlot;
    if (victim >= coop::players::kMaxPeers) return;
    if (s->role() == coop::net::Role::Host) {
        if (senderSlot == 0 || senderSlot >= coop::players::kMaxPeers) return;
        if (senderSlot == victim) return;
        const uint16_t nonce = static_cast<uint16_t>(payload.throwNonce[0] |
                                                     (payload.throwNonce[1] << 8));
        // The local body already applied this toss. Do not spend another rate token on the copy.
        if (payload.isThrow && victim == coop::players::Registry::Get().LocalPeerId() &&
            ThrowAlreadySpent(nonce))
            return;
        // A swing was already limited to arm's reach on the way out. The host checks the same
        // reach again; a throw is allowed from further away.
        const float reach = payload.isThrow ? 2500.f : 600.f;
        if (!BothOn(senderSlot, victim) || !RateOk(senderSlot) || !Near(senderSlot, victim, reach)) return;
        float damage = payload.damage;
        if (damage > 100.f) damage = 100.f;
        if (victim == coop::players::Registry::Get().LocalPeerId()) {
            if (payload.isThrow && !SpendThrow(nonce)) return;
            ApplyHit(damage);
            return;
        }
        coop::net::PvpHitPayload fwd = payload;
        fwd.damage = damage;
        s->SendReliableToSlot(victim, coop::net::ReliableKind::PvpHit, &fwd, sizeof(fwd));
        return;
    }
    if (senderSlot != 0) return;
    if (victim != coop::players::Registry::Get().LocalPeerId()) return;
    const uint16_t nonce = static_cast<uint16_t>(payload.throwNonce[0] |
                                                 (payload.throwNonce[1] << 8));
    if (payload.isThrow && !SpendThrow(nonce)) return;
    ApplyHit(payload.damage);
}

}  // namespace coop::pvp
