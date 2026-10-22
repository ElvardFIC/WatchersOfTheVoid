// coop/build_trust/self_identity.cpp -- this process's own build: the hash of the main.dll it was
// loaded from, the signature file beside it, and the one record they make.
//
// ComputeSelf runs once on the boot thread, before anything can host or join, and publishes the
// record through an atomic pointer; every other thread reads it with Self(). No config is read on the
// boot thread (the rows are unmeasured there): the test-key knob is read by SelfIsOfficial, which
// runs only after the harness has started.

#include "coop/build_trust/build_trust.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/net/protocol.h"  // kProtocolVersion -- the build number a signature must name
#include "coop/version.h"
#include "ue_wrap/core/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include <atomic>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace coop::build_trust {
namespace {

constexpr size_t kChunkBytes = 1u << 20;
constexpr size_t kSigFileMax = 1024;

BuildIdentity g_self;
std::atomic<const BuildIdentity*> g_published{nullptr};

std::string ToHex(const uint8_t* p, size_t n) {
    static const char kHex[] = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s.push_back(kHex[p[i] >> 4]);
        s.push_back(kHex[p[i] & 0xF]);
    }
    return s;
}

// The path of the module this code lives in, by the address of one of its own functions.
bool OwnModulePath(std::wstring* path) {
    HMODULE mod = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&ComputeSelf), &mod) ||
        mod == nullptr)
        return false;
    constexpr DWORD kCap = 32768;
    std::wstring buf(kCap, L'\0');
    const DWORD n = ::GetModuleFileNameW(mod, buf.data(), kCap);
    if (n == 0 || n >= kCap) return false;
    buf.resize(n);
    *path = std::move(buf);
    return true;
}

// SHA-256 of the whole file, read in chunks. Failure is ReadFailed or HashFailed; success is
// SelfStep::SigPresent (the caller's "go on").
SelfStep HashFile(const std::wstring& path, uint8_t out[kShaBytes]) {
    HANDLE f = ::CreateFileW(path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return SelfStep::ReadFailed;

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    SelfStep result = SelfStep::SigPresent;
    if (::BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        result = SelfStep::HashFailed;
    } else if (::BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
        result = SelfStep::HashFailed;
    } else {
        std::vector<uint8_t> chunk(kChunkBytes);
        for (;;) {
            DWORD got = 0;
            if (!::ReadFile(f, chunk.data(), static_cast<DWORD>(chunk.size()), &got, nullptr)) {
                result = SelfStep::ReadFailed;
                break;
            }
            if (got == 0) break;
            if (::BCryptHashData(hash, chunk.data(), got, 0) != 0) {
                result = SelfStep::HashFailed;
                break;
            }
        }
        if (result == SelfStep::SigPresent && ::BCryptFinishHash(hash, out, kShaBytes, 0) != 0)
            result = SelfStep::HashFailed;
    }
    if (hash) ::BCryptDestroyHash(hash);
    if (alg) ::BCryptCloseAlgorithmProvider(alg, 0);
    ::CloseHandle(f);
    return result;
}

// <path>.sig, whole, into *text. NoSigFile when it does not exist, SigMalformed when it cannot be
// read or is over 1 KiB; SigPresent means *text holds the file.
SelfStep ReadSigFile(const std::wstring& path, std::string* text) {
    const std::wstring sigPath = path + L".sig";
    HANDLE f = ::CreateFileW(sigPath.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        const DWORD err = ::GetLastError();
        return (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) ? SelfStep::NoSigFile
                                                                           : SelfStep::SigMalformed;
    }
    SelfStep result = SelfStep::SigPresent;
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(f, &size) || size.QuadPart < 0 ||
        static_cast<unsigned long long>(size.QuadPart) > kSigFileMax) {
        result = SelfStep::SigMalformed;
    } else {
        std::string buf(static_cast<size_t>(size.QuadPart), '\0');
        DWORD got = 0;
        if (!buf.empty() &&
            (!::ReadFile(f, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr) ||
             got != buf.size()))
            result = SelfStep::SigMalformed;
        else
            *text = std::move(buf);
    }
    ::CloseHandle(f);
    return result;
}

// The step the process ends on, with g_self.sha256 filled from NoSigFile on.
SelfStep Resolve(BuildIdentity* id) {
    std::wstring path;
    if (!OwnModulePath(&path)) return SelfStep::NoModulePath;

    uint8_t sha[kShaBytes];
    const SelfStep hashed = HashFile(path, sha);
    if (hashed != SelfStep::SigPresent) return hashed;
    std::memcpy(id->sha256, sha, kShaBytes);

    std::string text;
    const SelfStep read = ReadSigFile(path, &text);
    if (read != SelfStep::SigPresent) return read;

    SigFile sig;
    if (!ParseSigFile(text, &sig)) return SelfStep::SigMalformed;
    if (sig.target != coop::version::kGameTarget) return SelfStep::TargetMismatch;
    if (sig.build != static_cast<uint32_t>(coop::net::kProtocolVersion)) return SelfStep::BuildMismatch;
    if (std::memcmp(sig.sha256, id->sha256, kShaBytes) != 0) return SelfStep::HashMismatch;
    id->sig = std::move(sig);
    return SelfStep::SigPresent;
}

}  // namespace

const char* StepName(SelfStep step) {
    switch (step) {
        case SelfStep::NotComputed: return "not computed";
        case SelfStep::NoModulePath: return "no module path";
        case SelfStep::ReadFailed: return "read failed";
        case SelfStep::HashFailed: return "hash failed";
        case SelfStep::NoSigFile: return "no signature file";
        case SelfStep::SigMalformed: return "signature file malformed";
        case SelfStep::TargetMismatch: return "target mismatch";
        case SelfStep::BuildMismatch: return "build mismatch";
        case SelfStep::HashMismatch: return "hash mismatch";
        case SelfStep::SigPresent: return "signature present";
    }
    return "not computed";
}

void ComputeSelf() {
    if (g_published.load(std::memory_order_acquire) != nullptr) return;
    const ULONGLONG t0 = ::GetTickCount64();
    g_self.step = Resolve(&g_self);
    const unsigned long long ms = static_cast<unsigned long long>(::GetTickCount64() - t0);
    g_published.store(&g_self, std::memory_order_release);

    const BuildIdentity& id = g_self;
    const std::string sha8 = ToHex(id.sha256, 4);
    if (id.step == SelfStep::SigPresent && SignatureVerifies(id.sig, false)) {
        UE_LOGI("boot: build official (key %u) sha %s in %llu ms", static_cast<unsigned>(id.sig.keyId),
                sha8.c_str(), ms);
    } else if (id.step == SelfStep::SigPresent && id.sig.keyId == kTestKeyId &&
               SignatureVerifies(id.sig, true)) {
        UE_LOGI("boot: build signed by the test key sha %s in %llu ms", sha8.c_str(), ms);
    } else {
        const char* why = id.step == SelfStep::SigPresent ? "bad signature" : StepName(id.step);
        UE_LOGI("boot: build unofficial sha %s (%s) in %llu ms", sha8.c_str(), why, ms);
    }
}

const BuildIdentity& Self() {
    static const BuildIdentity kNotComputed{};
    const BuildIdentity* p = g_published.load(std::memory_order_acquire);
    return p != nullptr ? *p : kNotComputed;
}

bool SelfIsOfficial() {
    const BuildIdentity& id = Self();
    if (id.step != SelfStep::SigPresent) return false;
    return SignatureVerifies(id.sig,
                             coop::config::ResolveFlag(coop::config_registry::rows::build_trust_test_key));
}


std::string SelfShaHex() {
    const BuildIdentity& id = Self();
    return ToHex(id.sha256, kShaBytes);
}

std::string SelfSigHex() {
    const BuildIdentity& id = Self();
    return id.step == SelfStep::SigPresent ? ToHex(id.sig.sig, kSigBytes) : std::string();
}

}  // namespace coop::build_trust
