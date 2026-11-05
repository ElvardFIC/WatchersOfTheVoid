#include "coop/save/save_blob_codec.h"
#include "coop/save/slot_file_read.h"
#include "miniz.h"
#include <new>

namespace coop::save_blob_codec {

bool ValidHeader(const net::SaveTransferBeginPayload& p) {
    if (p.pad[0] || p.pad[1] || p.pad[2]) return false;
    if (!p.totalBytes)
        return !p.chunkCount && !p.sidecarBytes && !p.decodedBytes && !p.crc32;
    if (p.totalBytes > kMaxBlobBytes || p.decodedBytes > kMaxBlobBytes) return false;
    if (p.decodedBytes && p.decodedBytes <= p.totalBytes) return false;
    const uint32_t plainBytes = p.decodedBytes ? p.decodedBytes : p.totalBytes;
    return p.sidecarBytes < plainBytes &&
        p.chunkCount == (p.totalBytes + net::kSaveChunkBytes - 1) / net::kSaveChunkBytes;
}

bool CompressIfSmaller(std::vector<uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > kMaxBlobBytes) return false;
    try {
        std::vector<uint8_t> out(bytes.size());
        mz_ulong written = static_cast<mz_ulong>(out.size());
        const int rc = mz_compress2(out.data(), &written, bytes.data(),
                                    static_cast<mz_ulong>(bytes.size()), MZ_BEST_SPEED);
        if (rc != MZ_OK || !written || written >= bytes.size()) return false;
        out.resize(static_cast<size_t>(written));
        bytes.swap(out);
        return true;
    } catch (const std::bad_alloc&) { return false; }
}

bool DecodeAndVerify(std::vector<uint8_t>& bytes, uint32_t decodedBytes, uint32_t crc) {
    if (bytes.empty() || bytes.size() > kMaxBlobBytes || decodedBytes > kMaxBlobBytes) return false;
    if (!decodedBytes) return slot_file_read::Crc32(bytes.data(), bytes.size()) == crc;
    if (decodedBytes <= bytes.size()) return false;
    try {
        std::vector<uint8_t> out(decodedBytes);
        mz_ulong written = static_cast<mz_ulong>(out.size());
        if (mz_uncompress(out.data(), &written, bytes.data(),
                          static_cast<mz_ulong>(bytes.size())) != MZ_OK ||
            written != decodedBytes ||
            slot_file_read::Crc32(out.data(), out.size()) != crc) return false;
        bytes.swap(out);
        return true;
    } catch (const std::bad_alloc&) { return false; }
}
}
