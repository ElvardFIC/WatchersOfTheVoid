// coop/dev/settings_drill.cpp -- see coop/dev/settings_drill.h.

#include "coop/dev/settings_drill.h"

#include "coop/commands/command_sync.h"
#include "coop/config/config.h"
#include "coop/comms/peer_action_feed.h"
#include "coop/config/config_registry.h"
#include "coop/net/session.h"
#include "coop/player/nameplate.h"
#include "coop/player/nick_color.h"
#include "coop/voice/voice_chat.h"

#include "ui/fonts.h"
#include "ui/hud.h"
#include "ui/net_stats_panel.h"
#include "ui/scale.h"

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace coop::dev::settings_drill {
namespace {

namespace CS = coop::command_sync;

constexpr int kSlot = 1;  // the pair's client

// The step the row names, parsed once at the first Tick.
enum class Token {
    Off, Nameplate, NickColor, Flags, Scale, Font, VoiceMode, VoiceVolume, Server, ServerJoin,
    ServerRed, ServerCmd, ServerCmdRed, ServerCmdSpellRed, Red
};

// 0 idle; 1 SET done, waiting (the probe posted, or for a counter token the counter to move;
// serverjoin: until the joiner's world is up; a command token: for the command's reply, which
// posts the probe); 2 RESET done, waiting likewise; 3 DONE; 4 RESPELL done, waiting for its reply.
int      g_phase = 0;
Token    g_token = Token::Off;
// The steps this process has completed, counted in Done() and never reset: a rejoin re-arms the
// drill through OnDisconnect while the host's session lives on, so the second step is told apart
// by its number. A fresh process starts at 1.
int      g_cycle = 0;
// A counter token's counter, read immediately before the SET and again before the RESET.
uint32_t g_baseline = 0;

Token ParseToken(const std::string& mode) {
    if (mode == "nameplate") return Token::Nameplate;
    if (mode == "nickcolor") return Token::NickColor;
    if (mode == "flags")     return Token::Flags;
    if (mode == "scale")     return Token::Scale;
    if (mode == "font")      return Token::Font;
    if (mode == "voicemode") return Token::VoiceMode;
    if (mode == "voicevolume") return Token::VoiceVolume;
    if (mode == "server")    return Token::Server;
    if (mode == "serverjoin") return Token::ServerJoin;
    if (mode == "serverred") return Token::ServerRed;
    if (mode == "servercmd") return Token::ServerCmd;
    if (mode == "servercmdred") return Token::ServerCmdRed;
    if (mode == "servercmdspellred") return Token::ServerCmdSpellRed;
    if (mode == "red")       return Token::Red;
    return Token::Off;
}

// The three server tokens set the session's voice range through the setters, which every later
// read on the host answers at once, and which the client's log shows arriving.
bool IsServerToken() {
    return g_token == Token::Server || g_token == Token::ServerJoin || g_token == Token::ServerRed;
}

// The three command tokens run the server step through /set and /reset, submitted as command lines;
// their probes judge the voice range like the server step's, and each reads the command's reply.
bool IsCommandToken() {
    return g_token == Token::ServerCmd || g_token == Token::ServerCmdRed ||
           g_token == Token::ServerCmdSpellRed;
}

constexpr const char* kSetLine = "set voice.distance_cm 6000";
constexpr const char* kResetLine = "reset voice.distance_cm";
// A local row named: the command refuses it, so the reply is not the set's and the step fails.
constexpr const char* kRedLine = "set net.nick x";
constexpr const char* kSetReply = "voice.distance_cm is now 6000.";
constexpr const char* kResetReply = "voice.distance_cm is back to 4800.";
// The respell between the set and the reset: the same value in another spelling, which the
// command's reply echoes as typed and which announces nothing. The red arm respells to a value
// that really changes, so the client announces it. Source's string path would call back on this
// respell (reference/source-sdk-2013/src/tier1/convar.cpp:845); ours compares the resolved value.
constexpr const char* kRespellLine = "set voice.distance_cm 6000.0";
constexpr const char* kRespellReply = "voice.distance_cm is now 6000.0.";
constexpr const char* kRespellRedLine = "set voice.distance_cm 6001";
constexpr const char* kRespellRedReply = "voice.distance_cm is now 6001.";

// The render thread applies a scale or a font row at a later drawn frame, and the voice tick
// reopens the devices for a mode row at a later game tick, so these three tokens wait on the
// module's own counter instead of a posted probe.
bool IsCounterToken() {
    return g_token == Token::Scale || g_token == Token::Font || g_token == Token::VoiceMode;
}

uint32_t Counter() {
    if (g_token == Token::Scale) return ui::scale::RowApplies();
    if (g_token == Token::Font) return ui::fonts::RowsApplies();
    return coop::voice_chat::Reopens();
}

// The counter's name in the step's lines: a scale or font row is applied, a mode row reopens.
const char* CounterName() { return g_token == Token::VoiceMode ? "reopens" : "applies"; }

// The role's row: the first font role is ui.font.menu.
const coop::config_registry::EnumRow& FontRow() { return coop::config_registry::FontRoleRow(0); }

const char* CounterKey() {
    if (g_token == Token::Scale) return ::coop::config_registry::rows::ui_scale.row->key;
    if (g_token == Token::Font) return FontRow().row->key;
    return ::coop::config_registry::rows::voice_mode.row->key;
}

const char* SetResultName(coop::config::SetResult r) {
    switch (r) {
    case coop::config::SetResult::Saved:        return "Saved";
    case coop::config::SetResult::HeldNotSaved: return "HeldNotSaved";
    case coop::config::SetResult::Refused:      return "Refused";
    }
    return "Refused";
}

void Done() {
    g_phase = 3;
    if (IsCommandToken()) CS::SetReplyObserver(nullptr);
    UE_LOGI("[SETTINGS-DRILL] host DONE (cycle %d)", ++g_cycle);
}

// Game thread, after the subscriber of the reset (FIFO).
void ProbeAfterReset() {
    if (g_token == Token::Flags) {
        const bool peerActions = coop::peer_action_feed::Enabled();
        const bool netstats    = ui::net_stats_panel::Enabled();
        if (peerActions && !netstats)
            UE_LOGI("[SETTINGS-DRILL] host: flags followed back (peer_actions=1 netstats=0)");
        else
            UE_LOGW("[SETTINGS-DRILL] FAIL: flags did not follow back (peer_actions=%d netstats=%d)",
                    peerActions ? 1 : 0, netstats ? 1 : 0);
    } else if (g_token == Token::NickColor) {
        const uint32_t packed = coop::nick_color::LocalPacked();
        if (packed == coop::nick_color::Pack(255, 255, 255))
            UE_LOGI("[SETTINGS-DRILL] host: nick_color followed back (packed=%08X)", packed);
        else
            UE_LOGW("[SETTINGS-DRILL] FAIL: nick_color did not follow back (packed=%08X)", packed);
    } else if (g_token == Token::VoiceVolume) {
        const float def = ::coop::config_registry::rows::voice_volume.row->defF;
        const float vol = coop::voice_chat::MasterVolume();
        if (std::fabs(vol - def) < 0.001f)
            UE_LOGI("[SETTINGS-DRILL] host: voice.volume followed back (volume=%.2f)", vol);
        else
            UE_LOGW("[SETTINGS-DRILL] FAIL: voice.volume did not follow back (volume=%.2f)", vol);
    } else if (IsServerToken() || IsCommandToken()) {
        const float range = coop::config::ResolveFloat(::coop::config_registry::rows::voice_distance_cm);
        if (range == 4800.f)
            UE_LOGI("[SETTINGS-DRILL] host: voice.distance_cm followed back (host resolves %.0f)", range);
        else
            UE_LOGW("[SETTINGS-DRILL] FAIL: voice.distance_cm did not follow back (host resolves %.0f)", range);
    } else {
        const bool v = coop::nameplate::LocalVisible();
        if (v) UE_LOGI("[SETTINGS-DRILL] host: nameplate followed back (visible=1)");
        else   UE_LOGW("[SETTINGS-DRILL] FAIL: nameplate did not follow back (visible=0)");
    }
    Done();
}

// A counter token's RESET: no probe is posted; Tick waits for the counter to move.
void ResetCounterRow() {
    const char* key = CounterKey();
    g_baseline = Counter();
    UE_LOGI("[SETTINGS-DRILL] host: ResetValue %s", key);
    coop::config::SetResult r;
    if (g_token == Token::Scale)
        r = coop::config::ResetValue(::coop::config_registry::rows::ui_scale);
    else if (g_token == Token::Font)
        r = coop::config::ResetValue(FontRow());
    else
        r = coop::config::ResetValue(::coop::config_registry::rows::voice_mode);
    UE_LOGI("[SETTINGS-DRILL] host: ResetValue %s returned %s", key, SetResultName(r));
    g_phase = 2;
}

void Reset() {
    if (IsCounterToken()) {
        ResetCounterRow();
        return;
    }
    // A command token's reset is a command line: its probe is posted when the reply is read.
    if (IsCommandToken()) {
        UE_LOGI("[SETTINGS-DRILL] host: servercmd submits '%s'", kResetLine);
        g_phase = 2;
        CS::Submit(kResetLine);
        return;
    }
    if (g_token == Token::Flags) {
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue ui.chat.peer_actions");
        const coop::config::SetResult r1 =
            coop::config::ResetValue(::coop::config_registry::rows::ui_chat_peer_actions);
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue ui.chat.peer_actions returned %s",
                SetResultName(r1));
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue ui.netstats");
        const coop::config::SetResult r2 =
            coop::config::ResetValue(::coop::config_registry::rows::ui_netstats);
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue ui.netstats returned %s", SetResultName(r2));
    } else if (g_token == Token::NickColor) {
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue nick_color");
        const coop::config::SetResult r =
            coop::config::ResetValue(::coop::config_registry::rows::nick_color);
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue nick_color returned %s", SetResultName(r));
    } else if (g_token == Token::VoiceVolume) {
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue voice.volume");
        const coop::config::SetResult r =
            coop::config::ResetValue(::coop::config_registry::rows::voice_volume);
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue voice.volume returned %s", SetResultName(r));
    } else if (IsServerToken()) {
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue voice.distance_cm");
        const coop::config::SetResult r =
            coop::config::ResetValue(::coop::config_registry::rows::voice_distance_cm);
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue voice.distance_cm returned %s", SetResultName(r));
    } else {
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue nameplate");
        const coop::config::SetResult r =
            coop::config::ResetValue(::coop::config_registry::rows::nameplate);
        UE_LOGI("[SETTINGS-DRILL] host: ResetValue nameplate returned %s", SetResultName(r));
    }
    ue_wrap::game_thread::Post(&ProbeAfterReset);
    g_phase = 2;
}

// A command token's respell: its reply is read in phase 4, and the reset follows it.
void Respell() {
    g_phase = 4;
    CS::Submit(g_token == Token::ServerCmdSpellRed ? kRespellRedLine : kRespellLine);
}

// Game thread, after the subscriber of the set (FIFO). A failed probe ends the step.
void ProbeAfterSet() {
    bool followed = false;
    if (g_token == Token::Flags) {
        const bool peerActions = coop::peer_action_feed::Enabled();
        const bool netstats    = ui::net_stats_panel::Enabled();
        followed = !peerActions && netstats;
        if (followed) UE_LOGI("[SETTINGS-DRILL] host: flags followed (peer_actions=0 netstats=1)");
        else UE_LOGW("[SETTINGS-DRILL] FAIL: flags did not follow (peer_actions=%d netstats=%d)",
                     peerActions ? 1 : 0, netstats ? 1 : 0);
    } else if (g_token == Token::NickColor) {
        const uint32_t packed = coop::nick_color::LocalPacked();
        followed = (packed == 0);
        if (followed) UE_LOGI("[SETTINGS-DRILL] host: nick_color followed (packed=%08X)", packed);
        else UE_LOGW("[SETTINGS-DRILL] FAIL: nick_color did not follow (packed=%08X)", packed);
    } else if (g_token == Token::VoiceVolume) {
        const float vol = coop::voice_chat::MasterVolume();
        followed = std::fabs(vol - 0.5f) < 0.001f;
        if (followed) UE_LOGI("[SETTINGS-DRILL] host: voice.volume followed (volume=%.2f)", vol);
        else UE_LOGW("[SETTINGS-DRILL] FAIL: voice.volume did not follow (volume=%.2f)", vol);
    } else if (IsServerToken() || IsCommandToken()) {
        const float range = coop::config::ResolveFloat(::coop::config_registry::rows::voice_distance_cm);
        followed = (range == 6000.f);
        if (followed)
            UE_LOGI("[SETTINGS-DRILL] host: voice.distance_cm followed (host resolves %.0f)", range);
        else
            UE_LOGW("[SETTINGS-DRILL] FAIL: voice.distance_cm did not follow (host resolves %.0f)", range);
    } else {
        const bool v = coop::nameplate::LocalVisible();
        followed = !v;
        if (followed) UE_LOGI("[SETTINGS-DRILL] host: nameplate followed (visible=0)");
        else UE_LOGW("[SETTINGS-DRILL] FAIL: nameplate did not follow (visible=1)");
    }
    // The serverjoin step waits for the joiner's world before it resets: Tick calls Reset then.
    if (followed && g_token == Token::ServerJoin) return;
    if (followed && (g_token == Token::ServerCmd || g_token == Token::ServerCmdSpellRed)) {
        Respell();
        return;
    }
    if (followed) Reset();
    else Done();
}

// The first token of the font row's list that is not the family it holds now.
std::string OtherFontToken() {
    const std::string cur = coop::config::ResolveEnum(FontRow());
    const std::string list = FontRow().row->tokens;
    size_t pos = 0;
    while (pos <= list.size()) {
        size_t bar = list.find('|', pos);
        if (bar == std::string::npos) bar = list.size();
        std::string tok = list.substr(pos, bar - pos);
        if (tok != cur) return tok;
        pos = bar + 1;
    }
    return std::string();
}

// A counter token's SET. The baseline is read before the set; the drawn-frame line (the scale and
// font tokens) says whether the HUD keeps the overlay drawing, because the render thread applies
// only on a drawn frame.
void SetCounterRow() {
    const char* key = CounterKey();
    std::string value;
    if (g_token == Token::Scale) value = "1.50";
    else if (g_token == Token::Font) value = OtherFontToken();
    else value = "activation";
    g_baseline = Counter();
    UE_LOGI("[SETTINGS-DRILL] host: SetValue %s=%s", key, value.c_str());
    coop::config::SetResult r;
    if (g_token == Token::Scale)
        r = coop::config::SetValue(::coop::config_registry::rows::ui_scale, value.c_str());
    else if (g_token == Token::Font)
        r = coop::config::SetValue(FontRow(), value.c_str());
    else
        r = coop::config::SetValue(::coop::config_registry::rows::voice_mode, value.c_str());
    UE_LOGI("[SETTINGS-DRILL] host: SetValue %s=%s returned %s", key, value.c_str(),
            SetResultName(r));
    if (g_token != Token::VoiceMode)
        UE_LOGI("[SETTINGS-DRILL] host: waiting for a drawn frame (hud=%d)",
                ui::hud::IsActive() ? 1 : 0);
    g_phase = 1;
}

// Game thread, each Tick while a counter token waits.
void PollCounter() {
    const uint32_t now = Counter();
    if (now == g_baseline) return;
    if (g_phase == 1) {
        UE_LOGI("[SETTINGS-DRILL] host: %s followed (%s=%u)", CounterKey(), CounterName(), now);
        Reset();
    } else {
        UE_LOGI("[SETTINGS-DRILL] host: %s followed back (%s=%u)", CounterKey(), CounterName(),
                now);
        Done();
    }
}

// Game thread, posted by the respell's reply. The reset waits for the respell's own delta to be
// sent (FIFO behind its subscribers), so its mark cannot merge into the respell's. A disconnect
// may have ended the step meanwhile.
void AfterRespell() {
    if (g_phase != 4) return;
    Reset();
}

// Game thread, for every reply line the host's own command is answered with. The reply is
// delivered inside the dispatch, after the setter queued its subscribers, so the probe posted here
// runs after them (FIFO). A reply other than the one the step expects ends it.
void OnCommandReply(std::string_view line) {
    UE_LOGI("[SETTINGS-DRILL] host: servercmd reply '%s'", std::string(line).c_str());
    if (g_phase == 4) {
        const bool red = (g_token == Token::ServerCmdSpellRed);
        if (line != (red ? kRespellRedReply : kRespellReply)) {
            UE_LOGW("[SETTINGS-DRILL] FAIL: servercmd reply '%s'", std::string(line).c_str());
            Done();
            return;
        }
        UE_LOGI("[SETTINGS-DRILL] host: respell sent '%s'", red ? kRespellRedLine : kRespellLine);
        ue_wrap::game_thread::Post(&AfterRespell);
        return;
    }
    const bool setStep = (g_phase == 1);
    if (line != (setStep ? kSetReply : kResetReply)) {
        UE_LOGW("[SETTINGS-DRILL] FAIL: servercmd reply '%s'", std::string(line).c_str());
        Done();
        return;
    }
    ue_wrap::game_thread::Post(setStep ? &ProbeAfterSet : &ProbeAfterReset);
}

// A command token's SET: the reply observer is one slot, shared with the command drill, so the
// step refuses to start while that drill is on. The red arm submits a local row's name, which the
// command refuses.
void SetByCommand() {
    if (coop::config::ResolveEnum(::coop::config_registry::rows::command_drill) != "off") {
        UE_LOGW("[SETTINGS-DRILL] FAIL: command_drill is on; the reply observer is one slot");
        Done();
        return;
    }
    const char* line = (g_token == Token::ServerCmdRed) ? kRedLine : kSetLine;
    UE_LOGI("[SETTINGS-DRILL] host: servercmd submits '%s'", line);
    CS::SetReplyObserver(&OnCommandReply);
    g_phase = 1;
    CS::Submit(line);
}

// The red arms are the nameplate step and the server step with their one call skipped, the
// command step with a command the dispatcher refuses, and the respell with a real change.
void Set() {
    if (IsCounterToken()) {
        SetCounterRow();
        return;
    }
    if (IsCommandToken()) {
        SetByCommand();
        return;
    }
    const bool skipSet = (g_token == Token::Red || g_token == Token::ServerRed);
    if (g_token == Token::Flags) {
        UE_LOGI("[SETTINGS-DRILL] host: SetValue ui.chat.peer_actions=0");
        const coop::config::SetResult r1 =
            coop::config::SetValue(::coop::config_registry::rows::ui_chat_peer_actions, "0");
        UE_LOGI("[SETTINGS-DRILL] host: SetValue ui.chat.peer_actions=0 returned %s",
                SetResultName(r1));
        UE_LOGI("[SETTINGS-DRILL] host: SetValue ui.netstats=1");
        const coop::config::SetResult r2 =
            coop::config::SetValue(::coop::config_registry::rows::ui_netstats, "1");
        UE_LOGI("[SETTINGS-DRILL] host: SetValue ui.netstats=1 returned %s", SetResultName(r2));
    } else if (g_token == Token::NickColor) {
        UE_LOGI("[SETTINGS-DRILL] host: SetValue nick_color= (per-surface default)");
        const coop::config::SetResult r = coop::config::SetValue(
            ::coop::config_registry::rows::nick_color, coop::nick_color::IniTextFor(0).c_str());
        UE_LOGI("[SETTINGS-DRILL] host: SetValue nick_color= (per-surface default) returned %s",
                SetResultName(r));
    } else if (g_token == Token::VoiceVolume) {
        UE_LOGI("[SETTINGS-DRILL] host: SetValue voice.volume=0.50");
        const coop::config::SetResult r =
            coop::config::SetValue(::coop::config_registry::rows::voice_volume, "0.50");
        UE_LOGI("[SETTINGS-DRILL] host: SetValue voice.volume=0.50 returned %s", SetResultName(r));
    } else if (IsServerToken()) {
        UE_LOGI("[SETTINGS-DRILL] host: SetValue voice.distance_cm=6000");
        if (!skipSet) {
            const coop::config::SetResult r =
                coop::config::SetValue(::coop::config_registry::rows::voice_distance_cm, "6000");
            UE_LOGI("[SETTINGS-DRILL] host: SetValue voice.distance_cm=6000 returned %s",
                    SetResultName(r));
        }
    } else {
        UE_LOGI("[SETTINGS-DRILL] host: SetValue nameplate=0");
        if (!skipSet) {
            const coop::config::SetResult r =
                coop::config::SetValue(::coop::config_registry::rows::nameplate, "0");
            UE_LOGI("[SETTINGS-DRILL] host: SetValue nameplate=0 returned %s", SetResultName(r));
        }
    }
    ue_wrap::game_thread::Post(&ProbeAfterSet);
    g_phase = 1;
}

// The serverjoin step sets before any client connects, so the joiner's snapshot carries the value,
// and resets once the joiner's world is up: it waits on that readiness, not on a probe. A client
// already connected at the set is an order the rig cannot recover, said and ended, never a hang.
void TickServerJoin(coop::net::Session& session) {
    if (g_phase == 0) {
        for (int slot = kSlot; slot < coop::net::kMaxPeers; ++slot) {
            if (!session.IsSlotConnected(slot)) continue;
            UE_LOGW("[SETTINGS-DRILL] FAIL: a client connected before the serverjoin set");
            Done();
            return;
        }
        Set();
    } else if (g_phase == 1 && session.IsSlotWorldReady(kSlot)) {
        Reset();
    }
}

}  // namespace

void Tick(coop::net::Session* session) {
    static const bool s_parsed = [] {
        g_token = ParseToken(
            coop::config::ResolveEnum(::coop::config_registry::rows::settings_drill));
        return true;
    }();
    (void)s_parsed;
    if (g_token == Token::Off) return;
    if (!session || !session->running() || session->role() != coop::net::Role::Host) return;
    if (g_token == Token::ServerJoin) {
        TickServerJoin(*session);
        return;
    }
    if (IsCounterToken() && (g_phase == 1 || g_phase == 2)) {
        PollCounter();
        return;
    }
    if (g_phase != 0 || !session->IsSlotWorldReady(kSlot)) return;
    Set();
}

void OnDisconnect() {
    if (IsCommandToken()) CS::SetReplyObserver(nullptr);
    g_phase = 0;
}

}  // namespace coop::dev::settings_drill
