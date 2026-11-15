// coop/moderation/ban_list.h -- the host's persistent ban list, keyed by the PROVED player id and,
// when the host saw a real one, the player's own address.
//
// MTA precedent: CBanManager / CBan (reference/mtasa-blue/Server/mods/deathmatch/logic/
// CBanManager.{h,cpp}, CBan.h): a ban's serial, IP and nick persisted to banlist.xml, matched
// (serial, then IP) in Packet_PlayerJoinData before the player spawns (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:1956, :1973); the
// nick is the admin's reference and is never matched. Our serial is the id the identity proof
// established; our IP is the connection's own address, stored only on a direct path
// (IsBannableAddress, moderation::EnforceableAddress).
//
// HOST-ONLY, permanent, in the hosted server's folder as bans.json: the host's start hands that
// folder to Load, and the store keeps it for its writes. A file the load could not read whole is
// never rewritten that session. Locks: a WRITE mutex (outer) guards the disk write, the folder and
// the read-only mark; a SET mutex (inner) guards the set and its index; the set mutex is never
// held across a file write, so the net thread's IsBanned never waits on the disk.

#pragma once

#include "coop/text/utf8_codec.h"  // kNickBufBytes

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Divergences from MTA's CBanManager.cpp (reference/mtasa-blue/Server/mods/deathmatch/logic/):
//  - MTA's player ban stores the exact IP (AddBan, :90-96); its wildcard matching (GetBanFromIP,
//    :218-238) applies only to patterns an admin writes into an IPv4 entry. Ours has no
//    admin-written range patterns: no range rule is decided, so an address matches exactly.
//  - MTA allows saving from the start of its load (m_bAllowSave, :290), so an unparseable
//    banlist.xml is overwritten at the next save; ours never rewrites a file it could not read
//    whole, because the rewrite would drop the records it could not read.
//  - MTA refuses a second ban on an already-banned IP (IsSpecificallyBanned, :170-181, used by
//    AddBan at :92 and :106); ours keeps both records, which are keyed by id, and the index
//    answers the first.
namespace coop::ban_list {

// Plain-data record for UI consumption (the F1 Administration panel's Banned section renders
// copies on the render thread). `id` is the proved player id (32 lower-case hex); `nick` the last
// name seen, for the admin only; `address` the enforced address, empty when none was enforceable.
struct Entry {
    char      id[33]     = {};
    char      nick[coop::text::kNickBufBytes] = {};
    char      address[64] = {};
    char      reason[96] = {};
    long long bannedUnix = 0;
};

// An address that can name one player's machine: never an unspecified one, never loopback (every
// instance on the host's own PC). Only a bannable address is stored, indexed or matched.
bool IsBannableAddress(std::string_view a);

// address -> id of the entry that holds it. Built over the bannable addresses only.
using AddressIndex = std::unordered_map<std::string, std::string>;
AddressIndex BuildAddressIndex(const std::vector<Entry>& set);
// The id holding `address`, or nullptr. An address that is not bannable is never found.
const std::string* LookupAddress(const AddressIndex& index, std::string_view address);

// The file codec, pure (no file, no lock). ParseBans reads a bans.json text into `out`: false when
// the text does not parse or is not an object holding an array `bans`. A record that fails a
// field check is skipped (`*skipped` counts it, `problems` names it); two records with one id keep
// the later, said in `problems` and not counted as skipped. SerializeBans is its inverse.
bool ParseBans(std::string_view json, std::vector<Entry>* out, std::vector<std::string>* problems,
               int* skipped);
std::string SerializeBans(const std::vector<Entry>& in);

// Host: load `<serverDir>\bans.json` into memory, replacing the set. Runs once at host session
// start, before the net thread spawns. An empty `serverDir` (the folder could not be made) leaves
// the store memory-only. A file that is unreadable or has records this build cannot read makes
// the store READ-ONLY for the session: the bans it could read are enforced, and the file is not
// rewritten.
void Load(const std::wstring& serverDir);

// Any thread: the file is not being written this session (see Load). The admin panel's reader.
bool IsReadOnly();

// Net thread, at the identity proof: is this player id, or this address, banned? `address` is the
// connection's own address (empty on a relayed path or when GNS has none); it is matched only when
// IsBannableAddress. On a match the ban's stored reason is copied into `reasonOut` (may be null,
// "" for an empty one) so the close can carry it, and one log line names the key that matched.
bool IsBanned(std::string_view playerId, std::string_view address, char* reasonOut, int reasonLen);

// Game thread: ban a player id (adds to the in-memory set AND rewrites the file). `address` is
// stored only when IsBannableAddress, else as "". `nick` and `reason` are the admin's reference.
// Adding an id already banned refreshes its record. False, changing nothing, for an id that is
// not 32 lower-case hex; true otherwise (a read-only store or a failed write still bans in
// memory).
bool Add(const char* playerId, const char* nick, const char* address, const char* reason);

// Unban a player id (removes from the set AND rewrites the file). True when it removed one; false
// for an id that was not banned, changing nothing. Any thread (internal mutexes).
bool Remove(const char* playerId);

// The ids that start with `hexPrefix` (compared lower-case), in no particular order; an empty
// prefix matches every id. Any thread (the set mutex).
std::vector<std::string> IdsWithPrefix(std::string_view hexPrefix);

// Copy all ban records, most recent first. Any thread.
void GetSnapshot(std::vector<Entry>& out);

// The codec, the address rule and the index, in memory (no disk, never the live store). Run at
// each session start; prints `ban_list selftest: ALL PASS (%d checks)` or `ban_list selftest
// FAIL: %s`.
bool RunSelftest();

}  // namespace coop::ban_list
