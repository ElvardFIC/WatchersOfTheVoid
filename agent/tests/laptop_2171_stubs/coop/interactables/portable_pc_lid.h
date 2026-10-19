#pragma once

#include "coop/net/protocol.h"

#include <cstdint>

namespace coop::portable_pc_lid {
void OnLid(const coop::net::LaptopStatePayload& payload, uint8_t senderSlot);
}
