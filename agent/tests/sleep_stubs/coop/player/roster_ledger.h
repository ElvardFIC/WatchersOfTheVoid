#pragma once
#include <array>
#include <string>
#include "coop/net/protocol.h"
namespace coop::roster_ledger {
template<class T> struct PerSlotState {
    std::array<T, coop::net::kMaxPeers> rows{};
    T& operator[](int slot) { return rows.at(slot); }
};
inline const std::wstring& DisplayName(int) {static const std::wstring name=L"Host";return name;}
}
