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

detail::Counters g_counters{};
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

namespace detail {

const char* RefusalName(Refusal r) {
    switch (r) {
        case Refusal::Malformed:     return "malformed";
        case Refusal::DuplicateShot: return "duplicate_shot";
        case Refusal::QueueFull:     return "queue_full";
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
    if (coop::spawn_authority::IsActiveClientSession()) XS::Resolve();  // only a client reads a source class
    g_refusals.Flush(coop::net::RefusalTally::Clock::now());
    coop::net::Session* s = detail::SessionPtr();
    if (!s || !s->running() || s->role() != coop::net::Role::Host) return;
    // Per slot, in order: its shots, then the blasts of those shots, then the blasts of its body.
    for (uint8_t slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        detail::Slot& sl = g_slots[slot];
        sl.fired.Serve(detail::kFired, coop::net::NowMs, [&](const detail::FiredItem& it) {
            return detail::JudgeShot(slot, sl, it, coop::net::NowMs());
        });
        sl.shotBlast.Serve(detail::kShotBlast, coop::net::NowMs, [&](const detail::RequestItem& it) {
            return detail::JudgeShotBlast(slot, sl, it, coop::net::NowMs());
        });
        sl.bodyBlast.Serve(detail::kBodyBlast, coop::net::NowMs, [&](const detail::RequestItem& it) {
            return detail::JudgeBody(slot, sl, it, coop::net::NowMs());
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
    for (int r = 0; r < static_cast<int>(detail::Refusal::Count); ++r) {
        if (g_counters.refused[r] == 0 || n >= sizeof(line)) continue;
        const int w = std::snprintf(line + n, sizeof(line) - n, " %s=%llu",
                                    detail::RefusalName(static_cast<detail::Refusal>(r)),
                                    static_cast<unsigned long long>(g_counters.refused[r]));
        if (w > 0) n += static_cast<size_t>(w);
    }
    const detail::Counters& c = g_counters;
    if (n > 0 || c.shotsAccepted || c.requestsAccepted || c.evicted || c.expired)
        UE_LOGI("[EXPLOSION] request lane at disconnect: shots=%llu requests=%llu evicted=%llu expired=%llu refused%s",
                static_cast<unsigned long long>(c.shotsAccepted), static_cast<unsigned long long>(c.requestsAccepted),
                static_cast<unsigned long long>(c.evicted), static_cast<unsigned long long>(c.expired), line);
    if (c.shotsSent || c.requestsSent || c.pendingDropped || c.pendingDead || c.pendingOrphaned || c.clientDropped ||
        c.unmappedProjectile)
        UE_LOGI("[EXPLOSION] request lane at disconnect (client): shotsSent=%llu requestsSent=%llu "
                "pendingDropped=%llu pendingDead=%llu pendingOrphaned=%llu clientDropped=%llu "
                "unmappedProjectile=%llu", static_cast<unsigned long long>(c.shotsSent),
                static_cast<unsigned long long>(c.requestsSent), static_cast<unsigned long long>(c.pendingDropped),
                static_cast<unsigned long long>(c.pendingDead), static_cast<unsigned long long>(c.pendingOrphaned),
                static_cast<unsigned long long>(c.clientDropped),
                static_cast<unsigned long long>(c.unmappedProjectile));
    for (int i = 0; i < g_slots.size(); ++i) g_slots[i] = detail::Slot{};
    detail::ClearClient();
    g_counters = detail::Counters{};
    g_refusals.Clear();
}

void OnFiredReceived(int senderSlot, const uint8_t* bytes, uint16_t len) {
    if (!ValidSenderSlot(senderSlot)) return;  // the dispatch's own guard, kept at this lane's edge
    const auto slot = static_cast<uint8_t>(senderSlot);
    coop::net::ProjectileFiredPayload p{};
    if (!bytes || len != sizeof(p)) return detail::Refuse(slot, detail::Refusal::Malformed);
    std::memcpy(&p, bytes, sizeof(p));
    if (!FiredWellFormed(p)) return detail::Refuse(slot, detail::Refusal::Malformed);
    if (!detail::SlotOf(slot).fired.Push(detail::FiredItem{p, coop::net::NowMs()}, detail::kFiredCap))
        detail::Refuse(slot, detail::Refusal::QueueFull);
}

void OnRequestReceived(int senderSlot, const uint8_t* bytes, uint16_t len) {
    if (!ValidSenderSlot(senderSlot)) return;  // the dispatch's own guard, kept at this lane's edge
    const auto slot = static_cast<uint8_t>(senderSlot);
    coop::net::ExplosionRequestPayload p{};
    if (!bytes || len != sizeof(p)) return detail::Refuse(slot, detail::Refusal::Malformed);
    std::memcpy(&p, bytes, sizeof(p));
    if (!RequestWellFormed(p)) return detail::Refuse(slot, detail::Refusal::Malformed);
    detail::Slot& sl = detail::SlotOf(slot);
    const bool projectile = p.kind == static_cast<uint8_t>(RC::RequestKind::Projectile);
    auto& queue = projectile ? sl.shotBlast : sl.bodyBlast;
    const size_t cap = projectile ? detail::kShotBlastCap : detail::kBodyBlastCap;
    if (!queue.Push(detail::RequestItem{p, coop::net::NowMs()}, cap)) detail::Refuse(slot, detail::Refusal::QueueFull);
}

}  // namespace coop::explosion_request
