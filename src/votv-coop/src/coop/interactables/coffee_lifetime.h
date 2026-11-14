#pragma once

#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/engine/world_identity.h"
#include <string>

namespace coop::coffee_lifetime {
struct Session {
    coop::net::Session* pointer = nullptr;
    uint32_t serial = 0, world = 0;
    uint64_t stopped = 0;
    coop::net::Role role = coop::net::Role::Host;
    static Session Capture(coop::net::Session* s) {
        return {s, coop::net::session_serial::Current(), ue_wrap::world_identity::Generation(),
                s ? s->StopCount() : 0, s ? s->role() : coop::net::Role::Host};
    }
    bool Same(coop::net::Session* installed) const {
        return pointer == installed && serial == coop::net::session_serial::Current() &&
            world == ue_wrap::world_identity::Generation() &&
            (!pointer || (stopped == pointer->StopCount() && role == pointer->role()));
    }
    bool Current(coop::net::Session* installed) const {
        return pointer && Same(installed) && pointer->running();
    }
};
struct Sender {
    Session session;
    uint8_t slot;
    uint32_t generation;
    static Sender Capture(coop::net::Session& s, uint8_t slot) {
        return {Session::Capture(&s), slot, s.peerGenerationForSlot(slot)};
    }
    bool Current(coop::net::Session* installed) const {
        return session.Current(installed) && session.role == coop::net::Role::Host &&
            slot > 0 && slot < coop::net::kMaxPeers && generation &&
            session.pointer->peerGenerationForSlot(slot) == generation &&
            session.pointer->IsSlotConnected(slot) && session.pointer->IsSlotWorldReady(slot);
    }
};
inline bool KeyOk(const std::wstring& key) {
    if (key.empty() || key == L"None" || key.size() > sizeof(coop::net::WireKey{}.data)) return false;
    for (wchar_t c : key) if (c <= 0 || c > 127) return false;
    return true;
}
inline bool KeyOk(const coop::net::WireKey& key) {
    if (!key.len || key.len > sizeof(key.data)) return false;
    std::wstring text;
    for (size_t i=0; i<key.len; ++i) text.push_back(static_cast<unsigned char>(key.data[i]));
    return KeyOk(text);
}
}
