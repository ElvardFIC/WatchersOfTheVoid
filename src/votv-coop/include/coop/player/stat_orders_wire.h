// coop/player/stat_orders_wire.h -- an admin's orders to, and queries of, one player's stats and
// effects: their bytes, the checks of a client's answer, and the host's table of the ones awaiting
// an answer. Engine-free: no engine type, no session, no clock. The game thread uses it.
//
// Every check of a client's ANSWER is in an Unpack function, the length first; the host never
// reads an answer any other way, so a client's bytes cannot fault it. An order's op, row and
// numbers are judged by the client that takes it, which answers Refused and never drops it.
// Tokens: the table below mints them and never expires one. The reliable channel delivers or the
// slot disconnects, and a slot's disconnect answers every token it holds.
//
// Red arm: with the dev row selftest_break_stat_orders the selftest's first case expects the wrong
// token, so the run must fail.

#pragma once

#include "coop/net/protocol.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace coop::stat_orders {

using coop::net::kStatRows;

enum class Op : uint8_t { SetStat = 0, AddEffect = 1, RemoveEffect = 2 };

// 0-3 travel; Left and Malformed are the host's own (a disconnect; an answer that failed its checks).
// MTA drops a packet it cannot read (reference/mtasa-blue/ reference/mtasa-blue/Server/mods/deathmatch/logic/CPacketTranslator.cpp:252); here the
// waiting caller is answered Malformed instead, so an admin's order never waits silently.
enum class Result : uint8_t { Applied = 0, Refused = 1, NotFound = 2, NotReady = 3, Left = 4, Malformed = 5 };

// What an order's caller is given. The flags are false unless the result is Applied.
struct OrderAnswer { Result result; float valueNow; bool storable; bool stored; };
struct EffectView { std::string name; bool live; float strength; float time; };
struct TableView { uint32_t validMask; float values[kStatRows]; uint8_t effectTotal; std::vector<EffectView> effects; };
using OrderDone = std::function<void(const OrderAnswer&)>;
using QueryDone = std::function<void(Result, const TableView&)>;

struct OrderWire { uint32_t token; Op op; uint8_t field; float value; float strength; float seconds; std::string effect; };

// An effect name the wire carries: 1-11 printable ASCII characters (0x20-0x7E).
bool ValidEffectName(const std::string& name);
const char* ResultText(Result r);   // "Applied", "Refused", ... for log lines
const char* OpText(Op op);          // "set", "add", "remove"

// Packing. PackOrder is false for an effect op whose name fails ValidEffectName; a SetStat's effect
// is ignored and an effect op's field is 0.
bool PackOrder(const OrderWire& o, coop::net::StatOrderPayload* out);
// r <= NotReady (the host's Left and Malformed never travel); valueNow is written only for Applied.
coop::net::StatOrderReplyPayload PackOrderReply(uint32_t token, Result r, Op op, float valueNow);
coop::net::StatQueryPayload PackQuery(uint32_t token);
// r <= NotReady. For Applied: the first five entries of view.effects, `effectTotal` the size of
// view.effects clamped to 255, a name over 11 characters cut; any other result packs an empty table.
coop::net::StatQueryReplyPayload PackQueryReply(uint32_t token, Result r, const TableView& view);

// Unpacking. A false return leaves every output untouched.
// False: the length is not the payload's. A name is read to 12 bytes, unprintable -> '?'.
bool UnpackOrder(const void* data, size_t len, OrderWire* out);
bool UnpackQuery(const void* data, size_t len, uint32_t* token);
// False also when result > NotReady or (Applied) valueNow is not finite; a non-Applied valueNow reads 0.
bool UnpackOrderReply(const void* data, size_t len, uint32_t* token, Result* r, Op* op, float* valueNow);
// False also when result > NotReady. A non-Applied reply gives a zeroed view. validMask is masked to
// kStatRows bits; a non-finite value clears its bit and reads 0; effectCount is taken as
// min(effectCount, 5, effectTotal); an entry whose strength or time is not finite is dropped; a name
// is read to 12 bytes, unprintable -> '?'.
bool UnpackQueryReply(const void* data, size_t len, uint32_t* token, Result* r, TableView* out);

enum class Kind : uint8_t { Order, Query };
struct Pending {
    uint32_t token; uint8_t slot; uint32_t generation; Kind kind; Op op; uint8_t field;
    uint64_t sentMs; OrderDone orderDone; QueryDone queryDone;   // one of the two callbacks set
};

// The host's table of what it sent and has no answer to yet. Two allowances per slot, so a polling
// pane never starves an order.
// The token is the Source SDK's convar-query cookie (reference/source-sdk-2013/ iserverplugin.h:
// 36-44; its invalid cookie is -1, ours 0); nothing expires: the answer rides one reliable lane.
class PendingTable {
public:
    static constexpr int kMaxOrders = 16;   // per slot
    static constexpr int kMaxQueries = 1;   // per slot: a polling pane never starves an order
    explicit PendingTable(uint32_t firstToken = 1);
    // A fresh token (never 0, none another entry holds) for the entry; false when its slot holds its
    // allowance of that kind.
    bool Add(Pending entry, uint32_t* token);
    // The entry for `token`, removed; false (nothing removed) when the token is unknown or its slot,
    // kind or generation differ from the arguments.
    bool Take(uint32_t token, uint8_t slot, uint32_t generation, Kind kind, Pending* out);
    // The OLDEST entry of `slot` and `kind`, removed: an answer arrives in the order its requests
    // were sent (one ordered lane), so an answer too malformed to carry a token answers the oldest.
    // False (nothing removed) when the slot holds none or its oldest of that kind carries a different
    // generation, as Take refuses a changed one.
    bool TakeOldest(uint8_t slot, uint32_t generation, Kind kind, Pending* out);
    std::vector<Pending> TakeSlot(uint8_t slot);   // every entry of the slot, removed
    std::vector<Pending> TakeAll();
    int Count(uint8_t slot) const;                 // both kinds
    uint64_t OldestSentMs(uint8_t slot) const;     // the stamp of the slot's oldest entry, 0 when none
private:
    std::vector<Pending> entries_;
    uint32_t next_;
};

// Boot selftest of the codec and the table, both peers, once per process.
bool RunSelftest();

}  // namespace coop::stat_orders
