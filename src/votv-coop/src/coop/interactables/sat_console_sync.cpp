// coop/interactables/sat_console_sync.cpp -- see coop/interactables/sat_console_sync.h.

#include "coop/interactables/sat_console_sync.h"

#include "coop/interactables/sat_console_table.h"
#include "coop/items/save_record_wire.h"  // the blob's read and append primitives
#include "coop/net/blob_chunks.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/session/player_handshake.h"  // GuidForSlot: a typist's durable identity

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/gc_pin.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/console_desk.h"  // Instance: the desk a host terminal names as its panel
#include "ue_wrap/desk/dish.h"          // IndexOf, DishByIndex, Count
#include "ue_wrap/desk/sat_console.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/engine/world_identity.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <deque>
#include <cwctype>
#include <string>
#include <utility>
#include <vector>
#include <optional>

namespace coop::sat_console_sync {
namespace {

namespace SC = ue_wrap::sat_console;
namespace T  = coop::sat_console_table;
namespace W  = coop::save_record_wire;
namespace GT = ue_wrap::game_thread;
namespace WI = ue_wrap::world_identity;
namespace sg = ue_wrap::script_gate;
using Clock = std::chrono::steady_clock;
using coop::net::kMaxPeers;
using coop::net::ReliableKind;

// The blobs. A dish is named by its gamemode.dishs index, a panel by its placed actor's name.
constexpr uint8_t kOpLine = 0;  // typist -> host: [i32 dish][str name][str used panel][str line]
constexpr uint8_t kOpLog  = 1;  // host -> every display: [u8 bracketsType][str text]
constexpr uint8_t kOpBusy = 2;  // host -> typist: [u8 busy], the level, on each change and as a seed
constexpr uint8_t kOpLocalLog = 3; // client -> host: text from a command that runs locally, never execute it

constexpr int32_t kNoDish = -1;  // init with no dish: the ROOT console

constexpr size_t kMaxLineChars   = 255;   // MTA's clamp too; every command name is short
constexpr size_t kMaxNameChars   = 64;
constexpr size_t kMaxLogChars    = 8192;  // `debug rules` prints the game rules as JSON, a few thousand
constexpr int    kLinesPerWindow = 4;     // a person types slower: lines a typist may run per window
constexpr auto   kWindow         = std::chrono::seconds(1);
constexpr auto   kSpawnEvery     = std::chrono::seconds(2);  // a typist's spawn-on-every-call commands
constexpr size_t kMaxTerminals   = coop::net::kPeerCapacity - 1; // one per remote participant
constexpr size_t kOutboxCap      = 512;   // blobs owed to one slot while its sends are refused
constexpr auto   kResendEvery    = std::chrono::milliseconds(250);  // a refused slot is tried this often
const wchar_t* const kBusyLine = L"<r>The terminal is busy</>";  // the game's answer to a line it cannot take
const wchar_t* const kErrLine  = L"<r>err</>";                   // the game's answer to a line it cannot run

UE_FACT_FN(kRowEnterCommand, "sat_console_sync", L"ui_console_C", L"enterCommand", Declared);
UE_FACT_FN(kRowUbergraph, "sat_console_sync", L"ui_console_C", L"ExecuteUbergraph_ui_console", Declared);
UE_FACT_FN(kRowWriteToLog, "sat_console_sync", L"ui_console_C", L"writeToLog", Declared);
constexpr int kTagEnter = 0x53434E45;  // 'SCNE'
constexpr int kTagWrite = 0x53434E57;  // 'SCNW'
constexpr int kTagGraph = 0x53434E47;  // 'SCNG'

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_watched = false;
Clock::time_point g_watchRetry{};
bool g_saidWatchFailure=false;
coop::blob_chunks::Assembler g_asm;
uint32_t g_seq = 1;
Clock::time_point g_nextSweep{};
Counts g_counts{};
bool g_saidOffThread = false;

// Blobs owed to a slot whose send was refused. A blob goes whole or not at all, and one refused waits
// at the head, so a slot's blobs arrive in the order they were made. The host keeps one per typist's
// slot; a client keeps slot 0's, to the host. At its cap an outbox gives up its oldest blob that is not a
// busy level, which the typist's terminal needs to stop being busy.
struct Outbox {
    std::deque<std::vector<uint8_t>> q;
    std::deque<uint64_t> ids;
    Clock::time_point nextTry{};
    bool saidFull = false;
};
Outbox g_out[kMaxPeers];
bool g_flushing[kMaxPeers]{},g_seeding[kMaxPeers]{},g_seedOwed[kMaxPeers]{};
bool g_peerKnown[kMaxPeers]{};
uint32_t g_peerGeneration[kMaxPeers]{};
uint64_t g_peerVersion[kMaxPeers]{},g_seedVersion[kMaxPeers]{};
std::string g_peerGuid[kMaxPeers];
uint64_t g_queueRevision=0;
unsigned g_batching=0;

// HOST: the terminal each typist runs on, keyed by its durable identity, not its slot: a typist that
// leaves mid-command leaves its terminal running that command, as the game's terminal runs on when its
// player walks away, and a typist that comes back finds it again. A terminal lasts while the session
// does and its world stays current; the last client's leave ends the session, and each terminal is
// then discarded, its command stopping where it stands.
struct Terminal {
    std::string guid;
    ue_wrap::GcPin pin;
    void*   world = nullptr;  // the world it was made in, when known
    int     slot = -1;        // its typist's slot while connected, -1 once it left
    bool    busySent = false; // the busy level its typist last got
    bool    haveContext = false;
    bool    clearing = false;
    int32_t dish = kNoDish;
    std::wstring name;
    int     lines = 0;        // lines run in the current window
    Clock::time_point windowStart{};
    Clock::time_point nextSpawnOk{};
};
std::vector<Terminal> g_terms;
bool g_saidRefused[kMaxPeers] = {};
bool g_saidMalformed[kMaxPeers] = {};
bool g_saidNoTerminal[kMaxPeers] = {};
bool g_saidNoContext[kMaxPeers] = {};

// CLIENT: the host's busy level for this terminal, and whether a line this terminal ran itself holds
// the flag: the mirror sets it while the host's terminal is busy and clears only what it set.
bool g_mirrorBusy = false;
bool g_localHolds = false;
bool g_haveMirrorBusy=false;
ue_wrap::CachedObjRef g_busyTerm;
// Local commands keep their effects on the typist; only the host orders display
// writes. Unsolicited widget alerts have one producer, the host's visible terminal.
struct Surface {
    coop::net::Session* session=nullptr;
    uint64_t epoch=0,binding=0;
    uint32_t generation=0;
    ue_wrap::CachedObjRef term;
    bool visible=false;
};
struct BodyFrame {
    Surface source; uint64_t invocation=0; bool command=false,clear=false;
    ue_wrap::CachedObjRef function;
    void* stack=nullptr;
    uint8_t* locals=nullptr;
    int tag=0;
};
std::vector<BodyFrame> g_bodies;
Surface g_asyncCommand;
bool g_commandBusy=false,g_draining=false;
uint64_t g_commandRevision=0,g_busyRevision=0,g_displayRevision=0;
struct InputScope { Surface source; InputScope* previous=nullptr; };
InputScope* g_input=nullptr;
struct ReplayScope {
    Surface source; bool claimed=false; uint64_t invocation=0; ReplayScope* previous=nullptr;
    std::wstring text;uint8_t type=0;bool probing=false;
};
ReplayScope* g_replay=nullptr;
struct DisplayLine { std::wstring text; uint8_t type = 0; };
std::deque<DisplayLine> g_display;
constexpr size_t kDisplayCap = 128;
// Recent submitted-command output, including errors. Replayed as text only;
// reconnecting never executes a previous command or starts its event again.
std::deque<DisplayLine> g_history;
size_t g_historyChars=0;
bool g_haveHistory=false;
struct LogRate { Clock::time_point start{}; size_t chars=0, lines=0; };
LogRate g_logRate[kMaxPeers]{};
uint32_t g_world = 0;
uint64_t g_epoch = 0;
uint64_t g_serial=0,g_historyRevision=0;
coop::net::Role g_role=coop::net::Role::Host;

bool Current(coop::net::Session* s,uint64_t epoch) {
    return s && g_session.load()==s && epoch==g_epoch && s->running() && s->connected() &&
        s->role()==g_role && WI::Generation()==g_world && coop::net::session_serial::Current()==g_serial;
}

bool Recipient(coop::net::Session* s,uint8_t slot,uint32_t generation,const std::string& guid,uint64_t epoch,uint64_t binding) {
    return slot<kMaxPeers && g_peerKnown[slot] && g_peerVersion[slot]==binding && Current(s,epoch) && s->peerGenerationForSlot(slot)==generation &&
        (s->role()==coop::net::Role::Client ? slot==0 :
        slot>0 && s->IsSlotConnected(slot) && s->IsSlotWorldReady(slot) && coop::player_handshake::GuidForSlot(slot)==guid);
}

bool BindPeer(coop::net::Session* s,uint8_t slot) {
    if(slot>=kMaxPeers || !Current(s,g_epoch))return false;
    if(s->role()==coop::net::Role::Host ? slot==0 || !s->IsSlotConnected(slot) || !s->IsSlotWorldReady(slot) : slot!=0)return false;
    const auto generation=s->peerGenerationForSlot(slot);
    const std::string guid=s->role()==coop::net::Role::Host?coop::player_handshake::GuidForSlot(slot):std::string{};
    if(g_peerKnown[slot] && (g_peerGeneration[slot]!=generation || g_peerGuid[slot]!=guid)) {
        OnPeerGone(slot);
        g_seedOwed[slot]=s->role()==coop::net::Role::Host;++g_seedVersion[slot];
    }
    g_peerKnown[slot]=true;g_peerGeneration[slot]=generation;g_peerGuid[slot]=guid;
    return true;
}

coop::net::Session* SessionIf(coop::net::Role role) {
    auto* s = g_session.load(std::memory_order_acquire);
    return (Current(s,g_epoch) && s->role() == role) ? s : nullptr;
}

Surface CaptureSurface(void* term,bool visible) {
    Surface source;source.session=g_session.load();source.epoch=g_epoch;source.visible=visible;
    if(!Current(source.session,source.epoch))return source;
    if(source.session->role()==coop::net::Role::Client) {
        if(!BindPeer(source.session,0))return source;
        source.generation=g_peerGeneration[0];source.binding=g_peerVersion[0];
    }
    source.term.Set(term);
    return source;
}

bool SurfaceCurrent(const Surface& source) {
    auto* s=source.session;
    return Current(s,source.epoch) && source.term.Get() &&
        (!source.visible || source.term.Raw()==SC::LocalTerminal()) &&
        Current(s,source.epoch) &&
        (s->role()!=coop::net::Role::Client || Recipient(s,0,source.generation,{},source.epoch,source.binding));
}

void PruneBodies() {
    std::erase_if(g_bodies,[](const BodyFrame& frame){return !sg::IsInvocationActive(frame.invocation);});
}

std::optional<BodyFrame> ActiveBody(void* term,bool commandsOnly=false) {
    for(auto it=g_bodies.rbegin();it!=g_bodies.rend();++it)
        if(it->source.term.Raw()==term && (!commandsOnly || it->command) && sg::IsInvocationActive(it->invocation))return *it;
    return std::nullopt;
}

sg::Verdict TrackBody(const sg::Call& c,const Surface& source,bool command=false,bool clear=false) {
    PruneBodies();
    if(!SurfaceCurrent(source) || !c.invocation || g_bodies.size()>=64)return sg::Verdict::Cancel;
    BodyFrame frame{source,c.invocation,command,clear};
    frame.function.Set(c.function);frame.stack=c.stack;frame.locals=c.locals;frame.tag=c.tag;
    g_bodies.push_back(std::move(frame));
    return sg::Verdict::Run;
}

sg::Verdict OnBodyPre(const sg::Call& c) {
    if(!Current(g_session.load(),g_epoch))return sg::Verdict::Run;
    const bool visible=c.object==SC::LocalTerminal();
    if(!visible && !std::any_of(g_terms.begin(),g_terms.end(),[&](const Terminal& t){return t.pin.Raw()==c.object;}))return sg::Verdict::Run;
    return TrackBody(c,CaptureSurface(c.object,visible));
}

// A client's text, as the host's log shows it: printable characters only, so a line cannot forge one.
std::wstring Printable(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t ch : s) out.push_back(ch < 0x20 || ch == 0x7F ? L'?' : ch);
    return out;
}

void Flush(uint8_t slot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!BindPeer(s,slot) || g_flushing[slot]) return;
    g_flushing[slot]=true;
    struct Guard{uint8_t slot;~Guard(){g_flushing[slot]=false;}} guard{slot};
    const auto epoch=g_epoch;
    const auto generation=g_peerGeneration[slot];const auto guid=g_peerGuid[slot];
    const auto binding=g_peerVersion[slot];
    Outbox& o = g_out[slot];
    while (!o.q.empty()) {
        const auto id=o.ids.front();const auto blob=o.q.front();
        const auto current=[&]{return Recipient(s,slot,generation,guid,epoch,binding) && !o.ids.empty() && o.ids.front()==id;};
        if(!current())return;
        const bool sent=coop::blob_chunks::SendBlobToSlotWhile(s,slot,ReliableKind::SatConsole,g_seq++,blob,current);
        if(!current())return;
        if (!sent) {
            o.nextTry = Clock::now() + kResendEvery;
            return;
        }
        o.q.pop_front();o.ids.pop_front();
    }
    o.saidFull = false;
}

void Enqueue(uint8_t slot, std::vector<uint8_t> blob) {
    auto* s=g_session.load();if(!BindPeer(s,slot))return;
    Outbox& o = g_out[slot];
    if (o.q.size() >= kOutboxCap) {
        bool removed=false;
        for (auto it = o.q.begin(); it != o.q.end(); ++it) {
            if (!it->empty() && (*it)[0] != kOpBusy) {
                o.ids.erase(o.ids.begin()+std::distance(o.q.begin(),it));
                o.q.erase(it);
                removed=true;
                break;
            }
        }
        // Busy is a level. When only old levels remain, the newest one still
        // supersedes the oldest; a stalled peer cannot grow this queue forever.
        if(!removed){o.q.pop_front();o.ids.pop_front();}
        if (!o.saidFull) {
            o.saidFull = true;
            UE_LOGW("sat_console: %zu blobs owed to slot %u and its sends still refused -- the oldest line dropped",
                    kOutboxCap, static_cast<unsigned>(slot));
        }
    }
    o.q.push_back(std::move(blob));
    o.ids.push_back(++g_queueRevision);
    if (!g_batching && Clock::now() >= o.nextTry) Flush(slot);
}

void SendLog(uint8_t slot, const std::wstring& text, uint8_t bracketsType) {
    std::vector<uint8_t> b;
    W::AppU8(b, kOpLog);
    W::AppU8(b, bracketsType);
    W::AppWStr(b, text);
    Enqueue(slot, std::move(b));
}

void QueueDisplay(const std::wstring& text, uint8_t type) {
    if(type==3){g_display.clear();++g_displayRevision;} // everything queued before clr is superseded
    if (g_display.size() >= kDisplayCap) {
        // Keep the pending clear when the atlas opens after a burst of replies.
        if(g_display.front().type==3)g_display.erase(g_display.begin()+1);
        else g_display.pop_front();
    }
    g_display.push_back({text,type});
}

void ShareLog(const std::wstring& text, uint8_t type, int excludeSlot, bool showHere) {
    auto* s = SessionIf(coop::net::Role::Host);
    if (!s || !s->connected()) return;
    const auto epoch=g_epoch;
    g_haveHistory=true;++g_historyRevision;
    if(type==3){g_history.clear();g_historyChars=0;}
    else {
        g_history.push_back({text,type});g_historyChars+=text.size();
        while(g_history.size()>64 || (g_historyChars>kMaxLogChars && g_history.size()>1)) {
            g_historyChars-=g_history.front().text.size();g_history.pop_front();
        }
    }
    if (showHere) QueueDisplay(text,type);
    {
        // Every observer receives this row before a send can reenter with the
        // next row/clear. Otherwise a late observer can receive A after clr.
        ++g_batching;struct Guard{~Guard(){--g_batching;}} guard;
        for (int slot=1; slot<kMaxPeers; ++slot) {
            if(!Current(s,epoch))return;
            if (slot!=excludeSlot && s->IsSlotWorldReady(slot)) SendLog(uint8_t(slot),text,type);
        }
    }
    if(!g_batching)for(int slot=1;slot<kMaxPeers;++slot){
        if(!Current(s,epoch))return;
        if(slot!=excludeSlot && Clock::now()>=g_out[slot].nextTry)Flush(uint8_t(slot));
    }
}

Terminal* ByGuid(const std::string& guid);
void Rebind(Terminal& t,uint8_t slot);
void SendBusy(uint8_t slot,bool busy);

void SeedHistory(uint8_t slot) {
    auto* s=SessionIf(coop::net::Role::Host);
    if(!BindPeer(s,slot) || !g_seedOwed[slot] || g_seeding[slot])return;
    g_seeding[slot]=true;
    struct Guard{uint8_t slot;~Guard(){g_seeding[slot]=false;}} guard{slot};
    const auto epoch=g_epoch,revision=g_historyRevision;
    const auto request=g_seedVersion[slot],binding=g_peerVersion[slot];
    const auto generation=g_peerGeneration[slot];const auto guid=g_peerGuid[slot];
    const auto history=g_history;
    const auto current=[&]{return Recipient(s,slot,generation,guid,epoch,binding) && revision==g_historyRevision && request==g_seedVersion[slot];};
    // Supersede unsent history/live text with one ordered current snapshot.
    g_out[slot]=Outbox{};
    if(g_haveHistory) {
        SendLog(slot,L"",3);if(!current())return;
        for(const auto& line:history){SendLog(slot,line.text,line.type);if(!current())return;}
    }
    // Replacing queued history also replaced any busy debt. Seed its current
    // level again, including false when this typist has no private terminal.
    if(auto* term=ByGuid(guid))Rebind(*term,slot);
    else SendBusy(slot,false);
    if(current())g_seedOwed[slot]=false;
}

void DrainDisplay() {
    if(g_draining || !Current(g_session.load(),g_epoch))return;
    g_draining=true;struct Guard{~Guard(){g_draining=false;}} guard;
    for (int budget=8; budget && !g_display.empty(); --budget) {
        void* term=SC::LocalTerminal(); if(!term)return;
        const auto source=CaptureSurface(term,true);if(!SurfaceCurrent(source))return;
        if(g_display.empty())return; // binding discovery may have retired old-host display debt
        const auto revision=g_displayRevision;
        auto line=std::move(g_display.front()); g_display.pop_front();
        ReplayScope replay{source,false,0,g_replay,line.text,line.type==3?uint8_t(0):line.type};g_replay=&replay;
        struct ReplayGuard{ReplayScope* previous;~ReplayGuard(){g_replay=previous;}} replayGuard{replay.previous};
        const bool ok=line.type==3 ? SC::ClearLog(term) : SC::CallWriteToLog(term,line.text,line.type);
        if(!Current(source.session,source.epoch) || revision!=g_displayRevision)return;
        if(source.session->role()==coop::net::Role::Client &&
           !Recipient(source.session,0,source.generation,{},source.epoch,source.binding)) {
            BindPeer(source.session,0);return;
        }
        if(!ok || !SurfaceCurrent(source)) {
            if(g_display.size()<kDisplayCap)g_display.push_front(std::move(line));
            return;
        }
    }
}

void SendBusy(uint8_t slot, bool busy) {
    std::vector<uint8_t> b;
    W::AppU8(b, kOpBusy);
    W::AppU8(b, busy ? 1 : 0);
    Enqueue(slot, std::move(b));
}

// A line the host will not run is answered as the game answers one it cannot take or run: its echo,
// then the reason's line. The typist already consumed it.
void AnswerRefused(const std::wstring& line, const wchar_t* why) {
    ShareLog(line,1,-1,true);
    ShareLog(why,0,-1,true);
}

bool IsClear(const std::wstring& line) {
    const auto word=line.substr(0,line.find(L' '));
    return word.size()==3 && std::towlower(word[0])==L'c' &&
        std::towlower(word[1])==L'l' && std::towlower(word[2])==L'r';
}

void ObserveLocalTerminal(void* term) {
    if(!g_busyTerm.Is(term)){g_localHolds=false;g_busyTerm.Set(term);++g_busyRevision;}
}

void ApplyMirrorBusy() {
    auto* s=SessionIf(coop::net::Role::Client);if(!s || !g_haveMirrorBusy)return;
    const auto epoch=g_epoch;void* term=SC::LocalTerminal();if(!term)return;
    ObserveLocalTerminal(term);
    const auto revision=g_busyRevision;
    bool busy=false;
    if(SC::ReadProcessing(term,busy) && Current(s,epoch) && revision==g_busyRevision && g_busyTerm.Is(term) &&
       (g_mirrorBusy || !g_localHolds) && busy!=g_mirrorBusy)SC::WriteProcessing(term,g_mirrorBusy);
}

// ---- the typist ----

// A line whose command rests on the shared world goes to the host, with the terminal's context as it
// stands: the dish its panel pointed it at (or ROOT), its name, and the panel last used. A busy
// terminal answers the line itself, as it would alone; so does a line the terminal owns. The body is
// refused, which leaves its own tail unrun, so the input is consumed here as that tail does
// (ui_console.enterCommand: `command = ""`, the text box emptied); the echo and every answer come back
// from the host.
sg::Verdict OnEnterPre(const sg::Call& c) {
    auto* s=g_session.load();
    if (!Current(s,g_epoch)) return sg::Verdict::Run;
    void* term = c.object;
    if (term != SC::LocalTerminal()) return OnBodyPre(c);
    const auto source=CaptureSurface(term,true);
    if(!SurfaceCurrent(source))return sg::Verdict::Cancel;
    for(auto* input=g_input;input;input=input->previous)
        if(input->source.epoch==source.epoch && input->source.term.Is(term))return sg::Verdict::Cancel;
    InputScope input{source,g_input};g_input=&input;
    struct Guard{InputScope* previous;~Guard(){g_input=previous;}} guard{input.previous};
    const auto current=[&]{return SurfaceCurrent(source);};
    if(s->role()==coop::net::Role::Client)ObserveLocalTerminal(term);
    bool busy = false;
    std::wstring line;
    const bool readLine=SC::ReadEnterCommandLine(c.locals,line);
    if(!current())return sg::Verdict::Cancel;
    if(!readLine)return s->role()==coop::net::Role::Host?TrackBody(c,source,true):sg::Verdict::Cancel;
    const bool readBusy=SC::ReadProcessing(term,busy);
    if(!current())return sg::Verdict::Cancel;
    if(s->role()!=coop::net::Role::Client || (readBusy && busy) || T::Classify(line)!=T::Runs::OnHost)
        return TrackBody(c,source,true,readBusy && !busy && IsClear(line));
    if(!readBusy)return sg::Verdict::Cancel;
    // Executing a shortened command can change its meaning. Preserve the input
    // so the typist can edit it and submit the intended complete line.
    if(line.size()>kMaxLineChars)return sg::Verdict::Cancel;
    std::wstring name;
    if (!SC::ReadName(term, name) || !current()) return sg::Verdict::Cancel;
    if (name.size() > kMaxNameChars) name.resize(kMaxNameChars);
    void* dish = SC::ReadActiveDish(term);
    if(!current())return sg::Verdict::Cancel;
    const int32_t dishIdx = dish ? ue_wrap::dish::IndexOf(dish) : kNoDish;
    if(!current())return sg::Verdict::Cancel;
    void* panel=SC::ReadUsed(term);
    if(!current())return sg::Verdict::Cancel;
    std::wstring used = SC::PanelName(panel);
    if(!current())return sg::Verdict::Cancel;
    if (used.size() > kMaxNameChars) used.clear();
    std::vector<uint8_t> b;
    W::AppU8(b, kOpLine);
    W::AppI32(b, dishIdx < 0 ? kNoDish : dishIdx);
    W::AppWStr(b, name);
    W::AppWStr(b, used);
    W::AppWStr(b, line);
    // Consume first: failed or retired native input must never have an accepted
    // world command behind it. A nested commit of this same input is refused.
    if(g_out[0].q.size()>=kOutboxCap || !SC::ConsumeInputWhile(term,current) || !current())return sg::Verdict::Cancel;
    Enqueue(0, std::move(b));
    if(current())++g_counts.linesSent;
    UE_LOGI("sat_console: CLIENT '%ls' runs on the host's terminal (dish %d)", line.c_str(), dishIdx);
    return sg::Verdict::Cancel;
}

// After this terminal ran a body of its own. While the host's terminal is busy the flag is the mirror's:
// a line this terminal ran itself may have cleared it, and it is set again, the line's hold over. Else
// the flag is the line's own.
void ClientAfterLocalBody(void* term) {
    const auto source=CaptureSurface(term,true);if(!SurfaceCurrent(source))return;
    ObserveLocalTerminal(term);
    const auto revision=++g_busyRevision;
    bool busy = false;
    if (!SC::ReadProcessing(term, busy) || !SurfaceCurrent(source) || revision!=g_busyRevision) return;
    if (g_mirrorBusy) {
        if (!busy) {
            g_localHolds = false;
            SC::WriteProcessing(term, true);
        }
    } else {
        g_localHolds = busy;
    }
}

void ClientOnBlob(const std::vector<uint8_t>& blob) {
    size_t o = 0;
    uint8_t op = 0;
    if (!W::RdU8(blob, o, op)) return;
    if (op == kOpLog) {
        uint8_t type = 0;
        std::wstring text;
        if (!W::RdU8(blob, o, type) || type>3 || !W::RdWStr(blob, o, text) ||
            text.size() > kMaxLogChars || (type==3 && !text.empty()) || o!=blob.size()) {
            UE_LOGW("sat_console: a malformed line from the host -- dropped");
            return;
        }
        QueueDisplay(text,type);
        return;
    }
    if (op == kOpBusy) {
        uint8_t busy = 0;
        if (!W::RdU8(blob, o, busy) || busy>1 || o!=blob.size()) return;
        g_mirrorBusy = busy != 0;
        ++g_busyRevision;
        g_haveMirrorBusy=true;ApplyMirrorBusy();
        return;
    }
    UE_LOGW("sat_console: op %u reached a client -- dropped", static_cast<unsigned>(op));
}

// ---- the host ----

Terminal* ByObject(void* obj) {
    if (!obj) return nullptr;
    for (Terminal& t : g_terms)
        if (t.pin.Raw() == obj) return &t;
    return nullptr;
}

Terminal* ByGuid(const std::string& guid) {
    for (Terminal& t : g_terms)
        if (t.guid == guid) return &t;
    return nullptr;
}

// Unpin a terminal and stop what it runs; its typist, if still here, is told it is no longer busy.
void Discard(Terminal& t) {
    void* term = t.pin.Raw();
    if (t.slot>0 && t.slot<kMaxPeers && t.busySent && coop::player_handshake::GuidForSlot(t.slot)==t.guid)
        SendBusy(static_cast<uint8_t>(t.slot), false);
    t.pin.Release();
    SC::DiscardTerminal(term);
}

void DiscardAt(size_t i) {
    Terminal retired=std::move(g_terms[i]);
    g_terms.erase(g_terms.begin() + static_cast<std::ptrdiff_t>(i));
    Discard(retired);
}

// A typist back at the terminal it left: bound to it again and told its busy level, so its own terminal
// is busy while the command it left runs on, and gets that command's lines from here on.
void Rebind(Terminal& t, uint8_t slot) {
    auto* s=SessionIf(coop::net::Role::Host);if(!s || !BindPeer(s,slot))return;
    const auto epoch=g_epoch;void* term=t.pin.Raw();const auto guid=t.guid;
    const auto generation=g_peerGeneration[slot];
    const auto binding=g_peerVersion[slot];
    if(g_peerGuid[slot]!=guid)return;
    t.slot = slot;
    bool busy = false;
    SC::ReadProcessing(term, busy);
    auto* current=ByObject(term);
    if(!Recipient(s,slot,generation,guid,epoch,binding) || !current || current->guid!=guid || current->slot!=slot)return;
    current->busySent = busy;
    ++g_counts.rebinds;
    if (busy) ++g_counts.busyRebinds;
    SendBusy(slot, busy);
    UE_LOGI("sat_console: HOST slot %u is back at its terminal (busy=%d)", static_cast<unsigned>(slot), busy ? 1 : 0);
}

// The typist's terminal, made on its first line. At the cap, a departed typist's idle terminal gives way;
// with none, the line is refused.
Terminal* TerminalFor(uint8_t slot, const std::string& requestedGuid) {
    auto* s=SessionIf(coop::net::Role::Host);if(!s || !BindPeer(s,slot))return nullptr;
    const auto epoch=g_epoch;const auto generation=g_peerGeneration[slot];
    const auto binding=g_peerVersion[slot];
    const std::string guid=requestedGuid;
    const auto current=[&]{return Recipient(s,slot,generation,guid,epoch,binding);};
    if(!current())return nullptr;
    if (Terminal* t = ByGuid(guid)) {
        if (t->slot != slot) Rebind(*t, slot);
        return current()?ByGuid(guid):nullptr;
    }
    if (g_terms.size() >= kMaxTerminals) {
        std::vector<void*> candidates;
        for(const auto& t:g_terms)if(t.slot<0)candidates.push_back(t.pin.Raw());
        for(void* object:candidates) {
            auto* t=ByObject(object);if(!t || t->slot>=0)continue;
            bool busy = true;
            const bool read=SC::ReadProcessing(object,busy);
            if(!current())return nullptr;
            t=ByObject(object);
            if(t && t->slot<0 && read && !busy) {
                DiscardAt(size_t(t-g_terms.data()));
                if(!current())return nullptr;
                break;
            }
        }
        if (g_terms.size() >= kMaxTerminals) return nullptr;
    }
    void* desk = ue_wrap::console_desk::Instance();
    void* term = desk ? SC::CreateTerminal(desk) : nullptr;
    if(current())if(auto* existing=ByObject(term))return existing->guid==guid?existing:nullptr;
    Terminal made;
    if (!term || !current() || !made.pin.Pin(term) || !current() || !SC::WritePanel(term, desk) || !current() ||
        g_terms.size()>=kMaxTerminals || ByGuid(guid)) {
        made.pin.Release();
        SC::DiscardTerminal(term);
        return nullptr;
    }
    made.guid = guid;
    made.world = WI::CurrentWorld();  // null while unknown: the world term then fails open
    made.slot = slot;
    g_terms.push_back(std::move(made));
    UE_LOGI("sat_console: HOST made slot %u's terminal (%zu kept)", static_cast<unsigned>(slot), g_terms.size());
    return &g_terms.back();
}

// Whether a terminal stands in the context a line names. A dish index that does not resolve yet, or a
// context whose init fails, answers the line err rather than running it against none; nothing is kept,
// so the next line with the same index tries again. An index past the host's dishes is no dish at all.
enum class Context { Ready, NotYet, Bad };

template<class IsCurrent>
Context ApplyContext(Terminal& t, int32_t dish, const std::wstring& name, const IsCurrent& currentRequest) {
    if (t.haveContext && dish == t.dish && name == t.name) return Context::Ready;
    void* d = nullptr;
    if (dish >= 0) {
        d = ue_wrap::dish::DishByIndex(dish);
        if (!d) {
            const int32_t count = ue_wrap::dish::Count();
            return count > 0 && dish >= count ? Context::Bad : Context::NotYet;
        }
    }
    // An init that fails may have set part of the context: nothing is kept, so the next line inits again.
    t.haveContext = false;
    const auto epoch=g_epoch;
    void* term=t.pin.Raw();
    if (!SC::CallInit(term, name, false, d) || epoch!=g_epoch || !currentRequest()) return Context::NotYet;
    auto* current=ByObject(term);
    if(!current)return Context::NotYet;
    current->haveContext = true;
    current->dish = dish;
    current->name = name;
    return Context::Ready;
}

void HostOnBlob(uint8_t slot, const std::vector<uint8_t>& blob) {
    auto* s=SessionIf(coop::net::Role::Host);if(!s || !BindPeer(s,slot))return;
    const auto requestEpoch=g_epoch,binding=g_peerVersion[slot];
    const auto generation=g_peerGeneration[slot];const auto requestGuid=g_peerGuid[slot];
    const auto currentRequest=[&]{return Recipient(s,slot,generation,requestGuid,requestEpoch,binding);};
    size_t o = 0;
    uint8_t op = 0;
    int32_t dish = kNoDish;
    std::wstring name, used, line;
    if (!blob.empty() && blob[0]==kOpLocalLog) {
        o=1; uint8_t type=0;
        if(!W::RdU8(blob,o,type) || type>3 || !W::RdWStr(blob,o,line) ||
           line.size()>kMaxLogChars || (type==3 && !line.empty()) || o!=blob.size())return;
        auto& rate=g_logRate[slot]; const auto now=Clock::now();
        if(now-rate.start>=kWindow)rate={now,0,0};
        if(rate.lines>=64 || rate.chars+line.size()>32768)return;
        ++rate.lines; rate.chars+=line.size();
        ShareLog(line,type,-1,true); // the origin waits for this same canonical echo
        return;
    }
    if (!W::RdU8(blob, o, op) || op != kOpLine || !W::RdI32(blob, o, dish) || !W::RdWStr(blob, o, name) ||
        !W::RdWStr(blob, o, used) || !W::RdWStr(blob, o, line) || name.size() > kMaxNameChars ||
        used.size() > kMaxNameChars || line.size() > kMaxLineChars || o!=blob.size()) {
        if (!g_saidMalformed[slot]) {
            g_saidMalformed[slot] = true;
            UE_LOGW("sat_console: HOST a malformed blob from slot %u -- dropped", static_cast<unsigned>(slot));
        }
        return;
    }
    // The host runs only a command a typist may send: a line the table keeps on its typist's machine
    // would run on the host's own compass, player or screen. A client that sends one was not built to.
    if (T::Classify(line) != T::Runs::OnHost) {
        if (!g_saidRefused[slot]) {
            g_saidRefused[slot] = true;
            UE_LOGW("sat_console: HOST refused '%ls' from slot %u -- it runs on its typist's machine",
                    Printable(line).c_str(), static_cast<unsigned>(slot));
        }
        return;
    }
    const std::string guid = requestGuid;
    Terminal* t = guid.empty() ? nullptr : TerminalFor(slot, guid);
    if(!currentRequest())return;
    if (!t) {
        if (!g_saidNoTerminal[slot]) {
            g_saidNoTerminal[slot] = true;
            UE_LOGW("sat_console: HOST no terminal for slot %u (%s) -- its lines are answered err",
                    static_cast<unsigned>(slot), guid.empty() ? "its identity not in yet" : "none could be made");
        }
        AnswerRefused(line, kErrLine);
        return;
    }
    const auto now = Clock::now();
    if (now - t->windowStart >= kWindow) {
        t->windowStart = now;
        t->lines = 0;
    }
    const bool spawns = T::SpawnsEveryCall(line);
    if (++t->lines > kLinesPerWindow || (spawns && now < t->nextSpawnOk)) {
        AnswerRefused(line, kBusyLine);  // the terminal cannot take the line, as the game says it
        return;
    }
    // A terminal still running a command keeps its context: its latent continuation reads the dish,
    // name and panel it started with. The game's own terminal refuses a line while busy, so does this.
    bool busy = false;
    void* requestTerm=t->pin.Raw();
    const bool readOk = SC::ReadProcessing(requestTerm, busy);
    if(!currentRequest() || !(t=ByObject(requestTerm)))return;
    if (!readOk || busy) {
        AnswerRefused(line, readOk ? kBusyLine : kErrLine);   // an unreadable flag is not "busy"
        return;
    }
    // The line runs only in the context it names. One that cannot be set up is answered err and not
    // retried: a command with an effect in the world must not run twice.
    const auto contextEpoch=g_epoch;
    void* contextTerm=t->pin.Raw();
    const Context ctx = ApplyContext(*t, dish < kNoDish ? kNoDish : dish, name, currentRequest);
    if(contextEpoch!=g_epoch || !currentRequest() || !(t=ByObject(contextTerm)))return;
    if (ctx != Context::Ready) {
        if (!g_saidNoContext[slot]) {
            g_saidNoContext[slot] = true;
            UE_LOGW("sat_console: HOST no context for slot %u's line (dish %d: %s) -- answered err",
                    static_cast<unsigned>(slot), dish,
                    ctx == Context::Bad ? "past the host's dishes" : "not resolved, or the init failed");
        }
        AnswerRefused(line, kErrLine);
        return;
    }
    // The panel the typist last used: none is a real answer (ROOT), a name that does not resolve here
    // is not, and the line does not run against no panel in its place.
    void* panel = used.empty() ? nullptr : SC::FindPanelByName(used);
    const bool panelReady=(used.empty() || panel) && SC::WriteUsed(contextTerm,panel);
    if(!currentRequest() || !(t=ByObject(contextTerm)))return;
    if (!panelReady) {
        if (!g_saidNoContext[slot]) {
            g_saidNoContext[slot] = true;
            UE_LOGW("sat_console: HOST no panel '%ls' for slot %u's line -- answered err",
                    Printable(used).c_str(), static_cast<unsigned>(slot));
        }
        AnswerRefused(line, kErrLine);
        return;
    }
    if (spawns) t->nextSpawnOk = now + kSpawnEvery;
    UE_LOGI("sat_console: HOST runs '%ls' for slot %u (dish %d)", Printable(line).c_str(),
            static_cast<unsigned>(slot), dish);
    t->clearing=IsClear(line);
    void* runTerm=t->pin.Raw();
    const auto epoch=g_epoch;
    const bool ran=SC::RunCommand(runTerm,line);
    if(epoch!=g_epoch || !currentRequest())return;
    if(auto* current=ByObject(runTerm))current->clearing=false;
    if (!ran) {
        AnswerRefused(line, kErrLine);
        return;
    }
    ++g_counts.linesRun;
}

// Every write of a terminal's busy flag lies in its enterCommand or its ubergraph, the latent
// continuations included, so the level is read after each and sent when it moved.
void HostAfterBody(Terminal& t) {
    const int slot=t.slot;const auto guid=t.guid;void* term=t.pin.Raw();
    auto* s=SessionIf(coop::net::Role::Host);
    if(slot<=0 || slot>=kMaxPeers || !s || !BindPeer(s,uint8_t(slot)) || g_peerGuid[slot]!=guid)return;
    const auto epoch=g_epoch,binding=g_peerVersion[slot];const auto generation=g_peerGeneration[slot];
    bool busy = false;
    const bool read=SC::ReadProcessing(term,busy);
    auto* current=ByObject(term);
    if(!Recipient(s,uint8_t(slot),generation,guid,epoch,binding) || !current || current->guid!=guid ||
       current->slot!=slot || !read || busy==current->busySent)return;
    current->busySent = busy;
    SendBusy(static_cast<uint8_t>(slot), busy);
}

sg::Verdict OnWritePre(const sg::Call& c) {
    // fromOurCode covers every nested reflected call. Only this one direct
    // invocation is the row being replayed; nested alerts still need ordering.
    if(g_replay && !g_replay->claimed && !g_replay->probing && c.fromOurCode && !c.callerFunction && g_replay->source.term.Raw()==c.object) {
        auto& replay=*g_replay;replay.probing=true;
        struct ProbeGuard{ReplayScope& row;~ProbeGuard(){row.probing=false;}} guard{replay};
        std::wstring text;uint8_t type=0;
        if(SC::ReadWriteToLogArgs(c.locals,text,type) && text==replay.text && type==replay.type) {
            replay.claimed=true;replay.invocation=c.invocation;
            return SurfaceCurrent(replay.source)?sg::Verdict::Run:sg::Verdict::Cancel;
        }
    }
    auto* s=g_session.load();
    const auto epoch=g_epoch;
    if (!Current(s,epoch)) return sg::Verdict::Run;
    PruneBodies();
    const auto active=ActiveBody(c.object);
    if(active && !SurfaceCurrent(active->source))return sg::Verdict::Cancel;
    Terminal* t = ByObject(c.object);
    const bool privateClearing=t && t->clearing;
    const bool visible=c.object==SC::LocalTerminal();
    if (!t && !visible) return sg::Verdict::Run;
    const auto source=CaptureSurface(c.object,visible);
    if(!SurfaceCurrent(source))return sg::Verdict::Cancel;
    const auto command=ActiveBody(c.object,true);
    const bool local=(command && SurfaceCurrent(command->source)) ||
        (g_commandBusy && g_asyncCommand.term.Is(c.object) && SurfaceCurrent(g_asyncCommand));
    const bool clearing=command && command->clear;
    // A client's periodic alerts describe its mirror. The host publishes these
    // once; forwarding every mirror would multiply them and change their order.
    if(s->role()==coop::net::Role::Client && !local)return sg::Verdict::Cancel;
    std::wstring text;
    uint8_t type = 0;
    if (!SC::ReadWriteToLogArgs(c.locals, text, type)) return sg::Verdict::Cancel;
    if (type>2 || text.size()>kMaxLogChars) return sg::Verdict::Cancel;
    // clr clears fields directly before its final empty writeToLog. Carry this
    // display operation in order with the command echo, without re-running clr.
    if(!SurfaceCurrent(source))return sg::Verdict::Cancel;
    if(((local && clearing) || privateClearing) && type==0 && text.empty())type=3;
    if(s->role()==coop::net::Role::Host) ShareLog(text,type,-1,true);
    else {
        std::vector<uint8_t> b;
        W::AppU8(b,kOpLocalLog);W::AppU8(b,type);W::AppWStr(b,text);Enqueue(0,std::move(b));
    }
    if(SurfaceCurrent(source))++g_counts.linesReturned;
    return sg::Verdict::Cancel;
}

void OnBodyPost(const sg::Call& c) {
    const auto found=std::find_if(g_bodies.begin(),g_bodies.end(),[&](const BodyFrame& frame){
        return frame.invocation==c.invocation && frame.source.term.Raw()==c.object &&
            frame.function.Raw()==c.function && (!c.function || frame.function.Get()) &&
            frame.stack==c.stack && frame.locals==c.locals && frame.tag==c.tag;
    });
    if(found==g_bodies.end())return;
    // A callback can install new frames. Retire this exact invocation before
    // reading native state, then commit only against its copied source token.
    const auto frame=*found;g_bodies.erase(found);
    if(!SurfaceCurrent(frame.source))return;
    if(frame.source.visible) {
        if(frame.command){g_asyncCommand=frame.source;g_commandBusy=false;++g_commandRevision;}
        const auto revision=g_commandRevision;
        bool busy=false;
        const bool read=SC::ReadProcessing(c.object,busy);
        if(!SurfaceCurrent(frame.source))return;
        if(read && revision==g_commandRevision && g_asyncCommand.term.Is(c.object) && SurfaceCurrent(g_asyncCommand)) {
            g_commandBusy=busy && !g_mirrorBusy;
            if(!g_commandBusy)g_asyncCommand={};
        }
    }
    if (frame.source.session->role()==coop::net::Role::Host) {
        if (Terminal* t = ByObject(c.object)) HostAfterBody(*t);
    } else if (frame.source.visible && SurfaceCurrent(frame.source)) {
        ClientAfterLocalBody(c.object);
    }
}

// The three watches stand or fall together: a line refused on a client whose answers no watch would send
// back would vanish.
void EnsureWatched() {
    if (g_watched || Clock::now()<g_watchRetry) return;
    g_watchRetry=Clock::now()+std::chrono::seconds(1);
    const bool enter = sg::WatchName(kRowEnterCommand, kTagEnter, OnEnterPre, OnBodyPost);
    const bool graph = sg::WatchName(kRowUbergraph, kTagGraph, OnBodyPre, OnBodyPost);
    const bool write = sg::WatchName(kRowWriteToLog, kTagWrite, OnWritePre, nullptr);
    if (enter && graph && write) {g_watched=true;return;}
    if (enter) sg::UnwatchName(kRowEnterCommand, kTagEnter, OnEnterPre, OnBodyPost);
    if (graph) sg::UnwatchName(kRowUbergraph, kTagGraph, OnBodyPre, OnBodyPost);
    if (write) sg::UnwatchName(kRowWriteToLog, kTagWrite, OnWritePre, nullptr);
    if(!g_saidWatchFailure) {
        g_saidWatchFailure=true;
        UE_LOGE("sat_console: a terminal watch did not register -- none kept; registration will retry");
    }
}

}  // namespace

void Install(coop::net::Session* s) {
    const auto world=WI::Generation();
    const auto serial=coop::net::session_serial::Current();
    if(g_session.load()!=s || world!=g_world || serial!=g_serial || (s && s->role()!=g_role)) {
        const auto epoch=g_epoch;OnDisconnect();
        if(g_epoch!=epoch+1 || g_session.load())return;
    }
    g_world=WI::Generation();g_serial=coop::net::session_serial::Current();if(s)g_role=s->role();
    g_session.store(s, std::memory_order_release);
    EnsureWatched();
}

void Tick() {
    if (!GT::IsGameThread()) return;
    PruneBodies();
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running()) return;
    if(g_world!=WI::Generation() || g_serial!=coop::net::session_serial::Current() || s->role()!=g_role)Install(s);
    const auto epoch=g_epoch;if(!Current(s,epoch))return;
    EnsureWatched();
    SC::EnsureResolved();
    // Both levels survive a late/replaced widget, and local holds belong to it.
    ApplyMirrorBusy();if(!Current(s,epoch))return;
    DrainDisplay();
    if(!Current(s,epoch))return;
    const auto now = Clock::now();
    for (int i = 0; i < kMaxPeers; ++i) {
        if(!Current(s,epoch))return;
        if(s->role()==coop::net::Role::Host && i>0) {
            if(!s->IsSlotConnected(i)){if(g_peerKnown[i])OnPeerGone(uint8_t(i));continue;}
            BindPeer(s,uint8_t(i));if(g_seedOwed[i])SeedHistory(uint8_t(i));
        }
        if (!g_out[i].q.empty() && now >= g_out[i].nextTry) Flush(static_cast<uint8_t>(i));
    }
    if(!Current(s,epoch))return;
    // A terminal holds its world's desk, dish and panel: one made in a world that is known and no longer
    // the current one goes. An unknown world keeps it.
    if (void* world = WI::CurrentWorld()) {
        for (size_t i = 0; i < g_terms.size();) {
            if (!g_terms[i].world || g_terms[i].world == world) {
                ++i;
                continue;
            }
            UE_LOGI("sat_console: HOST a terminal discarded with its world");
            DiscardAt(i);
        }
    }
    if (!g_asm.Idle() && now >= g_nextSweep) {
        g_nextSweep = now + std::chrono::seconds(1);
        g_asm.Sweep(now, std::chrono::seconds(10));
    }
}

void OnChunk(const coop::net::BlobChunkPayload& p, uint8_t senderSlot) {
    if (!GT::IsGameThread()) {
        if (!g_saidOffThread) {
            g_saidOffThread = true;
            UE_LOGE("sat_console: a chunk arrived off the game thread -- dropped");
        }
        return;
    }
    if (senderSlot >= kMaxPeers) return;
    auto* s=g_session.load();
    if(!BindPeer(s,senderSlot))return;
    std::vector<uint8_t> blob;
    if (!g_asm.OnChunk(p, senderSlot, blob)) return;
    if (SessionIf(coop::net::Role::Host)) {
        if (senderSlot != 0) HostOnBlob(senderSlot, blob);
    } else if (SessionIf(coop::net::Role::Client)) {
        if (senderSlot == 0) ClientOnBlob(blob);
    }
}

void OnPeerWorldReady(int slot) {
    auto* s=SessionIf(coop::net::Role::Host);
    if (!s || slot < 1 || slot >= kMaxPeers || !BindPeer(s,uint8_t(slot))) return;
    g_seedOwed[slot]=true;++g_seedVersion[slot];SeedHistory(uint8_t(slot));
}

void OnPeerGone(uint8_t slot) {
    if (slot >= kMaxPeers) return;
    if(slot==0 && g_role==coop::net::Role::Client) {
        g_display.clear();++g_displayRevision;
        g_mirrorBusy=false;g_haveMirrorBusy=true;++g_busyRevision;
    }
    g_asm.ClearSlot(slot);
    g_out[slot] = Outbox{};
    g_peerKnown[slot]=false;g_peerGeneration[slot]=0;g_peerGuid[slot].clear();g_seedOwed[slot]=false;
    ++g_peerVersion[slot];++g_seedVersion[slot];
    g_saidRefused[slot] = g_saidMalformed[slot] = g_saidNoTerminal[slot] = false;
    g_logRate[slot]={};
    for (Terminal& t : g_terms) {
        if (t.slot != slot) continue;
        t.slot = -1;  // its command, if any, runs to its end; the terminal waits for its typist
        UE_LOGI("sat_console: HOST slot %u left -- its terminal kept", static_cast<unsigned>(slot));
    }
}

void OnDisconnect() {
    // Detach all old ownership before the first native callback. A callback may
    // install a new session whose terminals and queues the old teardown must keep.
    std::vector<Terminal> retired;retired.swap(g_terms);
    const auto busyTerm=g_busyTerm;const bool clearBusy=g_mirrorBusy && !g_localHolds;
    const auto counts=g_counts;
    ++g_epoch;
    g_session.store(nullptr,std::memory_order_release);
    g_world=0; g_display.clear();++g_displayRevision;
    g_commandBusy=false;g_asyncCommand={};++g_commandRevision;++g_busyRevision;
    PruneBodies();
    g_history.clear();g_historyChars=0;g_haveHistory=false;++g_historyRevision;
    for(auto& rate:g_logRate)rate={};
    g_mirrorBusy = g_localHolds = g_haveMirrorBusy = false;g_busyTerm.Reset();
    for (auto& o : g_out) o = Outbox{};
    for(int i=0;i<kMaxPeers;++i){g_peerKnown[i]=false;g_peerGeneration[i]=0;g_peerGuid[i].clear();g_seedOwed[i]=false;++g_peerVersion[i];++g_seedVersion[i];}
    for (bool& b : g_saidRefused) b = false;
    for (bool& b : g_saidMalformed) b = false;
    for (bool& b : g_saidNoTerminal) b = false;
    for (bool& b : g_saidNoContext) b = false;
    g_asm.Clear();
    g_counts = Counts{};
    if(clearBusy)if(void* term=busyTerm.Get())SC::WriteProcessing(term,false);
    for(auto& t:retired){void* term=t.pin.Raw();t.pin.Release();SC::DiscardTerminal(term);}
    if (counts.linesSent || counts.linesRun)
        UE_LOGI("sat_console: session end -- %llu line(s) sent to the host, %llu run for clients",
                static_cast<unsigned long long>(counts.linesSent),
                static_cast<unsigned long long>(counts.linesRun));
}

Counts LaneCounts() {
    return g_counts;
}

size_t KeptTerminals(void** out, size_t cap) {
    size_t n = 0;
    for (const Terminal& t : g_terms)
        if (n < cap) out[n++] = t.pin.Raw();
    return n;
}

}  // namespace coop::sat_console_sync
