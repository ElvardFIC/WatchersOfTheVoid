#pragma once
#include "coop/net/protocol.h"
#include <array>
namespace coop::roster_ledger {
struct Row {uint16_t playerNo=0;};
inline std::array<Row,net::kMaxPeers> rows;
inline const Row& Get(int slot){return rows[slot];}
}
