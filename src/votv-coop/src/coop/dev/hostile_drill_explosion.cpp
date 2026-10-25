// coop/dev/hostile_drill_explosion.cpp -- see coop/dev/hostile_drill_explosion.h. The arm table, the verdict print, the
// lobby-chat lines and the family's entry points; the client's sender is hostile_drill_explosion_client.cpp and the
// host's judge hostile_drill_explosion_host.cpp.

#include "coop/dev/hostile_drill_explosion.h"

#include "hostile_drill_explosion_detail.h"  // co-located private header (src tree, not include/)

#include "coop/comms/chat_feed.h"
#include "coop/comms/chat_sync.h"
#include "coop/dev/announce_hold.h"
#include "coop/net/session.h"

#include "ue_wrap/core/log.h"

#include <cstdint>
#include <cstring>
#include <string_view>

namespace coop::dev::hostile_drill::explosion {
namespace detail {
namespace {

using Refusal = coop::explosion_request::Refusal;
constexpr Refusal kNone = Refusal::Count;

// The arms: the messages each sends, then the host's expected deltas (shots, requests by kind, the one reason and its
// count, evicted).
const Spec kSpecs[] = {
    {"explosion_malformed",        Arm::Malformed,      Hand::Empty,   Mode::Plain,    4,   0,  {0, 0, 0}, Refusal::Malformed,     4, 0, false},
    {"explosion_duplicate",        Arm::Duplicate,      Hand::Rpg,     Mode::Plain,    2,   1,  {0, 0, 0}, Refusal::DuplicateShot, 1, 0, false},
    {"explosion_unknown_shot",     Arm::UnknownShot,    Hand::Empty,   Mode::Plain,    1,   0,  {0, 0, 0}, Refusal::UnknownShot,   1, 0, false},
    {"explosion_consumed",         Arm::Consumed,       Hand::Rpg,     Mode::Plain,    3,   1,  {1, 0, 0}, Refusal::UnknownShot,   1, 0, false},
    {"explosion_no_launcher",      Arm::NoLauncher,     Hand::Empty,   Mode::Plain,    1,   0,  {0, 0, 0}, Refusal::NoLauncher,    1, 0, false},
    {"explosion_origin",           Arm::Origin,         Hand::Rpg,     Mode::Plain,    1,   0,  {0, 0, 0}, Refusal::OriginBound,   1, 0, false},
    {"explosion_reach",            Arm::Reach,          Hand::Rpg,     Mode::Plain,    2,   1,  {0, 0, 0}, Refusal::Reach,         1, 0, false},
    {"explosion_not_seated",       Arm::NotSeated,      Hand::Empty,   Mode::Plain,    1,   0,  {0, 0, 0}, Refusal::NotSeated,     1, 0, false},
    {"explosion_no_explosive",     Arm::NoExplosive,    Hand::Empty,   Mode::Plain,    1,   0,  {0, 0, 0}, Refusal::NoExplosive,   1, 0, false},
    {"explosion_hand_bound",       Arm::HandBound,      Hand::Grenade, Mode::Plain,    1,   0,  {0, 0, 0}, Refusal::HandBound,     1, 0, false},
    {"explosion_story",            Arm::Story,          Hand::Rpg,     Mode::Story,    1,   0,  {0, 0, 0}, Refusal::FungunRule,    1, 0, false},
    {"explosion_wait_behind_shot", Arm::WaitBehindShot, Hand::Rpg,     Mode::Plain,    41,  40, {1, 0, 0}, kNone,                  0, 0, false},
    {"explosion_not_ready",        Arm::NotReady,       Hand::Empty,   Mode::NotReady, 1,   0,  {0, 0, 0}, Refusal::NotWorldReady, 1, 0, false},
    {"explosion_evicted",          Arm::Evicted,        Hand::Sticky,  Mode::Plain,    65,  65, {0, 0, 0}, kNone,                  0, 1, false},
    {"explosion_flood",            Arm::Flood,          Hand::Rpg,     Mode::Flood,    100, 0,  {0, 0, 0}, Refusal::QueueFull,     0, 0, false},
    {"explosion_rejoin",           Arm::Rejoin,         Hand::Sticky,  Mode::Rejoin,   2,   1,  {0, 0, 0}, Refusal::UnknownShot,   1, 0, false},
    {"explosion_self_clamp",       Arm::SelfClamp,      Hand::Empty,   Mode::Plain,    1,   0,  {0, 0, 1}, kNone,                  0, 0, true},
};

bool g_holdSet = false;

}  // namespace

const Spec* SpecOf(Arm a) {
    for (const Spec& s : kSpecs)
        if (s.arm == a) return &s;
    return nullptr;
}

void Verdict(Arm a, const char* what, const std::string& why) {
    if (std::strcmp(what, "PASS") == 0) UE_LOGI("[HOSTILE-DRILL] %s PASS -- %s", ArmName(a), why.c_str());
    else UE_LOGW("[HOSTILE-DRILL] %s %s -- %s", ArmName(a), what, why.c_str());
}

// Lines no save can hold, as a prop class or a place could.
std::string ReadyLine(Arm a, int life) {
    return std::string("hostile-drill ") + ArmName(a) + " host-ready life-" + std::to_string(life);
}
std::string TakenLine(Arm a) { return std::string("hostile-drill ") + ArmName(a) + " shot-taken"; }

void Say(const std::string& line) { coop::chat_sync::QueueSend(line); }

bool ChatHasLive(const std::string& needle) {
    bool found = false;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (!r.retained && r.line.find(needle) != std::string_view::npos) found = true;
    });
    return found;
}

bool PollDue() {
    static uint32_t sTicks = 0;
    return sTicks++ % 15 == 0;
}

const wchar_t* HandClass(Hand h) {
    switch (h) {
    case Hand::Rpg:     return L"prop_funGun_rpg_C";
    case Hand::Sticky:  return L"prop_funGun_stickyLauncher_C";
    case Hand::Grenade: return L"prop_grenade_C";
    default:            return nullptr;
    }
}

}  // namespace detail

Arm ArmOf(const std::string& value) {
    for (const detail::Spec& s : detail::kSpecs)
        if (value == s.name) return s.arm;
    return Arm::None;
}

const char* ArmName(Arm a) {
    const detail::Spec* s = detail::SpecOf(a);
    return s ? s->name : "off";
}

void Install(coop::net::Session* s, Arm a) {
    if (a != Arm::NotReady || detail::g_holdSet || !s || s->role() != coop::net::Role::Client) return;
    detail::g_holdSet = true;
    coop::dev::announce_hold::Set(true);
    UE_LOGI("[HOSTILE-DRILL] %s holds this client's announce until its shot is sent", ArmName(a));
}

void TickClient(coop::net::Session* s, Arm a) {
    if (const detail::Spec* sp = detail::SpecOf(a)) detail::ClientTick(s, *sp);
}

void TickHostJudge(coop::net::Session* s, Arm a) {
    if (const detail::Spec* sp = detail::SpecOf(a)) detail::HostTick(s, *sp);
}

void OnDisconnect() {
    detail::ClientReset();
    detail::HostReset();
    if (detail::g_holdSet) {
        detail::g_holdSet = false;
        coop::dev::announce_hold::Set(false);
    }
}

}  // namespace coop::dev::hostile_drill::explosion
