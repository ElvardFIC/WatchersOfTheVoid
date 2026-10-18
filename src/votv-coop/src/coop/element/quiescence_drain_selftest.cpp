// coop/element/quiescence_drain_selftest.cpp -- the un-gated selftest of the deferred-destroy retire rule, run
// once per session start: a destroy that names an element is retired by a spawn of that element only, a
// key-only destroy by a spawn of the same key, a "None" or empty key matches nothing. The rule is checked on
// stated inputs (quiescence_drain::internal::RetireMatches); the live table is never armed, so the session's
// own [DESTROY-DEFER] lines stay its own. States no honest run lines up (a destroy and its re-spawn racing
// inside one join bracket).

#include "coop/element/quiescence_drain.h"

#include "coop/net/protocol.h"
#include "ue_wrap/core/log.h"

#include <cstring>

namespace coop::element::quiescence_drain {

namespace {
coop::net::WireKey Key(const char* s) {
    coop::net::WireKey k{};
    k.len = static_cast<uint8_t>(std::strlen(s));
    std::memcpy(k.data, s, k.len);
    return k;
}

coop::net::PropDestroyPayload Destroy(const char* key, uint32_t eid) {
    coop::net::PropDestroyPayload p{};
    p.key = Key(key);
    p.elementId = eid;
    return p;
}

coop::net::PropSpawnPayload Spawn(const char* key, uint32_t eid) {
    coop::net::PropSpawnPayload p{};
    p.key = Key(key);
    p.elementId = eid;
    return p;
}
}  // namespace

void RunDestroyDeferSelftest() {
    int pass = 0, total = 0;
    const char* failed = nullptr;
    auto check = [&](bool ok, const char* what) {
        ++total;
        if (ok) { ++pass; return; }
        if (!failed) failed = what;
        UE_LOGE("destroy_defer selftest FAIL: %s", what);
    };
    using internal::RetireMatches;

    // A key-only destroy (eid 0) is retired by a spawn of the same key, whatever eid the spawn carries.
    check(RetireMatches(Destroy("selftest_K", 0), Spawn("selftest_K", 7)),
          "match: a spawn of the same key retires a key-only destroy");
    check(RetireMatches(Destroy("selftest_K", 0), Spawn("selftest_K", 0)),
          "match: a spawn of the same key with no eid retires a key-only destroy");
    check(!RetireMatches(Destroy("selftest_K", 0), Spawn("selftest_K2", 7)),
          "match: a spawn of another key keeps a key-only destroy");
    check(!RetireMatches(Destroy("selftest_K", 0), Spawn("selftest_", 7)),
          "match: a key that is a prefix of the destroy's keeps it");

    // A destroy that names an element is the death of that element: another incarnation carrying the same key
    // keeps it, a spawn of the same element retires it whatever key that spawn carries.
    check(!RetireMatches(Destroy("selftest_K", 11), Spawn("selftest_K", 12)),
          "match: the same key under another eid keeps an eid destroy");
    check(!RetireMatches(Destroy("selftest_K", 11), Spawn("selftest_K", 0)),
          "match: a spawn with no eid keeps an eid destroy");
    check(RetireMatches(Destroy("selftest_K", 11), Spawn("selftest_K2", 11)),
          "match: a spawn of the same eid retires an eid destroy");

    // An eid-only destroy (the trash clump, key None) matches by eid and never by its None key.
    check(!RetireMatches(Destroy("None", 21), Spawn("None", 22)),
          "match: a None key matches nothing, another eid keeps an eid-only destroy");
    check(RetireMatches(Destroy("None", 21), Spawn("None", 21)),
          "match: an eid-only destroy is retired by a spawn of its eid");

    // A None or empty key with no eid names no identity at all: no spawn retires it.
    check(!RetireMatches(Destroy("None", 0), Spawn("None", 0)), "match: a None key with no eid never matches");
    check(!RetireMatches(Destroy("None", 0), Spawn("selftest_K", 0)),
          "match: a None destroy is not retired by a keyed spawn");
    check(!RetireMatches(Destroy("selftest_K", 0), Spawn("None", 0)),
          "match: a None spawn retires no key-only destroy");
    check(!RetireMatches(Destroy("", 0), Spawn("", 0)), "match: an empty key with no eid never matches");

    if (!failed) UE_LOGI("destroy_defer selftest: ALL PASS (%d checks)", total);
    else UE_LOGE("destroy_defer selftest FAIL: %s (%d of %d checks passed)", failed, pass, total);
}

}  // namespace coop::element::quiescence_drain
