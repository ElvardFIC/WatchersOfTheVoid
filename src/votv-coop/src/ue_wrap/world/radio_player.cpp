#include "ue_wrap/world/radio_player.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"
#include <cstring>
#include <chrono>

namespace ue_wrap::radio_player {
namespace {
namespace R=reflection;
CachedObjRef g_class,g_atvClass,g_dataClass;
int g_stride=0,g_link=-1,g_url=-1;
void Diagnostic(const char* why,uint64_t track=0,uint64_t actual=0) {
    using namespace std::chrono;
    static steady_clock::time_point next{};
    const auto now=steady_clock::now();if (now<next) return;next=now+seconds(5);
    UE_LOGW("radio: %s selected=%llu actual=%llu",why,
        static_cast<unsigned long long>(track),static_cast<unsigned long long>(actual));
}
R::InstanceOffset g_data{L"data"},g_player{L"mediaPlayer"},g_gm{L"gamemode"},
    g_catalog{L"radioLinks"},g_mode{L"playmode"},g_pause{L"pausePlayer"},g_uiRadio{L"radio"};
void* Object(void* a,R::InstanceOffset& off) {
    if (!a) return nullptr;
    const int n=off.Of(a); void* p=nullptr;
    if (n>=0) std::memcpy(&p,static_cast<uint8_t*>(a)+n,sizeof(p));
    return p && R::IsLive(p) ? p : nullptr;
}
bool Layout(void* actor) {
    void* cls=R::ClassOf(actor);
    if (!cls) return false;
    if (g_dataClass.Get()!=cls || g_stride<=0 || g_link<0 || g_url<0) {
        const bool changed=g_dataClass.Get()!=cls;
        g_dataClass.Set(cls);
        void* type=R::PropertyInnerStruct(cls,L"data");
        // UStruct::PropertiesSize excludes possible trailing alignment. The
        // FStructProperty parameter carries the actual value/array element size.
        g_stride=R::FindParamSize(R::FindDispatchFunctionCached(cls,L"openLink"),L"data");
        g_link=R::FindPropertyOffsetByPrefix(type,L"link_");
        g_url=R::FindPropertyOffsetByPrefix(type,L"isUrl_");
        if (changed || (g_stride>0 && g_link>=0 && g_url>=0))
            UE_LOGI("radio: layout class='%ls' raw=%d stride=%d link=%d url=%d",
                R::ToString(R::NameOf(cls)).c_str(),R::StructSize(type),g_stride,g_link,g_url);
    }
    return g_stride>0 && g_stride<=256 && g_link>=0 && g_link+16<=g_stride && g_url>=0 && g_url<g_stride;
}
uint64_t Id(const uint8_t* row) {
    if (!row || row[g_url]) return 0; // only files already in this peer's radio catalog
    R::FString link{};
    std::memcpy(&link,row+g_link,sizeof(link));
    if (!link.Data || link.Num<2 || link.Num>32768 || link.Max<link.Num) return 0;
    return TrackId(std::wstring(link.Data,link.Num-1));
}
template<class T> bool Query(void* p,const wchar_t* name,T& value) {
    if (!p) return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(p),name));
    return f.valid() && Call(p,f) && f.GetRaw(L"ReturnValue",&value,sizeof(value));
}
bool Command(void* p,const wchar_t* name) { bool result=false; return Query(p,name,result) && result; }
uint64_t CurrentTrack(void* player) {
    R::FString url{};
    const bool ok=Query(player,L"GetUrl",url);
    // FString return storage is engine-owned; ParamFrame only frees its raw frame.
    struct Free { void* p; ~Free() { if (p) R::EngineFree(p); } } free{url.Data};
    return ok && url.Data && url.Num>=2 && url.Num<=32768
        ? TrackId(std::wstring(url.Data,url.Num-1)) : 0;
}
bool Seek(void* p,int64_t ticks) {
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(p),L"Seek"));
    return f.valid() && f.Set<int64_t>(L"Time",ticks) && Call(p,f) && f.Get<bool>(L"ReturnValue");
}
const uint8_t* FindTrack(void* actor,uint64_t id) {
    void* gm=Object(actor,g_gm);
    if (!gm || !Layout(actor)) return nullptr;
    const int off=g_catalog.Of(gm); field_io::TArrayView a{};
    if (off<0) return nullptr;
    std::memcpy(&a,static_cast<uint8_t*>(gm)+off,sizeof(a));
    if (!a.data || a.num<0 || a.num>10000 || a.max<a.num) return nullptr;
    const uint8_t* found=nullptr;
    for (int i=0;i<a.num;++i) {
        const uint8_t* row=a.data+i*g_stride;
        if (Id(row)!=id) continue;
        if (found && field_io::ReadFStringAt(found,g_link)!=field_io::ReadFStringAt(row,g_link)) return nullptr;
        found=row;
    }
    return found;
}
}
bool IsRadio(void* a) {
    if (!a) return false;
    void* cls=g_class.Get();
    if (!cls) { cls=object_index::ClassByName(L"prop_radio_internet_C"); if (cls) g_class.Set(cls); }
    void* type=R::ClassOf(a);
    if (!cls || !type || !R::IsDescendantOfAny(type,&cls,1)) return false;
    void* atv=g_atvClass.Get();
    if (!atv) { atv=object_index::ClassByName(L"prop_radio_atv_C");if (atv) g_atvClass.Set(atv); }
    // The ATV driver/parked-host lane already owns its radio. Two authors here
    // fight that state and also churn the single-class reflected field caches.
    return !atv || !R::IsDescendantOfAny(type,&atv,1);
}
void* UiRadio(void* widget) { void* a=Object(widget,g_uiRadio); return IsRadio(a) ? a : nullptr; }
bool HasLocalTrack(void* actor) {
    if (!IsRadio(actor) || !Layout(actor)) return false;
    const int data=g_data.Of(actor);
    return data>=0 && Id(static_cast<uint8_t*>(actor)+data)!=0;
}
bool OwnsPlayer(void* player) {
    void* cls=g_class.Get();
    if (!cls) { cls=object_index::ClassByName(L"prop_radio_internet_C"); if (cls) g_class.Set(cls); }
    if (!cls || !player) return false;
    struct Search { void* player; bool found; } s{player,false};
    object_index::ForEachInstance(cls,[](void* ctx,void* a,int32_t) {
        auto& s=*static_cast<Search*>(ctx);
        if (!s.found && IsRadio(a) && world_identity::WorldOf(a)==world_identity::CurrentWorld() && Object(a,g_player)==s.player && Layout(a)) {
            const int data=g_data.Of(a);
            // Local files and idle radios belong to this lane. Keep actual internet
            // URLs on the pre-existing media lane, including their native end callback.
            if (data>=0 && !static_cast<uint8_t*>(a)[data+g_url]) s.found=true;
        }
    },&s);
    return s.found;
}
bool Read(void* actor,State& out) {
    if (!IsRadio(actor) || !Layout(actor)) return false;
    void* player=Object(actor,g_player);
    const int data=g_data.Of(actor),mode=g_mode.Of(actor);
    if (!player || data<0 || mode<0) return false;
    bool preparing=false;
    if (!Query(player,L"IsPreparing",preparing) || preparing) return false;
    State s{}; s.track=Id(static_cast<uint8_t*>(actor)+data);
    s.mode=static_cast<uint8_t*>(actor)[mode];
    if (!s.track) return false;
    const auto actual=CurrentTrack(player);
    if (actual!=s.track) { Diagnostic("waiting for selected media",s.track,actual);return false; }
    bool playing=false;
    if (!s.track || s.mode>3 || !Query(player,L"IsPlaying",playing) || !Query(player,L"GetTime",s.ticks)) return false;
    s.playing=playing; if (s.ticks<0) s.ticks=0;
    out=s; return true;
}
bool Apply(void* actor,const State& s) {
    if (!IsRadio(actor) || !s.track || s.mode>3 || s.playing>1 || s.ticks<0 || !Layout(actor)) return false;
    void* player=Object(actor,g_player);
    const int data=g_data.Of(actor),pause=g_pause.Of(actor),mode=g_mode.Of(actor);
    if (!player || data<0 || pause<0 || mode<0) return false;
    bool preparing=false,playing=false;
    if (!Query(player,L"IsPreparing",preparing)) return false;
    const bool sameData=Id(static_cast<uint8_t*>(actor)+data)==s.track;
    const bool reopen=!sameData || (!preparing && CurrentTrack(player)!=s.track);
    const uint8_t* row=reopen ? FindTrack(actor,s.track) : nullptr;
    if (reopen && !row) { Diagnostic("local catalog has no unique matching track",s.track);return false; }
    // Native OnMediaOpened resumes asynchronously; give that callback the canonical pause bit too.
    static_cast<uint8_t*>(actor)[pause]=!s.playing;
    static_cast<uint8_t*>(actor)[mode]=s.mode;
    if (reopen) {
        ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(actor),L"openLink"));
        if (!f.valid() || !f.SetRaw(L"data",row,g_stride) || !f.Set<bool>(L"pausePlayer",!s.playing) || !Call(actor,f)) return false;
        return false; // opening is asynchronous; retain the row until ready
    }
    if (preparing || !Query(player,L"IsPlaying",playing)) return false;
    bool ok=true;
    int64_t time=0;
    if (!Query(player,L"GetTime",time)) return false;
    if (time-s.ticks>10000000 || s.ticks-time>10000000) ok=Seek(player,s.ticks);
    if (playing!=bool(s.playing)) ok=Command(player,s.playing ? L"Play" : L"Pause") && ok;
    return ok;
}
}
