// coop/config/config_session.cpp -- the SESSION layer: the session's value of every server-scope
// row, above the runtime layer while a session runs, on the host and on a client alike. Filled at
// session start (the host's own values) or from the wire (a client); emptied at session end.
//
// Two divergences from Source, whose replicated cvars this layer answers to. (1) A client may still
// set its own hosting default: Source takes a change of a replicated cvar only from the server's
// console (reference/source-sdk-2013/src/public/tier1/iconvar.h:60); here a client's SetValue of a server-scope row writes the install's own
// value and never the session's. (2) Leaving restores the install's own value, not the default:
// the SDK sets a client's saved replicated cvars back to their DEFAULT at a level's end
// (reference/source-sdk-2013/src/game/shared/gamerules.cpp:659-663, 914-927; reference/source-sdk-2013/src/game/client/tf/vgui/tf_training_ui.cpp:2032), while this layer is emptied and the
// install's value answers again, as MTA's client keeps the server's synced settings apart from its
// own config (reference/mtasa-blue/Client/mods/deathmatch/logic/CPacketHandler.cpp:5491-5541, Packet_SyncSettings).

#include "coop/config/config.h"

#include "config_internal.h"
#include "coop/config/config_registry.h"
#include "ue_wrap/core/log.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace coop::config {

namespace {

using Row = config_registry::Row;

// The layer is a map under the runtime layer's mutex (config_runtime.cpp). The lock order is
// unchanged: the set lock, then the layer lock; the ini is never read under the layer lock, and a
// notification runs after every lock is released. Nothing here is a setting changed: a put is a
// session opened or a value arrived, so it logs its own line and never goes through SetValue.
// Guarded by internal::LayerMutex().
std::unordered_map<const Row*, std::string> g_sessionLayer;
// 0 none, 1 host, 2 client. Written under the layer lock; read lock-free for the fast "no session"
// answer.
std::atomic<int> g_sessionRole{0};
// The notify rows whose resolved value a host-side write changed since the last take, in the order
// first marked. Guarded by internal::LayerMutex(); cleared by a session's begin and end.
std::vector<const Row*> g_pendingAnnounce;

// A value as the logs show it: through the registry's one printed form, after internal::Printable,
// since the host chose its bytes.
std::string Shown(const Row* row, const std::string& v) {
    return config_registry::ValueForLog(row, internal::Printable(v));
}

}  // namespace

namespace internal {

int SessionRole() { return g_sessionRole.load(std::memory_order_acquire); }

bool SessionLayerGet(const Row* row, std::string& raw) {
    if (g_sessionRole.load(std::memory_order_acquire) == 0) return false;
    // Only server-scope rows are ever held, so any other row answers without the lock.
    if (!config_registry::IsServerScope(row)) return false;
    std::lock_guard<std::mutex> lk(LayerMutex());
    const auto it = g_sessionLayer.find(row);
    if (it == g_sessionLayer.end()) return false;
    raw = it->second;
    return true;
}

void SessionLayerPutNoNotify(const Row* row, const std::string& raw) {
    std::lock_guard<std::mutex> lk(LayerMutex());
    // A row absent from the map is not marked: SessionLayerBegin fills every server-scope row
    // before the session's role is set, so a host-side put never misses. The old value is read
    // from the map, never through RawBelowSession, which reads the ini and the ini is never read
    // under the layer lock. ShouldAnnounce is pure.
    const auto it = g_sessionLayer.find(row);
    if (it != g_sessionLayer.end() && ShouldAnnounce(*row, it->second, raw) &&
        std::find(g_pendingAnnounce.begin(), g_pendingAnnounce.end(), row) ==
            g_pendingAnnounce.end())
        g_pendingAnnounce.push_back(row);
    g_sessionLayer[row] = raw;
}

std::string SessionSafe(const Row* row, const std::string& raw, const char* why) {
    const bool tooLong = config_registry::IsReplicated(row) &&
                         raw.size() > config_registry::kServerSettingTextMax;
    if (!tooLong && ValueValidForKey(row->key, raw, nullptr)) return raw;
    // The stored value itself is not printed: it may be a credential's.
    const std::string def = DefaultText(*row);
    UE_LOGW("config: SESSION %s=%s (%s; the stored value was not valid or too long)", row->key,
            Shown(row, def).c_str(), why);
    return def;
}

}  // namespace internal

void SessionLayerBegin(bool host) {
    std::vector<std::pair<const Row*, std::string>> fresh;
    std::vector<const Row*> stale;
    {
        // Held across the reads below and the put, so a host-side SetValue lands wholly before
        // or wholly after the session opens.
        std::lock_guard<std::mutex> setLock(internal::SetMutex());
        if (host) {
            size_t count = 0;
            const Row* rows = config_registry::Rows(count);
            const std::wstring iniPath = internal::LiveIniPath();
            for (size_t i = 0; i < count; ++i) {
                const Row* row = &rows[i];
                if (!config_registry::IsServerScope(row)) continue;
                std::string raw;
                internal::RawBelowSession(iniPath, row, raw);
                fresh.emplace_back(row, internal::SessionSafe(row, raw, "host start"));
            }
        }
        std::lock_guard<std::mutex> layerLock(internal::LayerMutex());
        stale.reserve(g_sessionLayer.size());
        for (const auto& kv : g_sessionLayer) stale.push_back(kv.first);
        g_sessionLayer.clear();
        g_pendingAnnounce.clear();
        for (const auto& f : fresh) g_sessionLayer[f.first] = f.second;
        g_sessionRole.store(host ? 1 : 2, std::memory_order_release);
    }
    if (!stale.empty())
        UE_LOGW("config: SESSION dropped %u stale rows", static_cast<unsigned>(stale.size()));
    for (const auto& f : fresh)
        UE_LOGI("config: SESSION %s=%s (host start)", f.first->key,
                Shown(f.first, f.second).c_str());
    for (const auto& f : fresh) internal::PostNotify(f.first);
    // A dropped row the new layer does not hold again changed too; a fresh row is told above.
    for (const Row* row : stale) {
        bool refilled = false;
        for (const auto& f : fresh) refilled = refilled || f.first == row;
        if (!refilled) internal::PostNotify(row);
    }
}

bool SessionLayerPut(const Row* row, const std::string& text) {
    if (!ValueValidForKey(row->key, text, nullptr)) {
        UE_LOGW("config: SESSION %s REFUSED -- '%s' is not a valid value", row->key,
                Shown(row, text).c_str());
        return false;
    }
    bool held = false;
    {
        std::lock_guard<std::mutex> lk(internal::LayerMutex());
        if (g_sessionRole.load(std::memory_order_acquire) == 2) {
            g_sessionLayer[row] = text;
            held = true;
        }
    }
    if (!held) {
        UE_LOGW("config: SESSION %s ignored -- no client session is running", row->key);
        return false;
    }
    UE_LOGI("config: SESSION %s=%s (from the host)", row->key, Shown(row, text).c_str());
    internal::PostNotify(row);
    return true;
}

void SessionLayerEnd() {
    std::vector<const Row*> held;
    bool had = false;
    {
        // The set lock makes the clear final: no host put lands after it.
        std::lock_guard<std::mutex> setLock(internal::SetMutex());
        std::lock_guard<std::mutex> layerLock(internal::LayerMutex());
        had = g_sessionRole.load(std::memory_order_acquire) != 0 || !g_sessionLayer.empty();
        held.reserve(g_sessionLayer.size());
        for (const auto& kv : g_sessionLayer) held.push_back(kv.first);
        g_sessionLayer.clear();
        g_pendingAnnounce.clear();
        g_sessionRole.store(0, std::memory_order_release);
    }
    if (!had) return;
    UE_LOGI("config: SESSION cleared (%u rows)", static_cast<unsigned>(held.size()));
    for (const Row* row : held) internal::PostNotify(row);
}

// The accepted limit: a row changed and changed back between two takes (A -> B -> A) stays marked
// and announces its current value once.
std::vector<const Row*> TakePendingAnnouncements() {
    std::vector<const Row*> taken;
    std::lock_guard<std::mutex> lk(internal::LayerMutex());
    taken.swap(g_pendingAnnounce);
    return taken;
}

}  // namespace coop::config
