// coop/world/explosion_request.cpp -- see coop/world/explosion_request.h.

#include "coop/world/explosion_request.h"

#include "explosion_request_internal.h"  // co-located private header (src tree, not include/)

#include "coop/world/explosion_recipe.h"
#include "coop/world/spawn_authority.h"

#include "coop/net/net_clock.h"
#include "coop/net/protocol.h"
#include "coop/net/refusal_tally.h"
#include "coop/net/session.h"
#include "coop/player/roster_ledger.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/world/explosion_source.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace coop::explosion_request {
namespace {

namespace RC = coop::explosion_recipe;
namespace XS = ue_wrap::explosion_source;

std::atomic<coop::net::Session*> g_session{nullptr};

Counters g_counters{};
coop::roster_ledger::PerSlotState<detail::Slot> g_slots;

// The first refusal after a quiet spell is said at once and a burst is summed into one line per window
// (coop/net/refusal_tally.h), so a sender at the edge's rate cannot make the log the cost.
coop::net::RefusalTally g_refusals{"explosion_request"};

bool Finite3(const float v[3]) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

bool Zero3(const float v[3]) { return v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f; }

// A projectile class with the launcher that fires it: the only pairs the game's Blueprints produce.
bool LegalPair(uint8_t projClass, uint8_t launcher) {
    const auto rpg    = static_cast<uint8_t>(RC::Launcher::Rpg);
    const auto sticky = static_cast<uint8_t>(RC::Launcher::StickyLauncher);
    const auto witch  = static_cast<uint8_t>(RC::Launcher::WitchCar);
    if (projClass == static_cast<uint8_t>(RC::ProjClass::Rocket)) return launcher == rpg || launcher == witch;
    if (projClass == static_cast<uint8_t>(RC::ProjClass::Sticky)) return launcher == sticky;
    return false;
}

bool FiredWellFormed(const coop::net::ProjectileFiredPayload& p) {
    return p.shotId != 0 && LegalPair(p.projClass, p.launcher) && Finite3(p.origin);
}

// A kind with the source classes that cause it, the ids it carries, and the fields it leaves zero.
bool RequestWellFormed(const coop::net::ExplosionRequestPayload& p) {
    if (!std::isfinite(p.pillLevel) || !Finite3(p.loc)) return false;
    const XS::Kind src = RC::KindOfSrcClass(p.srcClass);
    switch (static_cast<RC::RequestKind>(p.kind)) {
        case RC::RequestKind::Projectile:
            return (src == XS::Kind::Rocket || src == XS::Kind::Sticky) && p.shotId != 0 && p.pillLevel == 0.0f;
        case RC::RequestKind::Hand:
            return (src == XS::Kind::Grenade || src == XS::Kind::Pipebomb || src == XS::Kind::PoopGrenade ||
                    src == XS::Kind::SoapGrenade) && p.shotId == 0 && p.pillLevel == 0.0f;
        case RC::RequestKind::Self:
            return src == XS::Kind::Player && p.shotId == 0 && Zero3(p.loc);
    }
    return false;
}

bool ValidSenderSlot(int senderSlot) { return senderSlot >= 1 && senderSlot < coop::net::kMaxPeers; }

// A sender still in its load window has no puppet and no hand state to judge against; MTA drops a packet from a
// player that has not finished joining (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:2835, :2957).
bool SenderWorldReady(uint8_t slot) {
    const coop::net::Session* s = detail::SessionPtr();
    return s && s->IsSlotWorldReady(slot);
}

// A rocket's life is 30 s and 2 s more are allowed; an entry past that can no longer be asked for. A sticky waits
// for its launcher's RMB or destruction, so it has no age.
constexpr uint64_t kRocketLifeMs = 32000;

void ExpireRockets(detail::Slot& sl) {
    if (sl.count == 0) return;
    const uint64_t now = coop::net::NowMs();
    for (size_t i = 0; i < sl.count;) {
        const detail::Shot& sh = sl.shots[i];
        if (sh.projClass == static_cast<uint8_t>(RC::ProjClass::Rocket) && now > sh.recvMs + kRocketLifeMs) {
            for (size_t j = i + 1; j < sl.count; ++j) sl.shots[j - 1] = sl.shots[j];
            --sl.count;
            ++g_counters.expired;
        } else {
            ++i;
        }
    }
}

}  // namespace

const char* RefusalName(Refusal r) {
    switch (r) {
        case Refusal::Malformed:     return "malformed";
        case Refusal::DuplicateShot: return "duplicate_shot";
        case Refusal::QueueFull:     return "queue_full";
        case Refusal::NotWorldReady: return "not_world_ready";
        case Refusal::NoLauncher:    return "no_launcher";
        case Refusal::FungunRule:    return "fungun_rule";
        case Refusal::NotSeated:     return "not_seated";
        case Refusal::OriginBound:   return "origin_bound";
        case Refusal::UnknownShot:   return "unknown_shot";
        case Refusal::Reach:         return "reach";
        case Refusal::NoExplosive:   return "no_explosive";
        case Refusal::HandBound:     return "hand_bound";
        case Refusal::NoPuppet:      return "no_puppet";
        case Refusal::PuppetDead:    return "puppet_dead";
        case Refusal::Unresolved:    return "unresolved";
        case Refusal::SpawnFailed:   return "spawn_failed";
        case Refusal::Count:         break;
    }
    return "unknown";
}

Counters GetCounters() { return g_counters; }

namespace detail {

void Refuse(uint8_t slot, Refusal r) {
    ++g_counters.refused[static_cast<int>(r)];
    g_refusals.Note(slot, RefusalName(r), coop::net::RefusalTally::Clock::now());
}

coop::net::Session* SessionPtr() { return g_session.load(std::memory_order_acquire); }

Slot& SlotOf(uint8_t slot) { return g_slots[slot]; }

Counters& MutCounters() { return g_counters; }

}  // namespace detail

void Install(coop::net::Session* s) {
    g_session.store(s, std::memory_order_release);
    detail::InstallClient();
}

void Tick() {
    if (coop::spawn_authority::IsActiveClientSession()) {  // only a client reads a source class
        XS::Resolve();
        detail::TickClient();
    }
    g_refusals.Flush(coop::net::RefusalTally::Clock::now());
    coop::net::Session* s = detail::SessionPtr();
    if (!s || !s->running() || s->role() != coop::net::Role::Host) return;
    // Per slot, in order: its shots, then the blasts of those shots, then the blasts of its body.
    for (uint8_t slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        detail::Slot& sl = g_slots[slot];
        sl.fired.Serve(detail::kFired, coop::net::NowMs, [&](const detail::FiredItem& it) {
            return detail::JudgeShot(slot, sl, it);
        });
        sl.shotBlast.Serve(detail::kShotBlast, coop::net::NowMs, [&](const detail::RequestItem& it) {
            return detail::JudgeShotBlast(slot, sl, it);
        });
        sl.bodyBlast.Serve(detail::kBodyBlast, coop::net::NowMs, [&](const detail::RequestItem& it) {
            return detail::JudgeBody(slot, sl, it);
        });
        ExpireRockets(sl);
    }
}

void OnWorldChange() {
    for (int i = 0; i < g_slots.size(); ++i) g_slots[i] = detail::Slot{};
    detail::ClearClientWorld();
}

void OnDisconnect() {
    char line[512];
    size_t n = 0;
    line[0] = '\0';
    for (int r = 0; r < static_cast<int>(Refusal::Count); ++r) {
        if (g_counters.refused[r] == 0 || n >= sizeof(line)) continue;
        const int w = std::snprintf(line + n, sizeof(line) - n, " %s=%llu",
                                    RefusalName(static_cast<Refusal>(r)),
                                    static_cast<unsigned long long>(g_counters.refused[r]));
        if (w > 0) n += static_cast<size_t>(w);
    }
    // The accepted shots by launcher, the launchers that took one only; the label prints only when one did.
    char   by[128];
    size_t nb = 0;
    by[0] = '\0';
    for (size_t l = 0; l < RC::kLauncherSlots; ++l) {
        if (g_counters.shotsAcceptedBy[l] == 0 || nb >= sizeof(by)) continue;
        const int w = std::snprintf(by + nb, sizeof(by) - nb, " launcher%zu=%llu", l,
                                    static_cast<unsigned long long>(g_counters.shotsAcceptedBy[l]));
        if (w > 0) nb += static_cast<size_t>(w);
    }
    const Counters& c = g_counters;
    const auto u = [](uint64_t v) { return static_cast<unsigned long long>(v); };
    const bool hostSaw = n > 0 || c.shotsAccepted || c.evicted || c.expired || c.waited || c.requestsAccepted[0] ||
                         c.requestsAccepted[1] || c.requestsAccepted[2];
    if (hostSaw)
        UE_LOGI("[EXPLOSION] request lane at disconnect: shots=%llu requests=%llu/%llu/%llu evicted=%llu "
                "expired=%llu waited=%llu%s%s refused%s", u(c.shotsAccepted), u(c.requestsAccepted[0]),
                u(c.requestsAccepted[1]), u(c.requestsAccepted[2]), u(c.evicted), u(c.expired), u(c.waited),
                nb > 0 ? " shotsBy" : "", by, line);
    if (c.shotsSent || c.requestsSent[0] || c.requestsSent[1] || c.requestsSent[2] || c.pendingDropped ||
        c.pendingDead || c.pendingOrphaned || c.clientDropped || c.unmappedProjectile || c.shotNotAnnounced)
        UE_LOGI("[EXPLOSION] request lane at disconnect (client): shotsSent=%llu requestsSent=%llu/%llu/%llu "
                "pendingDropped=%llu pendingDead=%llu pendingOrphaned=%llu clientDropped=%llu "
                "unmappedProjectile=%llu shotNotAnnounced=%llu", u(c.shotsSent), u(c.requestsSent[0]),
                u(c.requestsSent[1]), u(c.requestsSent[2]), u(c.pendingDropped), u(c.pendingDead),
                u(c.pendingOrphaned), u(c.clientDropped), u(c.unmappedProjectile), u(c.shotNotAnnounced));
    for (int i = 0; i < g_slots.size(); ++i) g_slots[i] = detail::Slot{};
    detail::ClearClient();
    g_counters = Counters{};
    g_refusals.Clear();
}

void OnFiredReceived(int senderSlot, const uint8_t* bytes, uint16_t len) {
    if (!ValidSenderSlot(senderSlot)) return;  // the dispatch's own guard, kept at this lane's edge
    const auto slot = static_cast<uint8_t>(senderSlot);
    coop::net::ProjectileFiredPayload p{};
    if (!SenderWorldReady(slot)) return detail::Refuse(slot, Refusal::NotWorldReady);
    if (!bytes || len != sizeof(p)) return detail::Refuse(slot, Refusal::Malformed);
    std::memcpy(&p, bytes, sizeof(p));
    if (!FiredWellFormed(p)) return detail::Refuse(slot, Refusal::Malformed);
    if (!detail::SlotOf(slot).fired.Push(detail::FiredItem{p, coop::net::NowMs()}, detail::kFiredCap))
        detail::Refuse(slot, Refusal::QueueFull);
}

void OnRequestReceived(int senderSlot, const uint8_t* bytes, uint16_t len) {
    if (!ValidSenderSlot(senderSlot)) return;  // the dispatch's own guard, kept at this lane's edge
    const auto slot = static_cast<uint8_t>(senderSlot);
    coop::net::ExplosionRequestPayload p{};
    if (!SenderWorldReady(slot)) return detail::Refuse(slot, Refusal::NotWorldReady);
    if (!bytes || len != sizeof(p)) return detail::Refuse(slot, Refusal::Malformed);
    std::memcpy(&p, bytes, sizeof(p));
    if (!RequestWellFormed(p)) return detail::Refuse(slot, Refusal::Malformed);
    detail::Slot& sl = detail::SlotOf(slot);
    const bool projectile = p.kind == static_cast<uint8_t>(RC::RequestKind::Projectile);
    auto& queue = projectile ? sl.shotBlast : sl.bodyBlast;
    const size_t cap = projectile ? detail::kShotBlastCap : detail::kBodyBlastCap;
    if (!queue.Push(detail::RequestItem{p, coop::net::NowMs()}, cap)) detail::Refuse(slot, Refusal::QueueFull);
}

}  // namespace coop::explosion_request
