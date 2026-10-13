// coop/save/slot_file_read.cpp -- see coop/save/slot_file_read.h.

#include "coop/save/slot_file_read.h"

#include "ue_wrap/core/log.h"

#include <windows.h>

#include <atomic>
#include <array>
#include <fstream>

namespace coop::slot_file_read {
namespace {

constexpr uint64_t kStablePollMs = 300;

}  // namespace

uint32_t Crc32(const uint8_t* data, size_t len) {
    static const auto table = [] {
        std::array<uint32_t, 256> values{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            values[i] = c;
        }
        return values;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

namespace {
bool ReadWholeStream_(std::istream& f, std::vector<uint8_t>& out) {
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n <= 0) return false;
    out.resize(static_cast<size_t>(n));
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data()), n);
    // The game rewrites saves in place. EOF here means the file shrank after
    // tellg(), not a complete read: never CRC/send a zero-filled unread tail.
    return f.good() && f.gcount() == n;
}
}  // namespace

bool ReadWholeFile(const std::filesystem::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    return ReadWholeStream_(f, out);
}

Poll PollStable(StableRead& st, const std::filesystem::path& file, int slot, std::vector<uint8_t>& out,
                uint32_t& crc) {
    namespace fs = std::filesystem;
    const uint64_t now = ::GetTickCount64();
    if (now - st.lastProbeTick < kStablePollMs) return Poll::Waiting;  // wait out the poll gap
    st.lastProbeTick = now;

    std::error_code ec;
    const uint64_t size = fs::file_size(file, ec);
    if (ec) return ++st.readAttempts >= 4 ? Poll::Unreadable : Poll::Waiting;
    const auto mtime = fs::last_write_time(file, ec).time_since_epoch().count();
    if (ec) return Poll::Waiting;

    if (size != st.lastSize || mtime != st.lastMtime) {
        // Changed since the last probe (the game may be mid-save): the stability count restarts,
        // which also covers the first probe.
        st.lastSize = size;
        st.lastMtime = mtime;
        st.stableCount = 1;
        st.haveFirstRead = false;
        return Poll::Waiting;
    }
    if (++st.stableCount < 3) return Poll::Waiting;  // need 2 stable gaps (3 identical probes)

    if (!ReadWholeFile(file, out) || out.empty()) return ++st.readAttempts >= 4 ? Poll::Unreadable : Poll::Waiting;
    crc = Crc32(out.data(), out.size());
    if (!st.haveFirstRead) {
        // The first full read keeps its CRC and the next read must match: the double read closes
        // the in-place-write window.
        st.haveFirstRead = true;
        st.firstReadCrc = crc;
        return Poll::Waiting;
    }
    if (crc != st.firstReadCrc) {
        UE_LOGW("save_transfer: slot %d double-read CRC mismatch (file changing) -- re-probing", slot);
        st.haveFirstRead = false;
        st.stableCount = 0;
        return Poll::Waiting;
    }
    return Poll::Ready;
}

}  // namespace coop::slot_file_read
