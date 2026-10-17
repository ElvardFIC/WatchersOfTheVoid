// coop/net/identity_key_file.cpp -- see coop/net/identity_key_file.h for WHY.

#include "identity_key_file.h"

#include "identity_crypto.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>   // SetNamedSecurityInfoW

#include <vector>

namespace coop::net::identity_key_file {

namespace {

using identity_crypto::FromHex;
using identity_crypto::KeyFileAcl;
using identity_crypto::PrivateAcl;
using identity_crypto::Sha256;
using identity_crypto::ToHex;

const char* kKeyFileName = "multivoid_identity.key";

std::wstring KeyFileName() {
    std::wstring n;
    for (const char* c = kKeyFileName; *c; ++c) n.push_back(static_cast<wchar_t>(*c));
    return n;
}

// Sets the private list on the file; reports a volume that keeps none. False says only that the
// list is not on the file: the caller keeps the file regardless, since an identity that does not
// persist is the worse outcome.
bool ApplyKeyFileAcl(const std::wstring& path) {
    const PrivateAcl& acl = KeyFileAcl();
    if (!acl.ok) {
        UE_LOGW("peer_identity: could not build the key file's access list (this account's SID "
                "or the system's was unavailable) -- %ls keeps its folder's inherited list",
                path.c_str());
        return false;
    }
    const DWORD rc = ::SetNamedSecurityInfoW(
        const_cast<LPWSTR>(path.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr,
        acl.dacl, nullptr);
    if (rc == ERROR_SUCCESS) return true;
    if (rc == ERROR_NOT_SUPPORTED || rc == ERROR_INVALID_FUNCTION) {
        UE_LOGW("peer_identity: the volume holding %ls keeps no access lists (FAT or exFAT), so "
                "every account on this PC can read the key file; an NTFS volume protects it",
                path.c_str());
        return false;
    }
    UE_LOGW("peer_identity: could not set the key file's access list on %ls (error %lu) -- it "
            "keeps its folder's inherited list", path.c_str(), static_cast<unsigned long>(rc));
    return false;
}

}  // namespace

std::wstring InstallKeyFilePath() {
    const std::wstring dir = ue_wrap::paths::ExeDir();
    if (dir.empty()) return {};
    return dir + L"\\" + KeyFileName();
}

std::wstring AccountKeyFileName(const uint8_t* sid, size_t n) {
    if (sid == nullptr || n == 0) return {};
    uint8_t digest[32];
    if (!Sha256(sid, n, digest)) return {};
    std::wstring name = L"multivoid_identity_";
    for (char c : ToHex(digest, 8)) name.push_back(static_cast<wchar_t>(c));
    return name + L".key";
}

std::wstring AccountKeyFilePath() {
    const std::wstring dir = ue_wrap::paths::ExeDir();
    const std::vector<uint8_t>& sid = KeyFileAcl().userSid;
    if (dir.empty()) return {};
    const std::wstring name = AccountKeyFileName(sid.data(), sid.size());
    if (name.empty()) return {};
    return dir + L"\\" + name;
}

ReadResult ReadKeyFile(const std::wstring& path, uint8_t priv[kPrivKeyBytes],
                       unsigned long* errorOut) {
    if (errorOut) *errorOut = 0;
    if (path.empty()) return ReadResult::Missing;
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD e = ::GetLastError();
        if (errorOut) *errorOut = e;
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) return ReadResult::Missing;
        if (e == ERROR_ACCESS_DENIED) return ReadResult::Denied;
        return ReadResult::Unreadable;
    }
    char buf[4096];
    DWORD n = 0;
    const bool readOk = ::ReadFile(h, buf, sizeof(buf) - 1, &n, nullptr) != 0;
    if (!readOk && errorOut) *errorOut = ::GetLastError();
    ::CloseHandle(h);
    if (!readOk) return ReadResult::Unreadable;
    std::string text(buf, n);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        if (line.compare(0, eq, "key") != 0) continue;
        std::string hex = line.substr(eq + 1);
        while (!hex.empty() && (hex.back() == '\r' || hex.back() == '\n' ||
                                hex.back() == ' ' || hex.back() == '\t'))
            hex.pop_back();
        return FromHex(hex, priv, kPrivKeyBytes) ? ReadResult::Loaded : ReadResult::Malformed;
    }
    return ReadResult::Malformed;
}

bool WriteKeyFile(const std::wstring& path, const uint8_t priv[kPrivKeyBytes],
                  unsigned long* errorOut, coop::atomic_file::Mode mode) {
    if (errorOut) *errorOut = 0;
    if (path.empty()) return false;
    std::string text =
        "# Multivoid durable player identity -- KEEP THIS FILE SECRET.\n"
        "# Anyone who has this key can play as you: it is what proves your identity\n"
        "# to every host you join, and it is what your stored inventory is named by.\n"
        "# Copy it to another PC, named\n"
        "# multivoid_identity.key, to take your identity with you; never paste it into\n"
        "# a bug report, a screenshot or a Discord message.\n"
        "key=";
    text += ToHex(priv, kPrivKeyBytes);
    text += "\n";
    SECURITY_DESCRIPTOR sd{};
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    const PrivateAcl& acl = KeyFileAcl();
    if (acl.ok && ::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) &&
        ::SetSecurityDescriptorDacl(&sd, TRUE, acl.dacl, FALSE)) {
        ::SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
        sa.lpSecurityDescriptor = &sd;
    }
    const coop::atomic_file::Result r = coop::atomic_file::Write(
        path, text, mode, coop::atomic_file::Sync::ToDisk,
        sa.lpSecurityDescriptor ? &sa : nullptr);
    if (errorOut) *errorOut = r.error;
    return r.ok();
}

Found Find(uint8_t priv[kPrivKeyBytes]) {
    const std::wstring installPath = InstallKeyFilePath();
    const std::wstring account = AccountKeyFilePath();
    Found f;
    unsigned long readErr = 0;
    // This account's own file: used when it loads; an unreadable one ends TEMPORARY with nothing
    // written (never mint over a durable key a transient error hid); a malformed or missing one
    // gives way to a new key written to `mintTarget` (over it only when `mintTarget` is this file).
    auto readAccountKey = [&](const std::wstring& mintTarget) {
        switch (ReadKeyFile(account, priv, &readErr)) {
        case ReadResult::Loaded:
            f.loadedFrom = account;
            break;
        case ReadResult::Unreadable:
        case ReadResult::Denied:
            UE_LOGW("peer_identity: could not read this account's key file %ls (error %lu) -- "
                    "a TEMPORARY identity for this session, the file left untouched",
                    account.c_str(), readErr);
            f.minted = true;
            f.temporary = true;
            break;
        case ReadResult::Malformed:
            UE_LOGW("peer_identity: %ls is not a key file this build can read -- minting a new "
                    "identity into %ls", account.c_str(), mintTarget.c_str());
            f.minted = true;
            f.target = mintTarget;
            f.replaceOnMint = mintTarget == account;
            break;
        case ReadResult::Missing:
            f.minted = true;
            f.target = mintTarget;
            break;
        }
    };
    switch (ReadKeyFile(installPath, priv, &readErr)) {
    case ReadResult::Loaded:
        f.loadedFrom = installPath;
        break;
    case ReadResult::Denied:
        // Another account's file: this account keeps its own key beside it.
        if (account.empty()) {
            UE_LOGW("peer_identity: the install's key file (%ls) belongs to another account and "
                    "this account's own key file cannot be named -- a TEMPORARY identity for this "
                    "session", installPath.c_str());
            f.minted = true;
            f.temporary = true;
            break;
        }
        UE_LOGI("peer_identity: the install's key file (%ls) belongs to another account on this "
                "PC -- using this account's own key beside it (%ls)",
                installPath.c_str(), account.c_str());
        readAccountKey(account);
        break;
    case ReadResult::Unreadable:
        UE_LOGW("peer_identity: could not read the key file %ls (error %lu; a scanner holding it, "
                "or a device error) -- a TEMPORARY identity for this session, the file left "
                "untouched: your stored inventory will be found again once it reads",
                installPath.c_str(), readErr);
        f.minted = true;
        f.temporary = true;
        break;
    case ReadResult::Malformed:
        UE_LOGW("peer_identity: %ls is not a key file this build can read -- minting a new "
                "identity over it", installPath.c_str());
        f.minted = true;
        f.target = installPath;
        f.replaceOnMint = true;
        break;
    case ReadResult::Missing:
        // No install key: an account whose owner deleted it keeps the identity it already has.
        readAccountKey(installPath);
        break;
    }
    return f;
}

void Conclude(const Found& f, const uint8_t priv[kPrivKeyBytes], const std::string& guid,
              const std::string& dial) {
    if (f.minted && f.temporary) {
        // The read said why; this names the identity the session runs on, with the dial half a
        // joiner would need, so the host's log is complete even on the bad day.
        UE_LOGW("peer_identity: TEMPORARY identity %s for this session (the key file could "
                "not be read, see above) -- dial=%s", guid.c_str(), dial.c_str());
    } else if (f.minted) {
        unsigned long err = 0;
        const coop::atomic_file::Mode mode = f.replaceOnMint ? coop::atomic_file::Mode::Replace
                                                             : coop::atomic_file::Mode::CreateOnly;
        if (!f.target.empty() && WriteKeyFile(f.target, priv, &err, mode)) {
            ApplyKeyFileAcl(f.target);
            UE_LOGI("peer_identity: minted a new durable identity %s (saved to %ls) -- dial=%s",
                    guid.c_str(), f.target.c_str(), dial.c_str());
        } else {
            // Same shape as the retired player_guid's unreadable-ini path: the session still
            // works, the identity just does not survive a restart, and saying so is the
            // difference between a puzzle and a known state. MTA refuses to start when its
            // server-id.keys cannot be read or created
            // (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:758-762); a client of a
            // single-player game still plays, on a temporary identity.
            if (!f.target.empty() && !f.replaceOnMint &&
                (err == ERROR_ALREADY_EXISTS || err == ERROR_FILE_EXISTS)) {
                // The create-only write found the file: the one failure that is a race, not a
                // folder that refuses a write.
                UE_LOGW("peer_identity: minted identity %s but another copy of the game minted "
                        "the key first (%ls); this session runs on a TEMPORARY identity, the next "
                        "launch loads that key", guid.c_str(), f.target.c_str());
            } else {
                UE_LOGW("peer_identity: minted identity %s but could NOT write %ls (error %lu) "
                        "-- this identity is TEMPORARY and your stored inventory will not be "
                        "found again next launch", guid.c_str(),
                        f.target.empty() ? L"the key file (no folder resolved)" : f.target.c_str(),
                        err);
            }
        }
    } else {
        // Re-applied at every load, so a file an older build wrote, or one copied here from
        // another PC, is tightened at the next boot.
        ApplyKeyFileAcl(f.loadedFrom);
        // The `dial=` half is the value a joiner needs when there is no master in the loop
        // (net.host_identity), so it is printed on the ordinary path and not only on the mint.
        UE_LOGI("peer_identity: loaded durable identity %s from %ls -- dial=%s",
                guid.c_str(), f.loadedFrom.c_str(), dial.c_str());
    }
}

}  // namespace coop::net::identity_key_file
