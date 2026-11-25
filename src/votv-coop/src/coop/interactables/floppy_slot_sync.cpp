// coop/interactables/floppy_slot_sync.cpp -- see coop/interactables/floppy_slot_sync.h.

#include "coop/interactables/floppy_slot_sync.h"
#include "coop/props/disc_record_ceiling.h"

#include "coop/interactables/laptop_buffer_sync.h"  // the quad's shadow primes after a laptop apply
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/text/utf8_codec.h"

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/actors/floppy_disc.h"
#include "ue_wrap/devices/floppy_slot.h"
#include "ue_wrap/devices/laptop.h"
#include "ue_wrap/devices/serverbox.h"

#include <windows.h>

#include <atomic>
#include <array>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace coop::floppy_slot_sync {
namespace {

namespace FS = ue_wrap::floppy_slot;
namespace SB = ue_wrap::serverbox;
namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;

std::atomic<coop::net::Session*> g_session{nullptr};

// Slot occupancy is interaction state. A one-second poll lets an eject race an insert that the
// other peer has not observed yet. The digest read is allocation-free for empty slots.
constexpr uint64_t kSweepMs = 50;

// The identity is the device's index in its kind's own list, and the list is the gamemode's
// servers[]: the level places the boxes, so both peers build the same order. The cap is
// serverbox_sync's, for the same reason -- one lane must not address a box the other cannot.
constexpr size_t kMaxDevices = 64;

// A full disc of server requests is about 50 KiB. Reserve room for either outer header when
// packing one slot; larger sets still split into independently bounded canonical blobs.
constexpr size_t kMaxSlotBytes = coop::blob_chunks::MaxBlobBytes() - 64;
constexpr size_t kCanonicalHeadBytes = 8;  // op, kind, generation, entry count
static_assert(kMaxSlotBytes + kCanonicalHeadBytes <= coop::blob_chunks::MaxBlobBytes());

// A client's claim is unbounded input. The window is generous next to the 1 Hz poll that
// produces one, so a peer playing normally never reaches it.
constexpr uint64_t kRateWindowMs = 2000;
constexpr int      kMaxClaimsPerWindow = 12;

uint64_t NowMs() { return static_cast<uint64_t>(::GetTickCount64()); }

// ---- wire ----
// head [u8 op][u8 deviceKind]; op 0=claim{[u8 index][slot]}, 1=canonical{[u32 generation][u16 n]{[u8 index][slot]}}
// The generation counts the host's changes of the laptop's occupancy (0 for the boxes): the laptop's file quad carries
// it too, so the two lanes agree which disc an edit belongs to.
// slot [i32 type][i32 rw][u8 zip][u32 nametypeLen][utf8][u32 jsonLen][utf8][u16 rows]{[u32 len][utf8]}
// nametype is empty for every class but the laptop's, and it is on the wire because the format
// claims to leave room for that device: a member the digest raises an edge on and the wire drops
// is a field the receiver would blank on every change.

constexpr uint8_t kOpClaim     = 0;
constexpr uint8_t kOpCanonical = 1;

void PutU16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFF));
    b.push_back(static_cast<uint8_t>(v >> 8));
}
void PutU32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}
// A row or a JSON can be long, so both take a 32-bit length; the reader bounds them against the
// blob it is reading out of.
void PutStr32(std::vector<uint8_t>& b, const std::wstring& w) {
    const std::string u = coop::text::ToUtf8Exact(w);
    PutU32(b, static_cast<uint32_t>(u.size()));
    b.insert(b.end(), u.begin(), u.end());
}

struct Reader {
    const uint8_t* p; size_t n; size_t off = 0;
    bool ok = true;
    uint8_t U8() { if (off + 1 > n) { ok = false; return 0; } return p[off++]; }
    uint16_t U16() { if (off + 2 > n) { ok = false; return 0; } uint16_t v = static_cast<uint16_t>(p[off] | (p[off + 1] << 8)); off += 2; return v; }
    uint32_t U32() { if (off + 4 > n) { ok = false; return 0; } uint32_t v = 0; std::memcpy(&v, p + off, 4); off += 4; return v; }
    std::wstring Str32() {
        const uint32_t len = U32();
        if (!ok || len > n - off) { ok = false; return std::wstring(); }
        std::wstring w;
        const bool wellFormed = coop::text::FromUtf8Strict(reinterpret_cast<const char*>(p + off), len, &w);
        off += len;
        if (!wellFormed) { ok = false; return std::wstring(); }
        return w;
    }
};

struct Slot {
    FS::Scalars st;
    FS::Content c;
};

// An empty slot is a type and nothing else. What the device still holds in its other fields is
// residue its own eject left behind, so carrying it would put a stale disc's JSON on the wire for
// every empty box in the connect set.
void PackSlot(std::vector<uint8_t>& b, const Slot& s) {
    PutU32(b, static_cast<uint32_t>(s.st.floppyType));
    if (s.st.floppyType < 0) return;
    PutU32(b, static_cast<uint32_t>(s.st.readWrites));
    b.push_back(s.st.zip ? 1 : 0);
    PutStr32(b, s.c.nametype);
    PutStr32(b, s.c.objectData);
    // The count and the rows that follow it are the same number or the reader desyncs, so the
    // clamp binds both.
    const size_t rows = s.c.data.size() < 0xFFFF ? s.c.data.size() : 0xFFFF;
    PutU16(b, static_cast<uint16_t>(rows));
    for (size_t i = 0; i < rows; ++i) PutStr32(b, s.c.data[i]);
}

bool ParseSlot(Reader& r, Slot& out) {
    out.st.floppyType = static_cast<int32_t>(r.U32());
    if (!r.ok) return false;
    if (out.st.floppyType < 0) return true;  // empty: the type is the whole body
    out.st.readWrites = static_cast<int32_t>(r.U32());
    out.st.zip        = r.U8() != 0;
    out.c.nametype    = r.Str32();
    out.c.objectData  = r.Str32();
    const uint16_t rows = r.U16();
    for (uint16_t i = 0; i < rows && r.ok; ++i) out.c.data.push_back(r.Str32());
    return r.ok;
}

// ---- devices ----

// The live device list for a kind: the gamemode's server boxes, or the one laptop.
constexpr FS::DeviceKind kKinds[] = {FS::DeviceKind::ServerBox, FS::DeviceKind::Laptop};

size_t ReadDevices(FS::DeviceKind kind, std::vector<void*>& out) {
    out.clear();
    if (kind == FS::DeviceKind::Laptop) {
        void* laptop = ue_wrap::laptop::EnsureResolved() ? ue_wrap::laptop::Instance() : nullptr;
        if (laptop) out.push_back(laptop);
        return out.size();
    }
    SB::ReadServers(out);
    if (out.size() > kMaxDevices) out.resize(kMaxDevices);
    return out.size();
}

// The laptop's file rows and read-writes are the quad's (coop/interactables/laptop_buffer_sync) between two changes of
// its occupancy, so its digest is the occupancy alone and a file edit raises no claim here.
FS::DigestScope ScopeOf(FS::DeviceKind kind) {
    return kind == FS::DeviceKind::Laptop ? FS::DigestScope::Occupancy : FS::DigestScope::Whole;
}

bool ReadSlotOf(FS::DeviceKind kind, void* device, Slot& out,
                const std::function<bool()>& current = {}) {
    return (!current || current()) && FS::ReadScalars(kind, device, out.st) &&
        (!current || current()) && FS::ReadContent(kind, device, out.c) && (!current || current());
}

// ---- state ----

// Per (kind, index): the digest of the slot as this peer last published or applied it. The poll
// compares against it, so a wire apply can never be read back as a local edit.
std::map<uint32_t, uint64_t> g_shadow;
std::set<uint32_t> g_retry;          // sends the transport refused
std::set<uint32_t> g_unsendable;     // slots this peer cannot put on the wire at all
std::set<uint32_t> g_refusedOversize;  // slots whose over-ceiling claim was already logged, one line each

// CLIENT: until the host's canonical has landed, this peer's slots are its own save's, not the
// session's. Its boxes come up at class defaults, the save's loadData fills them a tick or two
// later, and a sweep that read THAT as a local edge would claim the joiner's stale world over the
// host's live one -- publishing, as canonical, the disappearance of a disc the host inserted after
// its last save. Prime and stay mute until the host has spoken -- per kind, because each kind's
// connect set is its own blob and a refused one is retried alone: the boxes' set landing first says
// nothing about the laptop.
bool g_haveCanonical[FS::kDeviceKindCount] = {};

// The connect snapshot can arrive before the local gamemode has populated its device array.
// Retain canonical values until their target exists so a later local save load cannot be claimed
// over the host's value.
std::map<uint32_t, Slot> g_pendingCanonical;
std::map<uint32_t, uint32_t> g_pendingCanonicalGen;
std::map<uint32_t, uint64_t> g_pendingCanonicalRevision;
std::set<uint32_t> g_pendingWriteDebt;
std::array<uint32_t,FS::kDeviceKindCount> g_pendingCursor{};
uint64_t g_pendingEpoch=0,g_pendingRevision=0;
uint32_t g_boundWorld=0,g_boundSerial=0;
bool g_pendingDraining=false;
struct CanonicalOrder{bool known=false;uint32_t sequence=0;};
std::array<std::array<CanonicalOrder,kMaxDevices>,FS::kDeviceKindCount> g_canonicalOrder{};

// CLIENT: a claim the host drops -- for its rate, its size, a bad index or a malformed body --
// gets no answer of any kind, and this peer primed its shadow when the claim was SENT. Without
// this the divergence is permanent. Re-claim on a deadline, a bounded number of times.
struct Awaiting { uint64_t deadline = 0; int tries = 0; };
std::map<uint32_t, Awaiting> g_awaiting;
constexpr uint64_t kAnswerMs = 4000;
constexpr int      kMaxClaimTries = 3;

// HOST: a joiner whose connect set the transport refused has a permanently stale world -- the 1 Hz
// sweep covers edges, not a set nobody asked for. Re-send it.
struct ConnectRetry {
    uint32_t occupant = 0;
    uint64_t revision = 0, next = 0;
    std::array<bool, FS::kDeviceKindCount> sent{};
};
std::map<int, ConnectRetry> g_connectRetry;
uint64_t g_connectRevision = 0;
bool g_hostPublishing = false, g_connectDraining = false, g_ticking = false;
uint64_t g_publishRequest = 0;
bool g_clientClaiming = false, g_hostClaiming = false, g_sweeping = false;
uint64_t g_clientRequest = 0;
std::array<uint64_t, coop::net::kMaxPeers> g_peerEpoch{};

// The laptop's generation: the host's count of its occupancy changes; a client's is the last canonical's.
uint32_t g_laptopGen = 0;
uint32_t GenOf(FS::DeviceKind kind) { return kind == FS::DeviceKind::Laptop ? g_laptopGen : 0; }

// HOST: the occupancy the current generation names. The generation moves when the laptop's occupancy does, however the
// host comes to see it -- its sweep, its silent prime, its answer to a claim -- and at nothing else: a claim for the disc
// the laptop already holds, or a publish the transport refused and the sweep sends again, names the same disc.
uint64_t g_laptopGenDigest = 0;
bool     g_laptopGenKnown = false;

void NoteLaptopOccupancy(void* laptop, const std::function<bool()>& current = {}) {
    uint64_t d = 0;
    if (!FS::ReadDigest(FS::DeviceKind::Laptop, laptop, d, FS::DigestScope::Occupancy)) return;
    if (current && !current()) return;
    if (g_laptopGenKnown && d == g_laptopGenDigest) return;
    if (g_laptopGenKnown) ++g_laptopGen;
    g_laptopGenDigest = d;
    g_laptopGenKnown = true;
}
constexpr uint64_t kConnectRetryMs = 1000;
uint64_t g_nextSweep = 0;
coop::blob_chunks::Assembler g_asm;
uint32_t g_nextSeq = 1;

struct Rate { uint64_t windowStart = 0; int count = 0; bool warned = false; };
std::map<uint8_t, Rate> g_rate;      // per sender slot

uint32_t ShadowKey(FS::DeviceKind kind, size_t index) {
    return (static_cast<uint32_t>(kind) << 16) | static_cast<uint32_t>(index);
}

bool StageCanonical(FS::DeviceKind kind,uint8_t index,Slot value,uint32_t generation,uint32_t sequence){
    const auto kindIndex=static_cast<uint8_t>(kind);
    if(kindIndex>=FS::kDeviceKindCount || index>=kMaxDevices)return false;
    auto& order=g_canonicalOrder[kindIndex][index];
    if(order.known && int32_t(sequence-order.sequence)<=0)return false;
    const auto key=ShadowKey(kind,index);
    g_pendingCanonical[key]=std::move(value);
    g_pendingCanonicalRevision[key]=++g_pendingRevision;
    if(kind==FS::DeviceKind::Laptop)g_pendingCanonicalGen[key]=generation;
    g_awaiting.erase(key);
    g_retry.erase(key);
    g_unsendable.erase(key);
    order={true,sequence};
    return true;
}

bool IsHost() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->role() == coop::net::Role::Host;
}

struct HostFrame {
    coop::net::Session* session;
    uint64_t epoch = g_pendingEpoch;
    uint32_t world = ue_wrap::world_identity::Generation();
    uint32_t serial = coop::net::session_serial::Current();
    bool Current() const {
        return session && session == g_session.load() && epoch == g_pendingEpoch &&
            world == ue_wrap::world_identity::Generation() &&
            serial == coop::net::session_serial::Current() && session->connected() &&
            session->role() == coop::net::Role::Host;
    }
};
struct PeerFrame {
    coop::net::Session* session;
    int slot;
    uint64_t epoch = g_pendingEpoch;
    uint32_t world = ue_wrap::world_identity::Generation();
    uint32_t serial = coop::net::session_serial::Current();
    coop::net::Role role;
    uint32_t occupant;
    uint64_t departure;
    PeerFrame(coop::net::Session* s, int peer) : session(s), slot(peer),
        role(s ? s->role() : coop::net::Role::Client),
        occupant(s && peer >= 0 && peer < coop::net::kMaxPeers ? s->peerGenerationForSlot(peer) : 0),
        departure(peer >= 0 && peer < coop::net::kMaxPeers ? g_peerEpoch[peer] : 0) {}
    bool Current() const {
        if (!session || slot < 0 || slot >= coop::net::kMaxPeers ||
            session != g_session.load() || epoch != g_pendingEpoch ||
            world != ue_wrap::world_identity::Generation() ||
            serial != coop::net::session_serial::Current() || !session->connected() ||
            role != session->role() || departure != g_peerEpoch[slot] ||
            occupant != session->peerGenerationForSlot(slot)) return false;
        return role == coop::net::Role::Host ? slot > 0 && occupant &&
            session->IsSlotConnected(slot) && session->IsSlotWorldReady(slot) : slot == 0;
    }
};
struct DeviceFrame {
    PeerFrame peer;
    ue_wrap::CachedObjRef target, cls;
    DeviceFrame(const PeerFrame& source, void* device) : peer(source) {
        if (!peer.Current() || !R::IsLive(device)) return;
        target.Set(device);
        if (target.Get()) cls.Set(R::ClassOf(device));
    }
    bool Current() const {
        return peer.Current() && target.Get() && cls.Get() && R::ClassOf(target.Get()) == cls.Get();
    }
};
struct BusyScope {
    bool& busy;
    explicit BusyScope(bool& value) : busy(value) { busy = true; }
    ~BusyScope() { busy = false; }
};

void PrimeShadow(FS::DeviceKind kind, size_t index, void* device,
                 const std::function<bool()>& current = {}) {
    uint64_t d = 0;
    if (FS::ReadDigest(kind, device, d, ScopeOf(kind)) && (!current || current()))
        g_shadow[ShadowKey(kind, index)] = d;
}

// A slot this peer cannot put on the wire -- past the size ceiling, or with rows the reader will
// not read. Prime the shadow so the sweep stops, drop the retry, and warn ONCE for this device.
// Leaving the shadow stale instead is a permanent 1 Hz re-read of the very field that is too big
// to read, with a log line every second; the slot is retried when it next changes, which is the
// only event that could make it sendable.
void ParkUnsendable(FS::DeviceKind kind, size_t index, void* device, const char* why,
                    size_t bytes, const std::function<bool()>& current = {}) {
    if (current && !current()) return;
    const uint32_t key = ShadowKey(kind, index);
    if (g_unsendable.insert(key).second)
        UE_LOGW("floppy_slot_sync: device %zu's slot cannot travel (%s, %zu B) -- this peer keeps "
                "its own copy and the divergence is real; retried when the slot next changes",
                index, why, bytes);
    PrimeShadow(kind, index, device, current);
    if (current && !current()) return;
    g_retry.erase(key);
}

// A slot that packs within the ceiling, or nothing.
bool PackOne(uint8_t index, const Slot& cur, std::vector<uint8_t>& out) {
    out.clear();
    out.push_back(index);
    PackSlot(out, cur);
    return out.size() <= coop::disc_record::MaxBodyBytes();
}

// Pack a set of devices into canonical blobs, split at the slot cap so one huge slot cannot take
// the whole set down with it. Returns the blobs to send, in order.
std::vector<std::vector<uint8_t>> PackCanonicalSet(
    FS::DeviceKind kind, const std::vector<std::pair<uint8_t, Slot>>& entries,
    std::vector<uint8_t>* droppedOut = nullptr) {
    std::vector<std::vector<uint8_t>> blobs;
    size_t i = 0;
    while (i < entries.size()) {
        std::vector<uint8_t> body;
        uint16_t count = 0;
        while (i < entries.size()) {
            std::vector<uint8_t> one;
            one.push_back(entries[i].first);
            PackSlot(one, entries[i].second);
            if (one.size() > coop::disc_record::MaxBodyBytes()) {
                if (droppedOut) droppedOut->push_back(entries[i].first);
                ++i;
                continue;
            }
            // Keep each entry whole, including the generation in the envelope size.
            if (count > 0 && body.size() + one.size() + kCanonicalHeadBytes > coop::blob_chunks::MaxBlobBytes())
                break;
            body.insert(body.end(), one.begin(), one.end());
            ++count;
            ++i;
        }
        if (count == 0) continue;
        std::vector<uint8_t> blob;
        blob.push_back(kOpCanonical);
        blob.push_back(static_cast<uint8_t>(kind));
        PutU32(blob, GenOf(kind));
        PutU16(blob, count);
        blob.insert(blob.end(), body.begin(), body.end());
        blobs.push_back(std::move(blob));
    }
    return blobs;
}

bool SlotEquals(FS::DeviceKind kind, const Slot& a, const Slot& b) {
    // Two empty slots are the same slot. What each still holds in floppyReadwrites and
    // floppyObjectData is residue its own eject left for a deferred spawn to read, not state, and
    // comparing it would make every peer rewrite an empty box it already agrees about.
    if (a.st.floppyType < 0 || b.st.floppyType < 0)
        return a.st.floppyType < 0 && b.st.floppyType < 0;
    const bool sameDisc = a.st.floppyType == b.st.floppyType && a.st.zip == b.st.zip &&
                          a.c.nametype == b.c.nametype && a.c.objectData == b.c.objectData;
    // The laptop holding the disc the canonical names is the canonical: its rows and read-writes are its quad's
    // until the disc changes, and a peer's own edits since its claim are in them.
    if (ScopeOf(kind) == FS::DigestScope::Occupancy) return sameDisc;
    return sameDisc && a.st.readWrites == b.st.readWrites && a.c.data == b.c.data;
}

// Every change of the laptop's occupancy this lane takes -- a claim sent, a publish, a canonical written -- moves the
// disc's rows with it, so the quad's shadow takes them there: an edit made after it is the quad's to send, and one made
// before it rode the slot.
void PrimeQuadIfLaptop(FS::DeviceKind kind) {
    if (kind == FS::DeviceKind::Laptop) coop::laptop_buffer_sync::PrimeQuadBaseline();
}

// Apply one slot to a live device and prime the shadow to what was written, so the next poll
// reads no edge. A slot that already reads the incoming value is left alone: at a join most of a
// base's boxes are empty on both peers, and writing each anyway would mint every string and swap
// every mesh in the frame that closes the connect set. True means complete,
// including an already-equal slot; false means the canonical still needs retry.
bool ApplySlot(FS::DeviceKind kind, size_t index, void* device, const Slot& input,
               const std::function<bool()>& guard={},bool retryWrite=false) {
    const Slot s=input;
    ue_wrap::CachedObjRef target,cls;target.Set(device);
    if(!target.Get())return false;cls.Set(R::ClassOf(device));
    const auto epoch=g_pendingEpoch;
    const auto world=ue_wrap::world_identity::Generation(),serial=coop::net::session_serial::Current();
    auto* session=g_session.load();
    const auto current=[&]{return epoch==g_pendingEpoch && session==g_session.load() &&
        world==ue_wrap::world_identity::Generation() && serial==coop::net::session_serial::Current() &&
        target.Get() && cls.Get() && R::ClassOf(device)==cls.Get() && (!guard || guard());};
    if(!current())return false;
    // Two empty slots agree before either one's content is read, and at a join most of a base's
    // boxes are that case -- reading the rows and the JSON first would mint a stale disc's record
    // per box for a comparison that never looks at it.
    FS::Scalars curSt{};
    const bool scalars=FS::ReadScalars(kind, device, curSt);
    if(!current())return false;
    if (!retryWrite && scalars && curSt.floppyType < 0 && s.st.floppyType < 0) {
        PrimeShadow(kind, index, device, current);
        return current();
    }
    Slot cur;
    const bool read = ReadSlotOf(kind, device, cur, current);
    if(!current())return false;
    if (!retryWrite && read && SlotEquals(kind, cur, s)) {
        PrimeShadow(kind, index, device, current);
        return current();
    }
    const bool written=s.st.floppyType < 0 ? FS::ClearSlot(kind,device) : FS::WriteSlot(kind,device,s.st,s.c);
    if(!written || !current())return false;
    PrimeShadow(kind, index, device, current);
    if(!current())return false;
    PrimeQuadIfLaptop(kind);
    return current();
}

// Apply staged host values when a device that was not present during the connect snapshot appears.
// The canonical is retained per device index; a later host value replaces an earlier staged one.
void DrainPendingCanonicals(FS::DeviceKind kind, const std::vector<void*>& devices) {
    if(g_pendingDraining)return;
    g_pendingDraining=true;
    struct DrainGuard{~DrainGuard(){g_pendingDraining=false;}} drainGuard;
    // Native writes can clear/replace the pending maps. Iterate owned values and
    // retire only the revision that successfully reached its original device.
    struct PendingSlot {uint32_t key;Slot value;uint64_t revision;uint32_t generation;bool hasGeneration;};
    std::vector<PendingSlot> pending;
    auto& cursor=g_pendingCursor[static_cast<uint8_t>(kind)];
    auto it=g_pendingCanonical.upper_bound(cursor);
    for(size_t visited=0;visited<g_pendingCanonical.size() && pending.size()<8;++visited){
        if(it==g_pendingCanonical.end())it=g_pendingCanonical.begin();
        const auto& [key,value]=*it++;
        if(static_cast<FS::DeviceKind>(key>>16)!=kind)continue;
        cursor=key;
        const auto revision=g_pendingCanonicalRevision.find(key);
        if(revision==g_pendingCanonicalRevision.end())continue;
        const auto gen=g_pendingCanonicalGen.find(key);
        pending.push_back({key,value,revision->second,gen==g_pendingCanonicalGen.end()?0:gen->second,gen!=g_pendingCanonicalGen.end()});
    }
    const auto epoch=g_pendingEpoch;
    for (const auto& item:pending) {
        const auto key=item.key;
        if(epoch!=g_pendingEpoch)return;
        if (static_cast<FS::DeviceKind>(key >> 16) != kind) continue;
        const size_t index = static_cast<uint16_t>(key);
        if (index >= devices.size() || !devices[index] || !R::IsLive(devices[index])) {
            continue;
        }
        const auto current=[&]{const auto row=g_pendingCanonicalRevision.find(key);
            return epoch==g_pendingEpoch && row!=g_pendingCanonicalRevision.end() && row->second==item.revision;};
        if(!current())continue;
        const bool retryWrite=g_pendingWriteDebt.count(key)!=0;
        g_pendingWriteDebt.insert(key);
        if(!ApplySlot(kind,index,devices[index],item.value,current,retryWrite) || !current())continue;
        g_haveCanonical[static_cast<uint8_t>(kind)] = true;
        if (kind == FS::DeviceKind::Laptop) {
            if(item.hasGeneration)g_laptopGen=item.generation;
        }
        g_pendingCanonicalGen.erase(key);
        g_pendingCanonicalRevision.erase(key);
        g_pendingWriteDebt.erase(key);
        g_pendingCanonical.erase(key);
    }
}

// ---- host ----

void HostBroadcastOne(coop::net::Session* s, FS::DeviceKind kind, size_t index, void* device,
                      const std::function<bool()>& guard = {}) {
    const HostFrame frame{s};
    if (!frame.Current() || index >= kMaxDevices || (guard && !guard())) return;
    const uint32_t key = ShadowKey(kind, index);
    const uint64_t request = ++g_publishRequest;
    g_retry.insert(key);
    if (g_hostPublishing || g_connectDraining) return;
    BusyScope busy(g_hostPublishing);
    ue_wrap::CachedObjRef target, cls;
    target.Set(device);
    if (!target.Get()) return;
    cls.Set(R::ClassOf(device));
    const auto current = [&] {
        return frame.Current() && target.Get() && cls.Get() && R::ClassOf(device) == cls.Get() &&
            (!guard || guard());
    };
    if (!current()) return;
    if (kind == FS::DeviceKind::Laptop) {
        NoteLaptopOccupancy(device, current);
        if (!current()) return;
    }
    uint64_t digest = 0, after = 0;
    if (!FS::ReadDigest(kind, device, digest, ScopeOf(kind)) || !current()) return;
    Slot cur;
    if (!ReadSlotOf(kind, device, cur, current)) return;
    if (!FS::ReadDigest(kind, device, after, ScopeOf(kind)) || !current() || after != digest) return;
    std::vector<uint8_t> body;
    if (!PackOne(static_cast<uint8_t>(index), cur, body)) {
        ParkUnsendable(kind, index, device, "past the size ceiling", body.size(), current);
        return;
    }
    std::vector<uint8_t> blob;
    blob.push_back(kOpCanonical);
    blob.push_back(static_cast<uint8_t>(kind));
    PutU32(blob, GenOf(kind));
    PutU16(blob, 1);
    blob.insert(blob.end(), body.begin(), body.end());
    const uint32_t sequence = g_nextSeq++;
    PrimeQuadIfLaptop(kind);
    if (!current()) return;
    // Broadcast success means at least one recipient accepted. A consumed $srv
    // row is complete only when every ready recipient accepted this snapshot.
    std::array<uint32_t, coop::net::kMaxPeers> recipients{};
    const auto departures = g_peerEpoch;
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot)
        if (s->IsSlotConnected(slot) && s->IsSlotWorldReady(slot))
            recipients[slot] = s->peerGenerationForSlot(slot);
    bool whole = true;
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        if (!recipients[slot]) continue;
        const auto recipientCurrent = [&] {
            return current() && s->IsSlotConnected(slot) && s->IsSlotWorldReady(slot) &&
                recipients[slot] == s->peerGenerationForSlot(slot) && departures[slot] == g_peerEpoch[slot];
        };
        const bool sent = coop::blob_chunks::SendBlobToSlotWhile(s, slot,
            coop::net::ReliableKind::FloppySlotState, sequence, blob, recipientCurrent);
        if (!current()) return;
        whole = whole && sent;
    }
    if (!whole || request != g_publishRequest) return;
    // Keep the digest captured with the bytes, never a newer post-send value.
    g_shadow.insert_or_assign(key, digest);
    g_retry.erase(key);
    g_unsendable.erase(key);
}

// ---- client ----

void ClientClaim(coop::net::Session* s, FS::DeviceKind kind, size_t index, void* device) {
    const auto kindIndex = static_cast<uint8_t>(kind);
    if (kindIndex >= FS::kDeviceKindCount || index >= kMaxDevices) return;
    const DeviceFrame frame{PeerFrame{s, 0}, device};
    if (!frame.Current() || frame.peer.role != coop::net::Role::Client) return;
    const uint32_t key = ShadowKey(kind, index);
    const auto order = g_canonicalOrder[kindIndex][index];
    const uint64_t request = ++g_clientRequest;
    g_retry.insert(key);
    if (g_clientClaiming) return;
    BusyScope busy(g_clientClaiming);
    const auto current = [&] {
        const auto latest = g_canonicalOrder[kindIndex][index];
        return frame.Current() && request == g_clientRequest &&
            latest.known == order.known && latest.sequence == order.sequence;
    };
    uint64_t digest = 0, before = 0, after = 0;
    if (!FS::ReadDigest(kind, device, digest, ScopeOf(kind)) || !current()) return;
    if (!FS::ReadDigest(kind, device, before, FS::DigestScope::Whole) || !current()) return;
    Slot cur;
    if (!ReadSlotOf(kind, device, cur, current)) return;
    if (!FS::ReadDigest(kind, device, after, FS::DigestScope::Whole) || !current() || before != after) return;
    std::vector<uint8_t> body;
    if (!PackOne(static_cast<uint8_t>(index), cur, body)) {
        ParkUnsendable(kind, index, device, "past the size ceiling", body.size(), current);
        return;
    }
    std::vector<uint8_t> blob;
    blob.push_back(kOpClaim);
    blob.push_back(static_cast<uint8_t>(kind));
    blob.insert(blob.end(), body.begin(), body.end());
    PrimeQuadIfLaptop(kind);
    if (!current()) return;
    if (!FS::ReadDigest(kind, device, after, FS::DigestScope::Whole) || !current() || before != after) return;
    const bool sent = coop::blob_chunks::SendBlobWhile(s, coop::net::ReliableKind::FloppySlotState,
                                                       g_nextSeq++, blob, current);
    if (!current()) return;
    if (sent) {
        // The claim is a report, not the truth: prime on the SENT state so the poll stops
        // re-claiming, and let the host's canonical correct it if the host said otherwise.
        g_shadow[key] = digest;
        g_retry.erase(key);
        g_unsendable.erase(key);
        auto& aw = g_awaiting[key];
        if (aw.deadline == 0) aw.tries = 0;
        aw.deadline = NowMs() + kAnswerMs;
        UE_LOGI("floppy_slot_sync: CLIENT claim sent (device=%zu type=%d rw=%d rows=%zu json=%zu entry=%zu B "
                "key '%ls')", index, cur.st.floppyType, cur.st.readWrites, cur.c.data.size(),
                cur.c.objectData.size(), body.size(), FS::ObjectDataKey(cur.c.objectData).c_str());
    } else {
        g_retry.insert(key);
    }
}

void HostClaim(const PeerFrame& source, FS::DeviceKind kind, size_t index, void* device,
               const Slot& input) {
    if (index >= kMaxDevices || static_cast<uint8_t>(kind) >= FS::kDeviceKindCount) return;
    const DeviceFrame frame{source, device};
    if (!frame.Current() || source.role != coop::net::Role::Host) return;
    const uint32_t key = ShadowKey(kind, index);
    g_retry.insert(key);
    if (g_hostClaiming || g_hostPublishing || g_connectDraining) return;
    BusyScope busy(g_hostClaiming);
    const Slot in = input;
    const auto current = [&] { return frame.Current(); };
    const bool validType = FS::IsSlotType(in.st.floppyType);
    if (!current() || !validType) return;
    uint64_t before = 0, after = 0;
    if (!FS::ReadDigest(kind, device, before, FS::DigestScope::Whole) || !current()) return;
    Slot previous;
    if (!ReadSlotOf(kind, device, previous, current)) return;
    if (!FS::ReadDigest(kind, device, after, FS::DigestScope::Whole) || !current() || before != after) return;
    const bool applied = ApplySlot(kind, index, device, in, current);
    if (!current()) return;
    // A refused completion may already have written native state. Publish what
    // exists; never rerun an uncertain write to manufacture an acknowledgement.
    UE_LOGI("floppy_slot_sync: HOST claim device=%zu slot=%d completed=%d previousType=%d",
            index, source.slot, applied ? 1 : 0, previous.st.floppyType);
    if (!current()) return;
    HostBroadcastOne(source.session, kind, index, device, current);
}

// Every device's slot to one joiner. True when every blob was accepted; a refused one leaves the
// retry armed, because the 1 Hz sweep covers EDGES and a set nobody asked for is not an edge.
bool SendConnectSetOf(coop::net::Session* s, int peerSlot, FS::DeviceKind kind,
                      const std::function<bool()>& current) {
    if (!current() || !FS::EnsureResolved(kind) || !current()) return false;
    std::vector<void*> devices;
    const size_t n = ReadDevices(kind, devices);
    if (!current()) return false;
    std::vector<std::pair<ue_wrap::CachedObjRef, ue_wrap::CachedObjRef>> bindings;
    const auto bound = [&] {
        if (!current()) return false;
        for (const auto& item : bindings)
            if (!item.first.Get() || !item.second.Get() ||
                R::ClassOf(item.first.Get()) != item.second.Get()) return false;
        return true;
    };
    std::vector<std::pair<uint8_t, Slot>> entries;
    bool complete = n != 0;
    for (size_t i = 0; i < n; ++i) {
        if (!bound()) return false;
        void* d = devices[i];
        if (!d || !R::IsLive(d)) { complete = false; continue; }
        ue_wrap::CachedObjRef target, cls;
        target.Set(d); cls.Set(R::ClassOf(d));
        bindings.emplace_back(target, cls);
        uint64_t before = 0, after = 0;
        if (!FS::ReadDigest(kind, d, before, FS::DigestScope::Whole) || !bound()) {
            complete = false; continue;
        }
        Slot cur;
        if (!ReadSlotOf(kind, d, cur, bound)) { complete = false; continue; }
        if (!FS::ReadDigest(kind, d, after, FS::DigestScope::Whole) || !bound() || before != after) {
            complete = false; continue;
        }
        // No prime here: the shadow tracks what the host last BROADCAST, and this set goes to one
        // joiner. Priming it would swallow a change the already-connected peers have not had yet.
        entries.emplace_back(static_cast<uint8_t>(i), std::move(cur));
    }
    // Nothing to send is not the same as nothing to do: before the host's own world is up
    // ReadDevices answers zero, and treating that as a delivered set would leave the joiner
    // muted for the session -- it stays silent until a canonical lands.
    if (entries.empty()) return false;
    std::vector<uint8_t> dropped;
    auto blobs = PackCanonicalSet(kind, entries, &dropped);
    if (!dropped.empty()) complete = false;
    int sent = 0;
    for (auto& b : blobs) {
        if (!bound()) return false;
        if (coop::blob_chunks::SendBlobToSlotWhile(s, peerSlot, coop::net::ReliableKind::FloppySlotState,
                                                   g_nextSeq++, b, bound))
            ++sent;
        if (!bound()) return false;
    }
    const bool whole = complete && sent == static_cast<int>(blobs.size());
    UE_LOGI("floppy_slot_sync: connect set -> slot %d (kind %u: %zu device(s) in %d of %zu blob(s), generation %u)",
            peerSlot, static_cast<unsigned>(kind), entries.size(), sent, blobs.size(), GenOf(kind));
    // The laptop's file quad follows its slot on the same lane and under the same generation, so a joiner takes the
    // disc before its files, and a set the transport refused brings its quad with its retry.
    if (whole && kind == FS::DeviceKind::Laptop) coop::laptop_buffer_sync::QueueConnectBroadcastForSlot(peerSlot);
    return whole && bound();
}

// The boxes', then the laptop's: a kind that did not go out keeps the retry armed.
bool SendConnectSet(coop::net::Session* s, int peerSlot) {
    if (g_connectDraining || g_hostPublishing) return false;
    BusyScope busy(g_connectDraining);
    const HostFrame frame{s};
    const auto found = g_connectRetry.find(peerSlot);
    if (!frame.Current() || found == g_connectRetry.end()) return false;
    const ConnectRetry receipt = found->second;
    const auto current = [&] {
        if (!frame.Current()) return false;
        const auto row = g_connectRetry.find(peerSlot);
        return row != g_connectRetry.end() && row->second.revision == receipt.revision &&
            receipt.occupant && receipt.occupant == s->peerGenerationForSlot(peerSlot) &&
            s->IsSlotConnected(peerSlot) && s->IsSlotWorldReady(peerSlot);
    };
    if (!current()) return false;
    bool whole = true;
    for (FS::DeviceKind kind : kKinds) {
        const auto kindIndex = static_cast<uint8_t>(kind);
        if (receipt.sent[kindIndex]) continue;
        if (!SendConnectSetOf(s, peerSlot, kind, current)) { whole = false; break; }
        if (!current()) return false;
        g_connectRetry.at(peerSlot).sent[kindIndex] = true;
    }
    if (!current()) return false;
    if (whole) g_connectRetry.erase(peerSlot);
    else g_connectRetry.at(peerSlot).next = NowMs() + kConnectRetryMs;
    return whole;
}

// A joiner whose set the transport refused would otherwise keep a world it has never been told
// about: it primes its own slots and stays mute until one of them changes, which may be never.
void DrainConnectRetries(coop::net::Session* s) {
    const HostFrame frame{s};
    if (!frame.Current() || g_connectDraining || g_connectRetry.empty()) return;
    std::vector<int> slots;
    for (const auto& kv : g_connectRetry) slots.push_back(kv.first);
    for (int slot : slots) {
        if (!frame.Current()) return;
        auto it = g_connectRetry.find(slot);
        if (it == g_connectRetry.end()) continue;
        if (!s->IsSlotConnected(slot) || it->second.occupant != s->peerGenerationForSlot(slot)) {
            g_connectRetry.erase(it);
            continue;
        }
        if (!s->IsSlotWorldReady(slot) || NowMs() < it->second.next) continue;
        SendConnectSet(s, slot);
    }
}

// CLIENT: has the host answered the claim we sent for this slot? An answer is a canonical, and
// the host drops a claim silently for four different reasons, so a claim with no answer is
// re-sent a bounded number of times and then parked -- never left as a permanent divergence.
bool AnswerOverdue(uint32_t key, void* device, size_t index, FS::DeviceKind kind,
                   const std::function<bool()>& current = {}) {
    if (current && !current()) return false;
    auto it = g_awaiting.find(key);
    if (it == g_awaiting.end()) return false;
    if (NowMs() < it->second.deadline) return false;
    if (it->second.tries >= kMaxClaimTries) {
        g_awaiting.erase(it);
        ParkUnsendable(kind, index, device, "the host never answered the claim", 0, current);
        return false;
    }
    ++it->second.tries;
    it->second.deadline = NowMs() + kAnswerMs;
    return true;
}

bool RateAllows(uint8_t senderSlot, bool& warnNow) {
    const uint64_t now = NowMs();
    Rate& r = g_rate[senderSlot];
    if (now - r.windowStart >= kRateWindowMs) { r.windowStart = now; r.count = 0; r.warned = false; }
    if (r.count >= kMaxClaimsPerWindow) {
        warnNow = !r.warned;
        r.warned = true;
        return false;
    }
    ++r.count;
    return true;
}

// One kind's sweep: a device whose digest left the shadow is published (host) or claimed (client).
void SweepKind(coop::net::Session* s, FS::DeviceKind kind, bool host) {
    if (g_sweeping) return;
    BusyScope busy(g_sweeping);
    const HostFrame frame{s};
    const PeerFrame peer{s, 0};
    const auto rootCurrent = [&] { return host ? frame.Current() : peer.Current(); };
    if (!rootCurrent() || !FS::EnsureResolved(kind) || !rootCurrent()) return;
    std::vector<void*> devices;
    const size_t n = ReadDevices(kind, devices);
    for (size_t i = 0; i < n; ++i) {
        if (!rootCurrent()) return;
        void* d = devices[i];
        if (!d || !R::IsLive(d)) continue;
        ue_wrap::CachedObjRef target, cls;
        target.Set(d); cls.Set(R::ClassOf(d));
        const auto current = [&] {
            return rootCurrent() && target.Get() && cls.Get() && R::ClassOf(d) == cls.Get();
        };
        if (!current()) return;
        if(!host && g_pendingCanonical.count(ShadowKey(kind,i)))continue;
        uint64_t digest = 0;
        if (!FS::ReadDigest(kind, d, digest, ScopeOf(kind))) continue;
        if (!current()) return;
        const uint32_t key = ShadowKey(kind, i);
        auto it = g_shadow.find(key);
        if (it == g_shadow.end() && !(host && g_retry.count(key))) {
            // First sight primes silently: a prime is not an edge, and the joiner's own connect
            // set is what makes the two peers agree at the start.
            g_shadow[key] = digest;
            if (host && kind == FS::DeviceKind::Laptop)
                NoteLaptopOccupancy(d, [&] { return frame.Current() && target.Get(); });
            continue;
        }
        if (!host && !g_haveCanonical[static_cast<uint8_t>(kind)]) {
            // Pre-canonical, every local difference is this peer's own save loading. Prime it
            // away rather than claiming it.
            g_shadow[key] = digest;
            continue;
        }
        const bool overdue = !host && AnswerOverdue(key, d, i, kind, current);
        if (!current()) return;
        it = g_shadow.find(key);
        if (it != g_shadow.end() && it->second == digest && !g_retry.count(key) && !overdue) continue;
        if (host) HostBroadcastOne(s, kind, i, d);
        else      ClientClaim(s, kind, i, d);
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    if(g_session.load()!=session || g_boundWorld!=ue_wrap::world_identity::Generation() ||
       g_boundSerial!=coop::net::session_serial::Current())OnDisconnect();
    g_boundWorld=ue_wrap::world_identity::Generation();g_boundSerial=coop::net::session_serial::Current();
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if (!GT::IsGameThread() || g_ticking || g_hostPublishing || g_connectDraining ||
        g_clientClaiming || g_hostClaiming) return;
    BusyScope busy(g_ticking);
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    Install(s);
    const HostFrame frame{s};
    const uint64_t now = NowMs();
    if (now < g_nextSweep) return;
    g_nextSweep = now + kSweepMs;

    g_asm.Sweep(std::chrono::steady_clock::now(), std::chrono::seconds(10));

    const bool host = IsHost();
    const PeerFrame peer{s, 0};
    const auto current = [&] { return host ? frame.Current() : peer.Current(); };
    if (host) DrainConnectRetries(s);

    for (FS::DeviceKind kind : kKinds) {
        if (!current()) return;
        SweepKind(s, kind, host);
    }
    if (!host) {
        for (FS::DeviceKind kind : kKinds) {
            if (!current()) return;
            if (!FS::EnsureResolved(kind)) continue;
            if (!current()) return;
            std::vector<void*> devices;
            ReadDevices(kind, devices);
            if (!current()) return;
            DrainPendingCanonicals(kind, devices);
        }
    }
}

void OnChunk(const coop::net::BlobChunkPayload& p, uint8_t senderSlot) {
    if (!GT::IsGameThread()) {
        UE_LOGW("floppy_slot_sync: OnChunk off-game-thread -- dropping");
        return;
    }
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    Install(s);
    const PeerFrame source{s, senderSlot};
    if (!source.Current()) return;
    std::vector<uint8_t> blob;
    if (!g_asm.OnChunk(p, senderSlot, blob)) return;
    if (!source.Current()) return;
    if (blob.size() < 2) return;

    Reader r{blob.data(), blob.size()};
    const uint8_t op   = r.U8();
    const uint8_t kindB = r.U8();
    if (kindB >= ue_wrap::floppy_slot::kDeviceKindCount) {
        UE_LOGW("floppy_slot_sync: unknown device kind %u from slot %u -- dropped",
                static_cast<unsigned>(kindB), static_cast<unsigned>(senderSlot));
        return;
    }
    const auto kind = static_cast<FS::DeviceKind>(kindB);
    const bool host = IsHost();

    if (op == kOpClaim) {
        if (!host) return;  // a client never takes another peer's claim; only the host's canonical
        if (!FS::EnsureResolved(kind)) {
            UE_LOGW("floppy_slot_sync: claim arrived before device kind %u resolved -- dropped; "
                    "the client's answer deadline will retry it", static_cast<unsigned>(kindB));
            return;
        }
        if (!source.Current()) return;
        std::vector<void*> devices;
        const size_t n = ReadDevices(kind, devices);
        if (!source.Current()) return;
        if (blob.size() > coop::blob_chunks::MaxBlobBytes()) {
            UE_LOGW("floppy_slot_sync: claim from slot %u is %zu B, past the %zu B ceiling -- "
                    "dropped", static_cast<unsigned>(senderSlot), blob.size(), coop::blob_chunks::MaxBlobBytes());
            return;
        }
        bool warnNow = false;
        if (!RateAllows(senderSlot, warnNow)) {
            if (warnNow)
                UE_LOGW("floppy_slot_sync: slot %u past %d claims per %llu ms -- dropping until "
                        "the window turns", static_cast<unsigned>(senderSlot),
                        kMaxClaimsPerWindow, static_cast<unsigned long long>(kRateWindowMs));
            return;
        }
        // Include the index byte, just as PackOne does. Reject before native mutation:
        // a claim the host cannot publish would leave every other peer with older state.
        if (blob.size() - r.off > kMaxSlotBytes) return;
        const uint8_t index = r.U8();
        // The entry is what the host re-publishes, so the ceiling is the sender's own: an entry the host could apply
        // and then not pass on would leave it holding a disc no peer can be given.
        const size_t entryBytes = blob.size() - r.off + 1;
        if (entryBytes > coop::disc_record::MaxBodyBytes()) {
            if (g_refusedOversize.insert(ShadowKey(kind, index)).second)
                UE_LOGW("floppy_slot_sync: claim for device %u from slot %u is %zu B, past the %zu B ceiling -- "
                        "refused (said once per device)",
                        static_cast<unsigned>(index), static_cast<unsigned>(senderSlot), entryBytes,
                        coop::disc_record::MaxBodyBytes());
            return;
        }
        Slot in;
        if (!ParseSlot(r, in) || r.off != blob.size()) {
            UE_LOGW("floppy_slot_sync: malformed claim from slot %u -- dropped",
                    static_cast<unsigned>(senderSlot));
            return;
        }
        if (index >= n || !devices[index] || !R::IsLive(devices[index])) {
            UE_LOGW("floppy_slot_sync: claim from slot %u names device %u of %zu -- dropped",
                    static_cast<unsigned>(senderSlot), static_cast<unsigned>(index), n);
            return;
        }
        HostClaim(source, kind, index, devices[index], in);
        return;
    }

    if (op != kOpCanonical) return;
    if (host) {
        UE_LOGW("floppy_slot_sync: canonical received on the HOST from slot %u -- dropped",
                static_cast<unsigned>(senderSlot));
        return;
    }
    if (senderSlot != 0) {
        UE_LOGW("floppy_slot_sync: canonical from non-host slot %u -- dropped",
                static_cast<unsigned>(senderSlot));
        return;
    }
    const uint32_t gen = r.U32();
    const uint16_t count = r.U16();
    if (count > kMaxDevices) {
        UE_LOGW("floppy_slot_sync: canonical names %u devices, past the %zu that can exist -- "
                "dropped", static_cast<unsigned>(count), kMaxDevices);
        return;
    }
    std::vector<std::pair<uint8_t, Slot>> parsed;
    parsed.reserve(count);
    for (uint16_t k = 0; k < count && r.ok; ++k) {
        const uint8_t index = r.U8();
        Slot in;
        if (!ParseSlot(r, in)) break;
        const bool validType = FS::IsSlotType(in.st.floppyType);
        if (!source.Current()) return;
        if (index >= kMaxDevices || !validType) {
            UE_LOGW("floppy_slot_sync: canonical carries invalid device=%u type=%d -- whole "
                    "canonical dropped", static_cast<unsigned>(index), in.st.floppyType);
            return;
        }
        parsed.emplace_back(index, std::move(in));
    }
    if (!r.ok || parsed.size() != count || r.off != blob.size()) {
        UE_LOGW("floppy_slot_sync: malformed canonical (%zu of %u device(s), %zu trailing B) -- "
                "whole canonical dropped", parsed.size(), count,
                r.off <= blob.size() ? blob.size() - r.off : 0);
        return;
    }
    for (auto& entry : parsed) {
        if (!source.Current()) return;
        StageCanonical(kind,entry.first,std::move(entry.second),gen,p.blobSeq);
    }
    std::vector<void*> devices;
    if (FS::EnsureResolved(kind)) ReadDevices(kind, devices);
    if (!source.Current()) return;
    const size_t before = g_pendingCanonical.size();
    DrainPendingCanonicals(kind, devices);
    const size_t applied = before - g_pendingCanonical.size();
    UE_LOGI("floppy_slot_sync: CLIENT canonical -- %zu device(s) reconciled, %zu staged until "
            "their local targets exist", applied, g_pendingCanonical.size());
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host ||
        peerSlot <= 0 || peerSlot >= coop::net::kMaxPeers || !s->IsSlotConnected(peerSlot)) return;
    Install(s);
    g_connectRetry[peerSlot] = {s->peerGenerationForSlot(peerSlot), ++g_connectRevision, 0};
    SendConnectSet(s, peerSlot);
}

uint32_t LaptopGeneration() { return g_laptopGen; }

bool LaptopOccupancySettled() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return true;  // no session: nobody to agree with
    const HostFrame host{s};
    const PeerFrame peer{s, 0};
    const bool isHost = s->role() == coop::net::Role::Host;
    const auto rootCurrent = [&] { return isHost ? host.Current() : peer.Current(); };
    if (!rootCurrent()) return false;
    constexpr auto kLaptop = FS::DeviceKind::Laptop;
    void* laptop = ue_wrap::laptop::EnsureResolved() ? ue_wrap::laptop::Instance() : nullptr;
    if (!rootCurrent() || !laptop || !R::IsLive(laptop)) return false;
    ue_wrap::CachedObjRef target, cls;
    target.Set(laptop); cls.Set(R::ClassOf(laptop));
    const auto current = [&] {
        return rootCurrent() && target.Get() && cls.Get() && R::ClassOf(laptop) == cls.Get();
    };
    uint64_t d = 0;
    if (!current() || !FS::EnsureResolved(kLaptop) || !current() ||
        !FS::ReadDigest(kLaptop, laptop, d, FS::DigestScope::Occupancy) || !current())
        return false;
    const uint32_t key = ShadowKey(kLaptop, 0);
    auto it = g_shadow.find(key);
    if (it == g_shadow.end() || it->second != d) return false;  // an edge this lane has not taken yet
    if (IsHost()) return !g_retry.count(key);                     // published
    return g_haveCanonical[static_cast<uint8_t>(kLaptop)] && !g_awaiting.count(key);  // the host answered its claim
}

void DevClaimLaptopNow() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || IsHost()) return;
    SweepKind(s, FS::DeviceKind::Laptop, false);
}

void OnPeerGone(uint8_t senderSlot) {
    if (senderSlot >= coop::net::kMaxPeers) return;
    ++g_peerEpoch[senderSlot];
    g_asm.ClearSlot(senderSlot);
    g_rate.erase(senderSlot);
    g_connectRetry.erase(senderSlot);
}

void OnDisconnect() {
    ++g_pendingEpoch;
    g_shadow.clear();
    g_retry.clear();
    g_unsendable.clear();
    g_refusedOversize.clear();
    g_awaiting.clear();
    g_pendingCanonical.clear();
    g_pendingCanonicalGen.clear();
    g_pendingCanonicalRevision.clear();
    g_pendingWriteDebt.clear();
    g_pendingCursor.fill(0);
    g_canonicalOrder={};
    g_connectRetry.clear();
    for (bool& have : g_haveCanonical) have = false;
    g_laptopGen = 0;
    g_laptopGenDigest = 0;
    g_laptopGenKnown = false;
    FS::ResetCache();
    ue_wrap::floppy_disc::ResetCache();  // the disc's class and offsets are world-scoped too
    g_rate.clear();
    g_asm.Clear();
    g_nextSweep = 0;
    g_session.store(nullptr, std::memory_order_release);
}

}  // namespace coop::floppy_slot_sync
