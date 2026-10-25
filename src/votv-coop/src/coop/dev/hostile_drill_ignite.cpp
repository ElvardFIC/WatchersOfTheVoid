// coop/dev/hostile_drill_ignite.cpp -- see coop/dev/hostile_drill_ignite.h. The arm table, the verdict print, the
// lobby-chat lines and the family's entry points; the client's sender is hostile_drill_ignite_client.cpp and the
// host's setup and judge hostile_drill_ignite_host.cpp.

#include "coop/dev/hostile_drill_ignite.h"

#include "hostile_drill_ignite_detail.h"  // co-located private header (src tree, not include/)

#include "coop/dev/announce_hold.h"
#include "coop/net/session.h"

#include "ue_wrap/core/log.h"

#include <cstring>

namespace coop::dev::hostile_drill::ignite {
namespace detail {
namespace {

using Refusal = coop::salt_rocket_ignite::Refusal;
constexpr Refusal kNone = Refusal::Count;

// The arms: the client's hand, the messages it sends, then the host's expected deltas (the lights performed by verb,
// the one reason and its count).
const Spec kSpecs[] = {
    {"ignite_malformed",     Arm::Malformed,   Hand::Empty,   Mode::Plain,    4,  {0, 0}, Refusal::Malformed,     4},
    {"ignite_no_tool",       Arm::NoTool,      Hand::Empty,   Mode::Plain,    1,  {0, 0}, Refusal::NoTool,        1},
    {"ignite_torch_attempt", Arm::TorchAttempt, Hand::Torch, Mode::Plain,    1,  {0, 0}, Refusal::NoTool,        1},
    {"ignite_torch_ignite",  Arm::TorchIgnite, Hand::Torch,   Mode::Plain,    1,  {0, 1}, kNone,                  0},
    {"ignite_reach",         Arm::Reach,       Hand::Lighter, Mode::Plain,    1,  {0, 0}, Refusal::Reach,         1},
    {"ignite_not_rocket",    Arm::NotRocket,   Hand::Lighter, Mode::Plain,    1,  {0, 0}, Refusal::NotRocket,     1},
    {"ignite_not_ready",     Arm::NotReady,    Hand::Empty,   Mode::NotReady, 1,  {0, 0}, Refusal::NotWorldReady, 1},
    {"ignite_flood",         Arm::Flood,       Hand::Lighter, Mode::Flood,    40, {0, 0}, Refusal::QueueFull,     0},
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

// A line no save can hold, as a prop class or a place could.
std::string ReadyLine(Arm a) { return std::string("hostile-drill ") + ArmName(a) + " host-ready life-1"; }
std::string ClientReadyLine(Arm a) { return std::string("hostile-drill ") + ArmName(a) + " client-ready"; }

const wchar_t* HandClass(Hand h) {
    switch (h) {
    case Hand::Lighter: return kLighterCls;
    case Hand::Torch:   return kTorchCls;
    default:            return nullptr;
    }
}

const wchar_t* HandItem(Hand h) {
    switch (h) {
    case Hand::Lighter: return L"lighter";
    case Hand::Torch:   return L"torch";
    default:            return nullptr;
    }
}

const wchar_t* HandPath(Hand h) {
    switch (h) {
    case Hand::Lighter: return kLighterPath;
    case Hand::Torch:   return kTorchPath;
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
    UE_LOGI("[HOSTILE-DRILL] %s holds this client's announce until its light is sent", ArmName(a));
}

void TickHostJudge(coop::net::Session* s, Arm a) {
    if (const detail::Spec* sp = detail::SpecOf(a)) detail::HostTick(s, *sp);
}

void TickClient(coop::net::Session* s, Arm a) {
    if (const detail::Spec* sp = detail::SpecOf(a)) detail::ClientTick(s, *sp);
}

void OnDisconnect() {
    detail::ClientReset();
    detail::HostReset();
    if (detail::g_holdSet) {
        detail::g_holdSet = false;
        coop::dev::announce_hold::Set(false);
    }
}

}  // namespace coop::dev::hostile_drill::ignite
