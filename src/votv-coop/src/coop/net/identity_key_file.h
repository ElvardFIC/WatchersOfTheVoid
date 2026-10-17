// coop/net/identity_key_file.h -- the durable identity's key FILE: where it lives, how it is found,
// read, written and protected, and what happens when it is missing, malformed or another
// account's. What the key MEANS (public key, id, signing) is peer_identity's; this unit hands it
// the 32-byte seed and says whether it was read or must be minted.
//
// Boot thread only (the one load); the pieces the selftest drives are stateless.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "coop/atomic_file/atomic_file.h"
#include "coop/net/peer_identity.h"

namespace coop::net::identity_key_file {

// Where the key lives: beside the game executable, with the install, where every build has kept
// it -- so an update changes nobody's identity, a Steam library move carries it, and a tester's
// two installs are two players. It is created under a private access list (this account and the
// system, nothing inherited), re-applied at every load, so another account on the same PC cannot
// read it. An account that cannot read the install's file -- another account holds it -- keeps its
// own key for this install beside it (`multivoid_identity_<account>.key`), so two accounts stay two
// players; while the install has no file of its own, an account that has such a file reads it, and
// one that has none mints the install's. A folder that refuses the write leaves the session on a
// temporary identity, as it leaves every file the mod writes. Never in the ini: inis get pasted
// into bug reports.
using peer_identity::kPrivKeyBytes;

// What a read found. Denied is the case the fallback exists for: the file is there and this
// account may not read it, which is another account's file. Malformed is a file that is not a
// key (a truncated or edited one); it gives way to a new key (written over it, except an account's
// own file while the install has none, whose new key goes to the install's path), and the log says
// so. Unreadable is any other failure to open or read a file that may well be there (a sharing
// violation from a scanner holding it, a device error): nothing is written over it, and this
// session runs on a temporary identity, because minting over a durable key that a transient error
// hid is the one loss this module exists to prevent.
enum class ReadResult { Loaded, Missing, Denied, Malformed, Unreadable };

// The install's own file. Empty only when the executable's folder cannot be resolved.
std::wstring InstallKeyFilePath();

// This account's key for an install whose own key file another account holds: beside it, named by
// a hash of this account's SID. Empty when nothing resolves.
std::wstring AccountKeyFilePath();

// The file name of an account's own key: a hash of the account's SID bytes and nothing else, so two
// accounts on one install stay two players. Empty when the SID is empty or the hash fails.
std::wstring AccountKeyFileName(const uint8_t* sid, size_t n);

// Reads the first `key=` line of `path` into `priv`. `errorOut` carries the OS error of a failed
// open or read.
ReadResult ReadKeyFile(const std::wstring& path, uint8_t priv[kPrivKeyBytes],
                       unsigned long* errorOut);

// Writes the key under the private list, which rides the create of the temporary and stays with
// the file through the move, so the file never exists with a wider one, not even for the instant
// between a create and a set. `errorOut` carries the OS error on failure, so the caller can tell a
// folder that refuses a write from anything else. CreateOnly fails with ERROR_ALREADY_EXISTS when
// the file is there (a second copy of the game minted first): the caller keeps its own key
// TEMPORARY and the next launch loads the file's.
bool WriteKeyFile(const std::wstring& path, const uint8_t priv[kPrivKeyBytes],
                  unsigned long* errorOut, coop::atomic_file::Mode mode);

// Where the key came from, or where a new one goes.
struct Found {
    std::wstring loadedFrom;  // the file the key was read from
    std::wstring target;      // where a minted key is written; empty = temporary
    bool minted = false;      // no key was read: the caller fills `priv` with fresh randomness
    // True only when the mint's target IS the file that read Malformed: only then may the write
    // replace; every other mint writes a target that read Missing, and create-only keeps a key a
    // second copy of the game minted a moment earlier.
    bool replaceOnMint = false;
    bool temporary = false;   // a file that may exist could not be read: nothing is written
};

// The install's file, then the account's fallback: reads the seed into `priv` when a file holds
// one, else says where a new one goes. Logs why whenever the answer is not the plain case.
Found Find(uint8_t priv[kPrivKeyBytes]);

// The end of a load, once the caller has the key's id and dial string: writes a minted key where
// Find said and logs the outcome (minted, loaded, or temporary and why).
void Conclude(const Found& found, const uint8_t priv[kPrivKeyBytes], const std::string& guid,
              const std::string& dial);

}  // namespace coop::net::identity_key_file
