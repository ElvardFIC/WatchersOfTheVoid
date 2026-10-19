#pragma once
#include "coop/net/protocol.h"
#include <cstdint>
#include <vector>

namespace coop::save_blob_codec {
// Bound both the wire and decoded allocation before trusting a peer's header.
inline constexpr uint32_t kMaxBlobBytes = 512u * 1024u * 1024u;
bool ValidHeader(const net::SaveTransferBeginPayload& p);
// False leaves input unchanged: incompressible saves and OS failures use raw transport.
bool CompressIfSmaller(std::vector<uint8_t>& bytes);
// decodedBytes=0 is raw. Verify the original framed [sidecar][save] CRC before
// exposing decoded bytes to the file writer. Failure leaves bytes unchanged.
bool DecodeAndVerify(std::vector<uint8_t>& bytes, uint32_t decodedBytes, uint32_t crc);
}
