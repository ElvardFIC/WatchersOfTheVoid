#pragma once
#include "coop/net/protocol.h"
#include <cstring>

namespace coop::cord_wire {
inline bool SameName(const char* a, const char* b) {
    // FNames compare without case; display casing can depend on load order per peer.
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    while (*a && *b) {
        if (lower(*a++) != lower(*b++)) return false;
    }
    return *a == *b;
}
inline bool Valid(const net::CordEndpoint& e, bool allowEmpty = false) {
    if (e.key.len > sizeof(e.key.data) || e.kind > 3) return false;
    if (e.kind == 0) {
        const net::CordEndpoint empty{};
        return allowEmpty && std::memcmp(&e, &empty, sizeof(e)) == 0;
    }
    if (e.kind != 2 && !e.key.len && !e.elementId) return false;
    if (e.kind == 2 && (e.key.len || e.elementId)) return false;
    const char* end = static_cast<const char*>(std::memchr(e.name, 0, sizeof(e.name)));
    if (!end || ((e.kind == 2 || e.kind == 3) && end == e.name)) return false;
    // These are UE object/component identifiers, not text; no truncation or lossy encoding.
    for (const char* p = e.name; p != end; ++p)
        if (static_cast<unsigned char>(*p) < 32 || static_cast<unsigned char>(*p) > 126) return false;
    return true;
}
inline bool Same(const net::CordEndpoint& a, const net::CordEndpoint& b) {
    if (a.kind != b.kind) return false;
    if (!a.kind) return true;
    if (a.kind == 2) return SameName(a.name, b.name);
    const bool owner = a.key.len && b.key.len
        ? a.key.len == b.key.len && std::memcmp(a.key.data, b.key.data, a.key.len) == 0
        : a.elementId && a.elementId == b.elementId;
    return owner && (a.kind != 3 || SameName(a.name, b.name));
}
inline bool Valid(const net::CordStatePayload& p) {
    return Valid(p.cord) && Valid(p.socket, true) && p.powered <= 1 &&
        (p.socket.kind || !p.powered);
}
}
