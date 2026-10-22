// coop/build_trust/build_trust.h -- is this main.dll one the release key signed?
//
// A COMPATIBILITY check, not a security mechanism: a peer states its own build, and a build changed
// on purpose can state the official one. What it catches is the honest case -- a fork, a home build,
// a half-updated install. Engine-free: no engine call, no wire; it takes only the version constants.
//
// The signed message is "multivoid-build-v1" || u8 len || target || u32le build || sha256(main.dll).
// The signature ships beside the DLL as main.dll.sig (grammar at ParseSigFile).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace coop::build_trust {

inline constexpr size_t kShaBytes = 32;
inline constexpr size_t kSigBytes = 64;
inline constexpr size_t kPubBytes = 32;
inline constexpr size_t kTargetMax = 23;
inline constexpr uint8_t kTestKeyId = 255;

// One main.dll.sig, parsed.
struct SigFile {
    uint8_t keyId = 0;
    std::string target;
    uint32_t build = 0;
    uint8_t sha256[kShaBytes]{};
    uint8_t sig[kSigBytes]{};
};

// "multivoid-build-v1" || u8 len || target || u32le build || sha256 -- what a release key signs.
std::string SignedMessage(std::string_view target, uint32_t build, const uint8_t sha256[kShaBytes]);

// The file's exact grammar: six lines in this order, each ended by "\n" (a "\r\n" accepted, the final
// newline optional), nothing after:
//   multivoid-build-sig 1 / key <1..255> / target <1..23 printable ASCII, no space> /
//   build <decimal u32> / sha256 <64 lowercase hex> / sig <128 lowercase hex>
// Numbers carry no sign and no leading zero. False on any deviation, and *out is then untouched.
bool ParseSigFile(std::string_view text, SigFile* out);

// True when sig.sig verifies over SignedMessage(sig.target, sig.build, sig.sha256) under the release
// key whose id is sig.keyId, or under the test key when sig.keyId == kTestKeyId and trustTestKey.
// An id in no row (0 included) is false.
bool SignatureVerifies(const SigFile& sig, bool trustTestKey);

// Once per process; logs ALL PASS / n/m checks passed.
bool RunSelftest();

// The decoded release table, for the selftest's checks 20-21 and the boot banner.
struct ReleaseKey {
    uint8_t id;
    uint8_t pub[kPubBytes];
    uint8_t fixture[kSigBytes];
};
size_t ReleaseKeyCount();
const ReleaseKey& ReleaseKeyAt(size_t index);  // index < ReleaseKeyCount()

// --- this process's own build (self_identity.cpp) ---------------------------------------------------

enum class SelfStep : uint8_t {
    NotComputed,   // ComputeSelf has not published
    NoModulePath,  // the module's own path did not resolve
    ReadFailed,    // main.dll could not be opened or read
    HashFailed,    // the CNG provider failed
    NoSigFile,     // <path>.sig does not exist (ERROR_FILE_NOT_FOUND / ERROR_PATH_NOT_FOUND)
    SigMalformed,  // the .sig could not be read, is larger than 1 KiB, or fails ParseSigFile
    TargetMismatch,
    BuildMismatch,
    HashMismatch,
    SigPresent  // every field matched; whether it VERIFIES is SignatureVerifies' answer
};

struct BuildIdentity {
    uint8_t sha256[kShaBytes]{};  // the file's hash: valid at NoSigFile and every later step
    SelfStep step = SelfStep::NotComputed;
    SigFile sig;  // meaningful only when step == SigPresent
};

void ComputeSelf();                // boot thread, once, before harness::Start
const BuildIdentity& Self();       // any thread
// Unlatched: one ini read and one signature verify per call; a caller that wants it often latches it.
bool SelfIsOfficial();             // step == SigPresent && SignatureVerifies(sig, ResolveFlag(rows::build_trust_test_key))
const char* StepName(SelfStep step);
std::string SelfShaHex();          // 64 lowercase hex of Self().sha256
std::string SelfSigHex();          // 128 lowercase hex of Self().sig.sig when step == SigPresent, else ""

}  // namespace coop::build_trust
