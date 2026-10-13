// coop/save/slot_file_read.h -- a save slot's file, read whole only once it holds still. VOTV's saveToSlot
// writes the .sav in place, with no rename, so a join landing mid-write would read a torn blob: the file
// is trusted only when its size and time hold across two polls 300 ms apart and two consecutive full
// reads are CRC-identical (the torn-read guard). The CRC is the save stream's, both sides of the wire.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace coop::slot_file_read {

// CRC-32 (IEEE, table-driven).
uint32_t Crc32(const uint8_t* data, size_t len);

// The whole file into `out`; false when it cannot be opened, is empty or the
// read is incomplete. On failure the caller must not use `out`.
bool ReadWholeFile(const std::filesystem::path& p, std::vector<uint8_t>& out);

// One file's read in progress, polled until it holds still.
struct StableRead {
    uint64_t lastSize = 0;
    int64_t  lastMtime = 0;
    uint64_t lastProbeTick = 0;  // GetTickCount64 of the last probe
    int      stableCount = 0;
    uint32_t firstReadCrc = 0;
    bool     haveFirstRead = false;
    int      readAttempts = 0;
};

// Poll `file`, at most once per 300 ms: Ready with its bytes and their CRC once it held still and two
// full reads agreed; Unreadable after four failed attempts (no file, or a failed read); Waiting otherwise.
// `slot` names the log line. Game thread.
enum class Poll : uint8_t { Waiting, Ready, Unreadable };
Poll PollStable(StableRead& st, const std::filesystem::path& file, int slot, std::vector<uint8_t>& out,
                uint32_t& crc);

}  // namespace coop::slot_file_read
