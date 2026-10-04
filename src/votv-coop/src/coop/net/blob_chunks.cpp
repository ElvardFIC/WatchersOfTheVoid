// coop/net/blob_chunks.cpp -- see coop/net/blob_chunks.h.

#include "coop/net/blob_chunks.h"

#include "coop/net/session.h"

#include "ue_wrap/core/log.h"

#include <cstring>

namespace coop::blob_chunks {

// W4 anti-flood: concurrent half-assemblies allowed from ONE sender, per Assembler instance.
// Legitimate use is one blob at a time per lane (the sender chunks a blob and finishes it before
// starting the next), so 4 is generous; the TTL sweep reclaims anything abandoned.
constexpr size_t kMaxAssembliesPerSender = 4;

namespace {
// Shared chunker for SendBlob / SendBlobToSlot. `sendOne(p)` does the per-chunk transport
// (broadcast vs slot-targeted); everything else (chunk count, the kMaxChunks cap, all-or-nothing) is
// identical (RULE 2 -- one chunking implementation).
template <class SendOne>
bool ChunkAndSend(coop::net::ReliableKind kind, uint32_t seq,
                  const std::vector<uint8_t>& blob, SendOne sendOne) {
    const size_t maxData = sizeof(coop::net::BlobChunkPayload{}.data);
    if (blob.size() > MaxBlobBytes()) {
        UE_LOGW("blob_chunks: blob of %zu bytes exceeds the %zu-chunk cap (kind %u) -- dropped",
                blob.size(), kMaxChunks, static_cast<unsigned>(kind));
        return false;
    }
    const uint16_t chunks =
        static_cast<uint16_t>(blob.empty() ? 1 : (blob.size() + maxData - 1) / maxData);
    size_t off = 0;
    for (uint16_t i = 0; i < chunks; ++i) {
        coop::net::BlobChunkPayload p{};
        p.blobSeq = seq;
        p.chunkIdx = i;
        p.chunks = chunks;
        const size_t take = blob.size() - off > maxData ? maxData : blob.size() - off;
        p.chunkLen = static_cast<uint16_t>(take);
        if (take) std::memcpy(p.data, blob.data() + off, take);
        off += take;
        if (!sendOne(p)) {
            UE_LOGW("blob_chunks: chunk %u/%u send refused (kind %u, seq=%u) -- "
                    "blob will retry under a fresh seq",
                    static_cast<unsigned>(i), static_cast<unsigned>(chunks),
                    static_cast<unsigned>(kind), seq);
            return false;
        }
    }
    return true;
}
}  // namespace

bool SendBlob(coop::net::Session* s, coop::net::ReliableKind kind, uint32_t seq,
              const std::vector<uint8_t>& blob) {
    return ChunkAndSend(kind, seq, blob,
                        [&](const coop::net::BlobChunkPayload& p) {
                            return s->SendReliable(kind, &p, sizeof(p));
                        });
}

bool SendBlobToSlot(coop::net::Session* s, int peerSlot, coop::net::ReliableKind kind,
                    uint32_t seq, const std::vector<uint8_t>& blob) {
    return ChunkAndSend(kind, seq, blob,
                        [&](const coop::net::BlobChunkPayload& p) {
                            return s->SendReliableToSlot(peerSlot, kind, &p, sizeof(p));
                        });
}

bool Assembler::OnChunk(const coop::net::BlobChunkPayload& p, uint8_t senderSlot,
                        std::vector<uint8_t>& outBlob) {
    if (p.chunks == 0 || p.chunks > kMaxChunks || p.chunkIdx >= p.chunks ||
        p.chunkLen > sizeof(p.data))
        return false;
    // The first chunk names the whole blob's size, unproven; reserve only a bounded head of it and
    // let the rest grow as chunks actually arrive.
    const size_t reserveChunks = p.chunks < 64 ? p.chunks : 64;

    const auto now = std::chrono::steady_clock::now();
    const auto key = std::make_pair(senderSlot, p.blobSeq);
    // Anti-flood cap. blobSeq is ATTACKER-CHOSEN, and `map_[key]` below default-inserts, so before
    // this cap one 228-byte packet bought a fresh assembly that reserved its whole claimed size --
    // about 246x amplification, on eight lanes, reachable client-to-host with no join or role
    // gate. Only the size of ONE assembly was bounded (now by kMaxChunks, and its reservation by
    // reserveChunks); their NUMBER was not.
    // order_sync caps its own table the same way -- this is that guard at the shared primitive, so
    // every lane gets it at once instead of eight bespoke patches.
    //
    // PER-SENDER, not global (unlike order_sync): a global table would let one flooding peer starve
    // every other peer's assemblies.
    if (map_.find(key) == map_.end()) {
        size_t fromSender = 0;
        for (const auto& kv : map_)
            if (kv.first.first == senderSlot) ++fromSender;
        if (fromSender >= kMaxAssembliesPerSender) {
            UE_LOGW("blob_chunks: sender %u has %zu concurrent assemblies (cap %zu) -- dropping new "
                    "blobSeq=%u", static_cast<unsigned>(senderSlot), fromSender,
                    kMaxAssembliesPerSender, p.blobSeq);
            return false;
        }
    }
    auto& a = map_[key];
    if (a.expectChunks == 0) {
        a.expectChunks = p.chunks;
        a.started = now;
        a.blob.reserve(reserveChunks * sizeof(p.data));
    }
    if (a.expectChunks != p.chunks || a.gotChunks != p.chunkIdx) {
        // Mismatched part: the RESTART semantics (see header).
        map_.erase(key);
        if (p.chunkIdx != 0) return false;
        auto& fresh = map_[key];
        fresh.expectChunks = p.chunks;
        fresh.started = now;
        fresh.blob.reserve(reserveChunks * sizeof(p.data));
        fresh.blob.insert(fresh.blob.end(), p.data, p.data + p.chunkLen);
        fresh.gotChunks = 1;
        if (fresh.gotChunks == fresh.expectChunks) {
            outBlob = std::move(fresh.blob);
            map_.erase(key);
            return true;
        }
        return false;
    }
    a.blob.insert(a.blob.end(), p.data, p.data + p.chunkLen);
    ++a.gotChunks;
    if (a.gotChunks < a.expectChunks) return false;

    outBlob = std::move(a.blob);
    map_.erase(key);
    return true;
}

void Assembler::Sweep(std::chrono::steady_clock::time_point now, std::chrono::seconds ttl) {
    for (auto it = map_.begin(); it != map_.end();) {
        if (now - it->second.started > ttl) it = map_.erase(it);
        else ++it;
    }
}

void Assembler::Clear() { map_.clear(); }

void Assembler::ClearSlot(uint8_t senderSlot) {
    // Slot teardown is a ROW TRANSITION: slots recycle lowest-free with no observable absence, so
    // person Y's chunks must never merge into person X's half-assembly under the same slot id. The
    // 20 s TTL alone leaves that window open, so callers hook this into the per-slot disconnect
    // fan-out.
    for (auto it = map_.begin(); it != map_.end();) {
        if (it->first.first == senderSlot) it = map_.erase(it);
        else ++it;
    }
}

}  // namespace coop::blob_chunks
