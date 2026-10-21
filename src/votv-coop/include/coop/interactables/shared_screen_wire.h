#pragma once
#include "ue_wrap/desk/shared_screen.h"
#include <cmath>
#include <cstring>
#include <set>
namespace coop::shared_screen_wire {
using ue_wrap::shared_screen::Row;
inline constexpr size_t kMaxBytes = 128 * 1024;
inline constexpr size_t kEnvelopeBytes = 10;
template<class T> void Put(std::vector<uint8_t>& bytes, T value) {
    const auto* p = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), p, p + sizeof(T));
}
inline std::vector<uint8_t> Encode(const std::vector<Row>& rows) {
    if (rows.empty() || rows.size() > 512) return {};
    std::vector<uint8_t> bytes;
    Put<uint16_t>(bytes, static_cast<uint16_t>(rows.size()));
    for (const Row& row : rows) {
        if (row.path.empty() || row.path.size() > 128 || row.text.size() > 4096 ||
            row.visibility > 4 || row.flags > 7 || !std::isfinite(row.scroll)) return {};
        Put<uint8_t>(bytes, static_cast<uint8_t>(row.path.size()));
        bytes.insert(bytes.end(), row.path.begin(), row.path.end());
        Put(bytes, row.visibility); Put(bytes, row.flags); Put(bytes, row.tab); Put(bytes, row.scroll);
        Put<uint16_t>(bytes, static_cast<uint16_t>(row.text.size()));
        for (wchar_t ch : row.text) Put<uint16_t>(bytes, static_cast<uint16_t>(ch));
        if (bytes.size() > kMaxBytes) return {};
    }
    return bytes;
}
inline bool Decode(const std::vector<uint8_t>& bytes, std::vector<Row>& rows) {
    rows.clear();
    if (bytes.size() > kMaxBytes) return false;
    size_t at = 0;
    auto get = [&]<class T>(T& value) {
        if (sizeof(T) > bytes.size() - at) return false;
        std::memcpy(&value, bytes.data() + at, sizeof(T)); at += sizeof(T); return true;
    };
    uint16_t count = 0;
    if (!get(count) || count == 0 || count > 512) return false;
    std::set<std::string> paths;
    std::vector<Row> result;
    for (uint16_t i = 0; i < count; ++i) {
        Row row; uint8_t length = 0; uint16_t chars = 0;
        if (!get(length) || length == 0 || length > 128 || length > bytes.size() - at) return false;
        row.path.assign(reinterpret_cast<const char*>(bytes.data() + at), length); at += length;
        if (!paths.insert(row.path).second || row.path.find("pc") != 0 ||
            row.path.find_first_not_of("pc/r0123456789") != std::string::npos) return false;
        if (!get(row.visibility) || !get(row.flags) || !get(row.tab) || !get(row.scroll) ||
            row.visibility > 4 || row.flags > 7 || row.tab < -1 || row.tab > 512 ||
            !std::isfinite(row.scroll) || row.scroll < 0 || row.scroll > 1000000 ||
            !get(chars) || chars > 4096) return false;
        for (uint16_t n = 0; n < chars; ++n) {
            uint16_t ch = 0; if (!get(ch)) return false;
            row.text.push_back(static_cast<wchar_t>(ch));
        }
        result.push_back(std::move(row));
    }
    if (at != bytes.size()) return false;
    rows = std::move(result); return true;
}
struct Snapshot {
    bool canonical = false;
    uint32_t lease = 0, revision = 0;
    uint8_t owner = 0xff;
    std::vector<Row> rows;
};
inline std::vector<uint8_t> EncodeSnapshot(bool canonical, uint32_t lease, uint32_t revision,
                                         uint8_t owner, const std::vector<uint8_t>& rows) {
    if (!lease || !revision || rows.empty() || rows.size() > kMaxBytes) return {};
    std::vector<uint8_t> bytes;
    bytes.reserve(kEnvelopeBytes + rows.size());
    Put<uint8_t>(bytes, canonical ? 1 : 0); Put(bytes, lease); Put(bytes, revision); Put(bytes, owner);
    bytes.insert(bytes.end(), rows.begin(), rows.end());
    return bytes;
}
inline bool DecodeSnapshot(const std::vector<uint8_t>& bytes, Snapshot& out) {
    if (bytes.size() <= kEnvelopeBytes || bytes.size() > kMaxBytes + kEnvelopeBytes || bytes[0] > 1) return false;
    Snapshot result;
    result.canonical = bytes[0] != 0;
    std::memcpy(&result.lease, bytes.data() + 1, 4);
    std::memcpy(&result.revision, bytes.data() + 5, 4);
    result.owner = bytes[9];
    if (!result.lease || !result.revision ||
        !Decode({bytes.begin() + kEnvelopeBytes, bytes.end()}, result.rows)) return false;
    out = std::move(result); return true;
}
}
