#include "script_gate_detail.h"

namespace ue_wrap::script_gate::detail {

// Registration under the mutex: an idempotent re-register re-enables the same slot; a new pair
// takes the first empty slot of the probe chain. `enabled` false keeps an entry for a later re-register
// without firing it. Returns false when the chain is full.
bool Register(Entry* table, std::atomic<int>& count, int& keyed, std::uint64_t key, int tag,
              PreFn pre, PostFn post, const wchar_t* name, const wchar_t* className,
              std::uint64_t classKey, bool resolved, bool enabled, bool unjudged) {
    for (int i = SlotOf(key), n = 0; n < kSlots; ++n, i = (i + 1) & (kSlots - 1)) {
        Entry& e = table[i];
        const std::uint64_t k = e.key.load(std::memory_order_relaxed);
        // A dead entry keeps its literals for the settled question and is never enabled again: a re-watch of it
        // takes a slot of its own, which dies the same way.
        if (k == key && e.tag == tag && e.pre == pre && e.post == post && e.name == name &&
            e.className == className && !e.dead.load(std::memory_order_relaxed)) {
            if (enabled && !e.enabled.exchange(true, std::memory_order_release))
                count.fetch_add(1, std::memory_order_release);
            return true;
        }
        if (k != 0) continue;
        if (keyed >= kMaxKeyed) return false;
        e.tag = tag; e.pre = pre; e.post = post; e.name = name;
        e.className = className; e.classKey = classKey;
        e.resolved.store(resolved, std::memory_order_relaxed);
        e.enabled.store(enabled, std::memory_order_relaxed);
        e.unjudged.store(unjudged, std::memory_order_relaxed);
        e.key.store(key, std::memory_order_release);   // published last: the reader sees a whole entry
        ++keyed;
        if (enabled) count.fetch_add(1, std::memory_order_release);
        if (unjudged) g_unjudged.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool Retire(Entry* table, std::atomic<int>& count, std::uint64_t key, int tag, PreFn pre,
            PostFn post, const wchar_t* name) {
    for (int i = SlotOf(key), n = 0; n < kSlots; ++n, i = (i + 1) & (kSlots - 1)) {
        Entry& e = table[i];
        const std::uint64_t k = e.key.load(std::memory_order_relaxed);
        if (k == 0) return false;
        if (k == key && e.tag == tag && e.pre == pre && e.post == post && e.name == name) {
            if (e.enabled.exchange(false, std::memory_order_release)) count.fetch_sub(1, std::memory_order_release);
            return true;
        }
    }
    return false;
}


}  // namespace ue_wrap::script_gate::detail
