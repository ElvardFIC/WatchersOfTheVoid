// coop/world/media_sync.cpp -- see coop/world/media_sync.h.

#include "coop/world/media_sync.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"

#include "ue_wrap/world/media_player.h"
#include "ue_wrap/core/walk_timer.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <unordered_map>

namespace coop::media_sync {
namespace {

std::atomic<coop::net::Session*> g_session{nullptr};
uint64_t g_nextMs = 0;
std::unordered_map<std::string, int64_t> g_sentTicks;

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

std::string Narrow(const std::wstring& w) {
    std::string s;
    s.reserve(w.size());
    for (wchar_t c : w) {
        if (c <= 0 || c >= 128) return {};
        s.push_back(static_cast<char>(c));
    }
    return s;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;
    const uint64_t now = NowMs();
    if (now < g_nextMs) return;
    g_nextMs = now + 1000;
    ue_wrap::media_player::Playing rows[4];
    const int n = ue_wrap::media_player::CopyPlaying(rows, 4);
    std::unordered_map<std::string, int64_t> still;
    for (int i = 0; i < n; ++i) {
        const std::string url = Narrow(rows[i].url);
        if (url.empty() || url.size() >= 160) continue;
        const auto prev = g_sentTicks.find(url);
        const int64_t last = prev == g_sentTicks.end() ? rows[i].ticks : prev->second;
        const int64_t drift = rows[i].ticks - last;
        const bool due = prev == g_sentTicks.end() || drift >= 20000000 || drift <= -20000000;
        if (due) {
            coop::net::MediaStatePayload p{};
            std::memcpy(p.url, url.c_str(), url.size());
            p.ticks = rows[i].ticks;
            p.playing = 1;
            if (s->SendReliable(coop::net::ReliableKind::MediaState, &p, sizeof(p)))
                still[url] = rows[i].ticks;
            else if (prev != g_sentTicks.end())
                still[url] = prev->second;
        } else {
            still[url] = last;
        }
    }
    for (const auto& kv : g_sentTicks) {
        if (still.count(kv.first)) continue;
        coop::net::MediaStatePayload p{};
        std::memcpy(p.url, kv.first.c_str(), kv.first.size());
        p.playing = 0;
        if (s->SendReliable(coop::net::ReliableKind::MediaState, &p, sizeof(p))) continue;
        still[kv.first] = kv.second;  // the stop did not leave; try again next tick
    }
    g_sentTicks = std::move(still);
}

void OnReliable(const coop::net::MediaStatePayload& payload) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Client) return;
    ue_wrap::ScopedWalkTimer timer{"sync:media_apply"};
    char url[sizeof(payload.url)];
    std::memcpy(url, payload.url, sizeof(url));
    url[sizeof(url) - 1] = 0;
    if (!url[0]) return;
    std::wstring w;
    for (const char* c = url; *c; ++c) w.push_back(static_cast<wchar_t>(*c));
    ue_wrap::media_player::Apply(w.c_str(), payload.ticks, payload.playing != 0);
}

void OnDisconnect() {
    g_sentTicks.clear();
    g_nextMs = 0;
    g_session.store(nullptr, std::memory_order_release);
}

}  // namespace coop::media_sync
