#include "coop/save/save_blob_codec.h"
#include "coop/save/slot_file_read.h"
#include <windows.h>
#include <compressapi.h>
#include <new>

namespace coop::save_blob_codec {
namespace {
struct Compressor {
    COMPRESSOR_HANDLE h = nullptr;
    ~Compressor() { if (h) CloseCompressor(h); }
};
struct Decompressor {
    DECOMPRESSOR_HANDLE h = nullptr;
    ~Decompressor() { if (h) CloseDecompressor(h); }
};
}

bool ValidHeader(const net::SaveTransferBeginPayload& p) {
    if (p.pad[0] || p.pad[1] || p.pad[2]) return false;
    if (!p.totalBytes)
        return !p.chunkCount && !p.sidecarBytes && !p.decodedBytes && !p.crc32;
    if (p.totalBytes > kMaxBlobBytes || p.decodedBytes > kMaxBlobBytes) return false;
    // A compressed transfer must actually save bytes. Zero means a raw stream.
    if (p.decodedBytes && p.decodedBytes <= p.totalBytes) return false;
    const uint32_t plainBytes = p.decodedBytes ? p.decodedBytes : p.totalBytes;
    return p.sidecarBytes < plainBytes &&
        p.chunkCount == (p.totalBytes + net::kSaveChunkBytes - 1) / net::kSaveChunkBytes;
}

bool CompressIfSmaller(std::vector<uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > kMaxBlobBytes) return false;
    Compressor c;
    if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &c.h)) return false;
    try {
        // The size query returns a worst-case bound, not the compressed size.
        // A raw-sized destination is enough for every result worth sending;
        // incompressible output simply fails or does not pass the size check.
        std::vector<uint8_t> out(bytes.size());
        SIZE_T written = 0;
        if (!Compress(c.h, bytes.data(), bytes.size(), out.data(), out.size(), &written) ||
            !written || written > out.size() || written >= bytes.size()) return false;
        out.resize(written);
        bytes.swap(out);
        return true;
    } catch (const std::bad_alloc&) { return false; }
}

bool DecodeAndVerify(std::vector<uint8_t>& bytes, uint32_t decodedBytes, uint32_t crc) {
    if (bytes.empty() || bytes.size() > kMaxBlobBytes || decodedBytes > kMaxBlobBytes) return false;
    if (!decodedBytes) return slot_file_read::Crc32(bytes.data(), bytes.size()) == crc;
    if (decodedBytes <= bytes.size()) return false;
    Decompressor d;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &d.h)) return false;
    SIZE_T required = 0;
    if (Decompress(d.h, bytes.data(), bytes.size(), nullptr, 0, &required) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER || required != decodedBytes) return false;
    try {
        std::vector<uint8_t> out(decodedBytes);
        SIZE_T written = 0;
        if (!Decompress(d.h, bytes.data(), bytes.size(), out.data(), out.size(), &written) ||
            written != decodedBytes || slot_file_read::Crc32(out.data(), out.size()) != crc) return false;
        bytes.swap(out);
        return true;
    } catch (const std::bad_alloc&) { return false; }
}
}
