// ue_wrap/world/media_player.cpp -- see ue_wrap/world/media_player.h.

#include "ue_wrap/world/media_player.h"
#include "ue_wrap/world/radio_player.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/fstring_utils.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/world_instances.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ue_wrap::media_player {
namespace {

namespace R = reflection;
namespace O = object_index;

// ATV_C.radio -> ChildActor -> mediaPlayer, the same three reads the ATV wrapper uses.
// -2 means not resolved yet (the class may still be loading); -1 means the property is absent.
int32_t g_atvRadioOff = -2;
int32_t g_childActorOff = -2;
int32_t g_mediaPlayerOff = -2;
void* g_mediaPlayerClass = nullptr;

constexpr int kMaxAtvRadios = 8;

struct AtvRadioSet {
    void* player[kMaxAtvRadios]{};
    int n = 0;
    bool resolved = false;
    bool sawSlot = false;
    bool overflow = false;
};

void NoteRadio(AtvRadioSet& set, void* player) {
    if (!player || !R::IsLive(player)) return;
    for (int i = 0; i < set.n; ++i)
        if (set.player[i] == player) return;
    if (set.n >= kMaxAtvRadios) {
        set.overflow = true;
        return;
    }
    set.player[set.n++] = player;
}

void ConsiderAtv(void* ctx, void* atv, int32_t) {
    auto& set = *static_cast<AtvRadioSet*>(ctx);
    if (!atv || !R::IsLive(atv) || g_atvRadioOff < 0) return;
    void* comp = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(atv) + g_atvRadioOff);
    if (!comp || !R::IsLive(comp)) return;
    if (g_childActorOff == -2) {
        g_childActorOff = R::FindPropertyOffset(R::ClassOf(comp), L"ChildActor");
        if (g_childActorOff < 0) g_childActorOff = -1;
    }
    if (g_childActorOff < 0) return;
    void* radio = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(comp) + g_childActorOff);
    if (!radio || !R::IsLive(radio)) return;
    void* rcls = R::ClassOf(radio);
    if (g_mediaPlayerOff == -2 || g_mediaPlayerClass != rcls) {
        const int32_t off = rcls ? R::FindPropertyOffset(rcls, L"mediaPlayer") : -1;
        g_mediaPlayerOff = off >= 0 ? off : -1;
        g_mediaPlayerClass = rcls;
    }
    if (g_mediaPlayerOff < 0) return;
    set.sawSlot = true;
    void* mp = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(radio) + g_mediaPlayerOff);
    NoteRadio(set, mp);
}

// AActor::Owner, for a player whose outer is a package but whose actor owner is the scooter.
// -2 retries while Actor is loading; -1 leaves that edge out.
void* ActorOwner(void* actor) {
    static int32_t off = -2;
    if (off == -2) {
        void* cls = R::FindClass(L"Actor");
        if (!cls) return nullptr;
        off = R::FindPropertyOffset(cls, L"Owner");
        if (off < 0) {
            off = -1;
            static bool said = false;
            if (!said) {
                said = true;
                UE_LOGW("media: Actor.Owner did not resolve -- an ATV radio is excluded by its "
                        "outer and its child-actor parent");
            }
        }
    }
    if (off < 0 || !actor) return nullptr;
    void* owner = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(actor) + off);
    return (owner && R::IsLive(owner)) ? owner : nullptr;
}

// Used only when the radio's mediaPlayer slot cannot be read. A player whose outer, actor owner,
// or child-actor parent is an ATV_C is the scooter's (the radio component's outer is the ATV, and
// the radio child actor's parent is that component). A base TV is none of those.
bool ChainReachesAtv(void* player) {
    void* seen[8] = {};
    int n = 0;
    auto push = [&](void* obj) {
        if (!obj || n >= 8 || !R::IsLive(obj)) return;
        for (int i = 0; i < n; ++i)
            if (seen[i] == obj) return;
        seen[n++] = obj;
    };
    push(player);
    for (int i = 0; i < n; ++i) {
        void* obj = seen[i];
        if (atv::IsAtv(obj)) return true;
        void* outer = R::OuterOf(obj);
        if (outer && R::IsLive(outer)) {
            if (atv::IsAtv(outer)) return true;
            push(outer);
        }
        void* cls = R::ClassOf(obj);
        if (!cls || !world_instances::IsActorClass(cls)) continue;
        if (atv::IsAtv(engine::ParentActorOf(obj))) return true;
        if (atv::IsAtv(ActorOwner(obj))) return true;
    }
    return false;
}

AtvRadioSet CollectAtvRadios() {
    AtvRadioSet set;
    void* cls = O::ClassByName(L"ATV_C");
    if (!cls) return set;
    if (g_atvRadioOff == -2) {
        g_atvRadioOff = R::FindPropertyOffset(cls, L"radio");
        if (g_atvRadioOff < 0) {
            g_atvRadioOff = -1;
            static bool said = false;
            if (!said) {
                said = true;
                UE_LOGW("media: ATV_C.radio did not resolve -- a scooter radio is left out of world "
                        "media only when its outer chain is the ATV");
            }
        }
    }
    if (g_atvRadioOff < 0) return set;
    O::ForEachInstance(cls, ConsiderAtv, &set);
    set.resolved = set.sawSlot && !set.overflow && g_childActorOff >= 0 && g_mediaPlayerOff >= 0;
    return set;
}

bool SkipWorldMedia(void* player, const AtvRadioSet& radios) {
    if (radio_player::OwnsPlayer(player)) return true;
    if (radios.resolved) {
        for (int i = 0; i < radios.n; ++i)
            if (radios.player[i] == player) return true;
        return false;
    }
    return ChainReachesAtv(player);
}

void* PlayerClass() {
    static CachedObjRef cls;
    if (!cls.Get()) cls.Set(O::ClassByName(L"MediaPlayer"));
    return cls.Get();
}

void* Fn(const wchar_t* name) {
    void* cls = PlayerClass();
    return cls ? R::FindDispatchFunctionCached(cls, name) : nullptr;
}

std::vector<CachedObjRef> Players() {
    std::vector<CachedObjRef> actors;
    if (void* cls=PlayerClass()) O::ForEachInstance(cls,[](void* ctx,void* obj,int32_t) {
        CachedObjRef ref;ref.Set(obj);static_cast<std::vector<CachedObjRef>*>(ctx)->push_back(ref);
    },&actors);
    // Call engine methods only after the index's iterator is released. Native
    // callbacks can create/destroy players; each captured serial is checked later.
    return actors;
}

bool IsDefault(void* obj) {
    return R::NameStartsWith(R::NameOf(obj), L"Default__");
}

std::wstring UrlOf(void* player) {
    void* fn = Fn(L"GetUrl");
    if (!fn || !player) return {};
    ParamFrame f(fn);
    if (!f.valid()) return {};
    const bool ok=Call(player,f);
    R::FString s = f.Get<R::FString>(L"ReturnValue");
    // ParamFrame frees the raw frame, not FString return buffers. GetUrl is
    // polled continuously, so leaving this allocation pinned leaked every poll.
    struct Free { void* p; ~Free(){if(p) R::EngineFree(p);} } free{s.Data};
    if (!ok || !s.Data || s.Num <= 1 || s.Num > 512 || s.Max<s.Num) return {};
    return std::wstring(s.Data, static_cast<size_t>(s.Num - 1));
}

bool PlayingOf(void* player) {
    void* fn = Fn(L"IsPlaying");
    if (!fn || !player) return false;
    ParamFrame f(fn);
    if (!f.valid() || !Call(player, f)) return false;
    return f.Get<bool>(L"ReturnValue");
}

int64_t TimeOf(void* player) {
    void* fn = Fn(L"GetTime");
    if (!fn || !player) return 0;
    ParamFrame f(fn);
    if (!f.valid() || !Call(player, f)) return 0;
    return f.Get<int64_t>(L"ReturnValue");
}

bool OpenFile(void* player, const std::wstring& path) {
    void* fn = Fn(L"OpenFile");
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid()) return false;
    const int32_t off = f.ParamOffset(L"FilePath");
    if (off < 0 || !fstring_utils::MintFString(path, static_cast<uint8_t*>(f.data()) + off)) return false;
    const auto arg=f.Get<R::FString>(L"FilePath");
    struct Free { void* p; ~Free(){if(p) R::EngineFree(p);} } free{arg.Data};
    return Call(player, f) && f.Get<bool>(L"ReturnValue");
}

bool OpenUrl(void* player, const std::wstring& url) {
    void* fn = Fn(L"OpenUrl");
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid()) return false;
    const int32_t off = f.ParamOffset(L"Url");
    if (off < 0 || !fstring_utils::MintFString(url, static_cast<uint8_t*>(f.data()) + off)) return false;
    const auto arg=f.Get<R::FString>(L"Url");
    struct Free { void* p; ~Free(){if(p) R::EngineFree(p);} } free{arg.Data};
    return Call(player, f) && f.Get<bool>(L"ReturnValue");
}

bool SeekTo(void* player, int64_t ticks) {
    void* fn = Fn(L"Seek");
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid()) return false;
    return f.Set<int64_t>(L"Time", ticks) && Call(player, f);
}

bool Pause(void* player) {
    void* fn = Fn(L"Pause");
    if (!fn) return false;
    ParamFrame f(fn);
    return f.valid() && Call(player, f);
}

bool Play(void* player) {
    void* fn = Fn(L"Play");
    if (!fn) return false;
    ParamFrame f(fn);
    return f.valid() && Call(player, f);
}

void* PlayerWithUrl(const std::wstring& url, void** freeOne) {
    *freeOne = nullptr;
    CachedObjRef idle;
    if (!PlayerClass()) return nullptr;
    const AtvRadioSet radios = CollectAtvRadios();
    for (const auto& ref : Players()) {
        void* obj=ref.Get();
        if (!obj || !R::IsLive(obj) || IsDefault(obj) || SkipWorldMedia(obj, radios)) continue;
        const std::wstring u = UrlOf(obj);
        if (!ref.Get()) continue;
        if (u == url) return obj;
        if (!idle.Get() && u.empty() && !PlayingOf(obj) && ref.Get()) idle=ref;
    }
    *freeOne=idle.Get();
    return nullptr;
}

}  // namespace

int CopyPlaying(Playing* out, int max) {
    if (!out || max <= 0 || !PlayerClass()) return 0;
    const AtvRadioSet radios = CollectAtvRadios();
    int n = 0;
    for (const auto& ref : Players()) {
        void* obj=ref.Get();
        if (!obj || !R::IsLive(obj) || IsDefault(obj) || SkipWorldMedia(obj, radios)) continue;
        if (n >= max) break;
        Playing p;
        p.url = UrlOf(obj);
        if (p.url.empty() || !ref.Get()) continue;
        p.playing = PlayingOf(obj);
        if (!ref.Get()) continue;
        p.ticks = p.playing ? TimeOf(obj) : 0;
        if (!p.playing || !ref.Get()) continue;
        out[n++] = std::move(p);
    }
    return n;
}

bool Apply(const wchar_t* url, int64_t ticks, bool playing) {
    if (!url || !url[0] || !PlayerClass()) return false;
    const std::wstring want(url);
    void* freeOne = nullptr;
    void* player = PlayerWithUrl(want, &freeOne);
    CachedObjRef identity;identity.Set(player);
    if (!playing) {
        if (player && PlayingOf(player) && identity.Get()) Pause(player);
        return identity.Get()!=nullptr;
    }
    if (!player) {
        // An empty idle player only. Taking the first one would replace a different video.
        player = freeOne;
        identity.Set(player);
        if (!player || !OpenUrl(player, want)) {
            static bool said = false;
            if (!said) {
                said = true;
                UE_LOGW("media: no MediaPlayer accepted '%ls'", url);
            }
            return false;
        }
    }
    if (!identity.Get()) return false;
    const bool alreadyPlaying=PlayingOf(player);
    if (!identity.Get()) return false;
    if (!alreadyPlaying) Play(player);
    if (!identity.Get()) return false;
    constexpr int64_t kTick = 10000000;  // one second
    const int64_t now = TimeOf(player);
    if (!identity.Get()) return false;
    if (ticks > 0 && (now < 0 || now - ticks > kTick / 2 || ticks - now > kTick / 2))
        SeekTo(player, ticks);
    return true;
}

std::wstring Url(void* player) { return UrlOf(player); }

bool IsPlaying(void* player) { return PlayingOf(player); }

bool Drive(void* player, const wchar_t* url, bool playing) {
    if (!player || !R::IsLive(player)) return false;
    CachedObjRef identity;identity.Set(player);
    if (url && url[0]) {
        const std::wstring want(url);
        const auto actual=UrlOf(player);
        if (!identity.Get()) return false;
        if (actual != want) {
            const bool web = want.find(L"://") != std::wstring::npos;
            if (web) {
                if (!OpenUrl(player, want)) return false;
            } else if (!OpenFile(player, want)) {
                return false;
            }
        }
    }
    if (!identity.Get()) return false;
    const bool actual=PlayingOf(player);
    if (!identity.Get()) return false;
    if (playing && !actual) Play(player);
    else if (!playing && actual) Pause(player);
    return true;
}

}  // namespace ue_wrap::media_player
