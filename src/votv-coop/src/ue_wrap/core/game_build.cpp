// ue_wrap/core/game_build.cpp -- see ue_wrap/core/game_build.h.

#include "ue_wrap/core/game_build.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/sdk_profile.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>

namespace ue_wrap::game_build {
namespace {

constexpr Row kRows[] = {
#define GAME_BUILD(id, version, display, stamp, hash) {id, version, display, stamp, hash},
#include "ue_wrap/facts/game_builds.inc"
#undef GAME_BUILD
};

// FPakInfo as UE4.27 serializes it at the end of a pak (version 11): [EncryptionKeyGuid 16]
// [bEncryptedIndex 1][Magic 4][Version 4][IndexOffset 8][IndexSize 8][IndexHash 20][five compression
// method names, 32 bytes each]. Read from the end, so the magic sits 204 bytes before it and the hash
// 180. Both builds measured are version 11; another layout is refused with its reason.
constexpr size_t   kTailBytes = 256;
constexpr size_t   kMagicFromEnd = 204;
constexpr size_t   kVersionFromEnd = 200;
constexpr size_t   kHashFromEnd = 180;
constexpr uint32_t kPakMagic = 0x5A6F12E1;
constexpr uint32_t kPakVersion = 11;

uint32_t ReadU32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, sizeof v);
    return v;
}

// The pure half of the pak read: the hash out of a footer's last bytes.
bool ParseFooter(const uint8_t* tail, size_t n, uint8_t out[kPakHashBytes], const char** problem) {
    if (n < kMagicFromEnd) {
        *problem = "the main pak is too short to hold a footer";
        return false;
    }
    if (ReadU32(tail + n - kMagicFromEnd) != kPakMagic) {
        *problem = "the main pak has no footer where a version 11 pak keeps it";
        return false;
    }
    if (ReadU32(tail + n - kVersionFromEnd) != kPakVersion) {
        *problem = "the main pak is not a version 11 pak";
        return false;
    }
    std::memcpy(out, tail + n - kHashFromEnd, kPakHashBytes);
    return true;
}

// The exe's link stamp, from the header the loader mapped: no file read, and the same bytes the packed
// and the unpacked image of one build carry.
uint32_t ReadExeStamp() {
    const auto* base = reinterpret_cast<const uint8_t*>(::GetModuleHandleW(nullptr));
    if (!base) return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->FileHeader.TimeDateStamp;
}

// One attempt at the footer. `transient` says whether another attempt could read what this one did not:
// an I/O error can pass; a missing file or another layout is a fact of the install.
bool ReadPakHashOnce(uint8_t out[kPakHashBytes], const char** problem, bool* transient) {
    *transient = false;
    const std::wstring exeDir = paths::ExeDir();
    if (exeDir.empty()) {
        *problem = "the game folder could not be read";
        return false;
    }
    // <game>\VotV\Binaries\Win64 -> <game>\VotV\Content\Paks
    const std::filesystem::path pak = std::filesystem::path(exeDir).parent_path().parent_path() / L"Content" /
                                      L"Paks" / profile::kMainPakName;
    // Shared every way: the engine holds the pak open for the whole run.
    const HANDLE h = ::CreateFileW(pak.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            *problem = "the main pak is not where the game keeps it";
        } else {
            *problem = "the main pak could not be opened";
            *transient = true;
        }
        return false;
    }
    uint8_t tail[kTailBytes] = {};
    LARGE_INTEGER size{};
    DWORD got = 0;
    bool read = ::GetFileSizeEx(h, &size) && size.QuadPart >= static_cast<LONGLONG>(kTailBytes);
    if (read) {
        LARGE_INTEGER at{};
        at.QuadPart = size.QuadPart - static_cast<LONGLONG>(kTailBytes);
        read = ::SetFilePointerEx(h, at, nullptr, FILE_BEGIN) && ::ReadFile(h, tail, kTailBytes, &got, nullptr) &&
               got == kTailBytes;
    }
    ::CloseHandle(h);
    if (!read) {
        *problem = "the main pak's end could not be read";
        *transient = true;
        return false;
    }
    return ParseFooter(tail, kTailBytes, out, problem);
}

// The footer, with an I/O error retried twice 50 ms apart before it latches for the session: the read runs
// once, and a sharing hiccup must not leave a healthy install unknown (and refused at every peer's Join).
bool ReadPakHash(uint8_t out[kPakHashBytes], const char** problem) {
    bool transient = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0) ::Sleep(50);
        if (ReadPakHashOnce(out, problem, &transient)) return true;
        if (!transient) return false;
    }
    return false;
}

void ToHex(const uint8_t* bytes, size_t n, char* out) {
    static const char kHex[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = kHex[bytes[i] >> 4];
        out[2 * i + 1] = kHex[bytes[i] & 0xF];
    }
    out[2 * n] = '\0';
}

bool FromHex(const char* hex, uint8_t* out, size_t n) {
    if (!hex || std::strlen(hex) != 2 * n) return false;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    for (size_t i = 0; i < n; ++i) {
        const int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>(hi << 4 | lo);
    }
    return true;
}

void ComposeKey(Identity& id) {
    if (id.pakRead) {
        char hash[2 * kPakHashBytes + 1];
        ToHex(id.pakHash, kPakHashBytes, hash);
        std::snprintf(id.key, sizeof id.key, "%08x-%.16s", id.exeStamp, hash);
    } else {
        std::snprintf(id.key, sizeof id.key, "%08x-nopak", id.exeStamp);
    }
}

bool PakEquals(const Row& row, const uint8_t* hash) {
    uint8_t rowHash[kPakHashBytes];
    return FromHex(row.pakIndexHashHex, rowHash, kPakHashBytes) && std::memcmp(rowHash, hash, kPakHashBytes) == 0;
}

Identity g_running;
std::once_flag g_once;

}  // namespace

const Identity& Running() {
    std::call_once(g_once, [] {
        Identity id;
        id.exeStamp = ReadExeStamp();
        id.pakRead = ReadPakHash(id.pakHash, &id.pakProblem);
        ComposeKey(id);
        g_running = id;
        const Row* row = Match(g_running);
        char hash[2 * kPakHashBytes + 1] = "-";
        if (g_running.pakRead) ToHex(g_running.pakHash, kPakHashBytes, hash);
        UE_LOGI("game-build: exe stamp %08x, pak index %s -> %s%s%s", g_running.exeStamp, hash,
                row ? "VOTV " : "no known build", row ? row->display : "",
                g_running.pakRead ? "" : " (the pak was not read)");
        if (!g_running.pakRead) UE_LOGW("game-build: %s", g_running.pakProblem);
    });
    return g_running;
}

const Row* Rows(size_t* count) {
    *count = sizeof kRows / sizeof kRows[0];
    return kRows;
}

const Row* Match(const Identity& id) {
    if (!id.pakRead) return nullptr;
    for (const Row& row : kRows)
        if (row.exeStamp == id.exeStamp && PakEquals(row, id.pakHash)) return &row;
    return nullptr;
}

const Row* MatchExe(const Identity& id) {
    for (const Row& row : kRows)
        if (row.exeStamp == id.exeStamp) return &row;
    return nullptr;
}

const Row* MatchPak(const Identity& id) {
    if (!id.pakRead) return nullptr;
    for (const Row& row : kRows)
        if (PakEquals(row, id.pakHash)) return &row;
    return nullptr;
}

const Row* FindById(const char* id) {
    if (!id) return nullptr;
    for (const Row& row : kRows)
        if (std::strcmp(row.id, id) == 0) return &row;
    return nullptr;
}

const Row* FindByKey(const char* key) {
    if (!key) return nullptr;
    for (const Row& row : kRows)
        if (std::strcmp(IdentityOf(row).key, key) == 0) return &row;
    return nullptr;
}

const Row* Target() { return FindById(profile::kTargetBuild); }

int OrderOf(const Row* row) {
    if (!row) return -1;
    return static_cast<int>(row - kRows);
}

Identity IdentityOf(const Row& row) {
    Identity id;
    id.exeStamp = row.exeStamp;
    id.pakRead = FromHex(row.pakIndexHashHex, id.pakHash, kPakHashBytes);
    if (!id.pakRead) id.pakProblem = "the row's pak hash is not 40 lowercase hex";
    ComposeKey(id);
    return id;
}

Identity UnknownIdentity() {
    Identity id;
    id.pakRead = true;  // zero stamp and zero hash: a build no row names
    ComposeKey(id);
    return id;
}

bool RunSelftest() {
    int checks = 0, failed = 0;
    auto ok = [&](bool cond, const char* what) {
        ++checks;
        if (!cond) {
            ++failed;
            UE_LOGE("game-build selftest FAIL: %s", what);
        }
    };

    // A synthetic v11 footer: the parse finds the hash at its place, and refuses another magic or version.
    uint8_t tail[kTailBytes] = {};
    const uint32_t magic = kPakMagic, version = kPakVersion;
    std::memcpy(tail + kTailBytes - kMagicFromEnd, &magic, 4);
    std::memcpy(tail + kTailBytes - kVersionFromEnd, &version, 4);
    for (size_t i = 0; i < kPakHashBytes; ++i) tail[kTailBytes - kHashFromEnd + i] = static_cast<uint8_t>(0xA0 + i);
    uint8_t hash[kPakHashBytes] = {};
    const char* problem = nullptr;
    ok(ParseFooter(tail, kTailBytes, hash, &problem) && hash[0] == 0xA0 && hash[19] == 0xB3,
       "a version 11 footer gives its index hash");
    const uint32_t other = 10;
    std::memcpy(tail + kTailBytes - kVersionFromEnd, &other, 4);
    ok(!ParseFooter(tail, kTailBytes, hash, &problem) && problem, "another pak version is refused, with a reason");
    std::memcpy(tail + kTailBytes - kVersionFromEnd, &version, 4);
    tail[kTailBytes - kMagicFromEnd] ^= 0xFF;
    ok(!ParseFooter(tail, kTailBytes, hash, &problem) && problem, "a footer without the magic is refused");

    // The rows: well formed, the profile's target among them, and every row's key finds that row.
    size_t n = 0;
    const Row* rows = Rows(&n);
    ok(Target() != nullptr, "the profile's kTargetBuild names a row");
    ok(Target() && std::strcmp(Target()->version, profile::kTargetBuild) == 0,
       "the target row is a public release: its version is its id");
    for (size_t i = 0; i < n; ++i) {
        const Identity id = IdentityOf(rows[i]);
        ok(id.pakRead, "a row's pak hash is 40 lowercase hex");
        ok(Match(id) == &rows[i] && FindByKey(id.key) == &rows[i], "a row's own identity matches that row");
    }
    // The key's form, on the measured 0.9.0n values.
    if (const Row* r = FindById("0.9.0n"))
        ok(std::strcmp(IdentityOf(*r).key, "6a08597d-56065b874f3bfc68") == 0,
           "the key is <stamp>-<16 hex of the hash>");

    // One half is not a build: the target's exe with another pak matches no row, and says whose exe it is.
    if (const Row* t = Target()) {
        Identity half = IdentityOf(*t);
        half.pakHash[0] ^= 0xFF;
        ComposeKey(half);
        ok(Match(half) == nullptr && MatchExe(half) == t && MatchPak(half) == nullptr,
           "an exe match with another pak is an unknown build whose exe is named");
    }
    const Identity unknown = UnknownIdentity();
    ok(Match(unknown) == nullptr && FindByKey(unknown.key) == nullptr, "the unknown identity matches no row");
    Identity nopak;
    nopak.exeStamp = 0x6a08597du;
    ComposeKey(nopak);
    ok(std::strcmp(nopak.key, "6a08597d-nopak") == 0 && Match(nopak) == nullptr, "an unread pak keys as nopak");

    if (failed == 0) UE_LOGI("game-build selftest: ALL PASS (%d checks)", checks);
    else             UE_LOGE("game-build selftest: %d/%d FAILED", failed, checks);
    return failed == 0;
}

}  // namespace ue_wrap::game_build
