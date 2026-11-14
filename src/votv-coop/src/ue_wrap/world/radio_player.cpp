#include "ue_wrap/world/radio_player.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <type_traits>

namespace ue_wrap::radio_player {
namespace {
namespace R=reflection;
using Guard=std::function<bool()>;
constexpr int64_t kMaxTicks=864000000000ll;
CachedObjRef g_class,g_atvClass;
R::InstanceOffset g_data{L"data"},g_player{L"mediaPlayer"},g_gm{L"gamemode"},
    g_catalog{L"radioLinks"},g_mode{L"playmode"},g_pause{L"pausePlayer"},g_uiRadio{L"radio"};
struct LayoutData {
    CachedObjRef cls,type,fn;
    int stride=0,link=-1,url=-1;
    bool Valid() const {return cls.Get() && type.Get() && fn.Get() && stride>0 && stride<=256 &&
        stride>=static_cast<int>(sizeof(R::FString)) && link>=0 && link<=stride-static_cast<int>(sizeof(R::FString)) &&
        url>=0 && url<stride;}
};
LayoutData g_layout;
bool Allowed(const Guard& current) {return !current || current();}
void* Object(void* a,R::InstanceOffset& off,const Guard& current={}) {
    CachedObjRef source;source.Set(a);
    if (!source.Get() || !Allowed(current) || !source.Get()) return nullptr;
    const int n=off.Of(a);
    if (n<0 || !source.Get() || !Allowed(current) || !source.Get()) return nullptr;
    void* p=nullptr;std::memcpy(&p,static_cast<uint8_t*>(a)+n,sizeof(p));
    if (!p || !R::IsLive(p)) return nullptr;
    CachedObjRef target;target.Set(p);
    return source.Get() && target.Get() && Allowed(current) && source.Get()?target.Get():nullptr;
}
bool Layout(void* actor,LayoutData& out,const Guard& current={}) {
    CachedObjRef ref;ref.Set(actor);
    if (!ref.Get() || !Allowed(current) || !ref.Get()) return false;
    void* cls=R::ClassOf(actor);
    CachedObjRef classRef;classRef.Set(cls);
    const auto owned=[&] {return ref.Get() && classRef.Get() && Allowed(current) && ref.Get() &&
        classRef.Get() && R::ClassOf(actor)==cls;};
    if (!owned()) return false;
    if (g_layout.cls.Is(cls) && g_layout.Valid()) {out=g_layout;return true;}
    LayoutData layout;layout.cls=classRef;
    void* type=R::PropertyInnerStruct(cls,L"data");
    if (!owned() || !type) return false;
    layout.type.Set(type);
    void* fn=R::FindDispatchFunctionCached(cls,L"openLink");
    if (!owned() || !layout.type.Get() || !fn) return false;
    layout.fn.Set(fn);
    const auto live=[&] {return owned() && layout.type.Get() && layout.fn.Get();};
    // FStructProperty carries the aligned value size, unlike PropertiesSize.
    layout.stride=R::FindParamSize(fn,L"data");
    if (!live()) return false;
    layout.link=R::FindPropertyOffsetByPrefix(type,L"link_");
    if (!live()) return false;
    layout.url=R::FindPropertyOffsetByPrefix(type,L"isUrl_");
    if (!live() || !layout.Valid()) return false;
    g_layout=layout;out=layout;return true;
}
bool CopyString(const R::FString& s,std::wstring& out) {
    if (!s.Data || s.Num<2 || s.Num>32768 || s.Max<s.Num || s.Max>32768 || s.Data[s.Num-1]!=L'\0') return false;
    for (int i=0;i<s.Num-1;++i) if (!s.Data[i]) return false;
    out.assign(s.Data,s.Num-1);return true;
}
uint64_t Id(const uint8_t* row,const LayoutData& layout) {
    if (!row || !layout.Valid() || row[layout.url]) return 0;
    R::FString s{};std::memcpy(&s,row+layout.link,sizeof(s));
    std::wstring path;
    return CopyString(s,path)?TrackId(std::move(path)):0;
}
// Each reflected operation owns the target/class/function through frame creation,
// dispatch and result extraction. Output is committed only after the final guard.
template<class T> bool Query(void* p,const wchar_t* name,T& value,const Guard& current={}) {
    CachedObjRef target,clsRef,fnRef;target.Set(p);
    if (!target.Get() || !Allowed(current) || !target.Get()) return false;
    void* cls=R::ClassOf(p);clsRef.Set(cls);
    const auto owner=[&] {return target.Get() && clsRef.Get() && Allowed(current) && target.Get() &&
        clsRef.Get() && R::ClassOf(p)==cls;};
    if (!owner()) return false;
    void* fn=R::FindDispatchFunctionCached(cls,name);
    if (!owner() || !fn) return false;
    fnRef.Set(fn);
    const auto live=[&] {return owner() && fnRef.Get();};
    ParamFrame f(fn);
    if (!f.valid() || !live()) return false;
    const bool ran=Call(p,f);
    T result{};
    const bool read=ran && f.GetRaw(L"ReturnValue",&result,sizeof(result));
    if (!live()) {
        if constexpr (std::is_same_v<T,R::FString>) if (read && result.Data) R::EngineFree(result.Data);
        return false;
    }
    if (!read) return false;
    value=result;return true;
}
bool Command(void* p,const wchar_t* name,const Guard& current) {bool result=false;return Query(p,name,result,current) && result;}
uint64_t CurrentTrack(void* player,const Guard& current={}) {
    R::FString url{};
    const bool ok=Query(player,L"GetUrl",url,current);
    struct Free {void* p;~Free(){if(p)R::EngineFree(p);}} free{url.Data};
    std::wstring path;
    return ok && CopyString(url,path) && Allowed(current)?TrackId(std::move(path)):0;
}
bool Seek(void* p,int64_t ticks,const Guard& current) {
    CachedObjRef target,clsRef,fnRef;target.Set(p);
    if (!target.Get() || ticks<0 || ticks>kMaxTicks || !Allowed(current) || !target.Get()) return false;
    void* cls=R::ClassOf(p);clsRef.Set(cls);
    const auto owned=[&] {return target.Get() && clsRef.Get() && Allowed(current) && target.Get() &&
        clsRef.Get() && R::ClassOf(p)==cls;};
    if (!owned()) return false;
    void* fn=R::FindDispatchFunctionCached(cls,L"Seek");
    if (!owned() || !fn) return false;
    fnRef.Set(fn);
    const auto live=[&] {return owned() && fnRef.Get();};
    ParamFrame f(fn);
    if (!f.valid() || !live() || !f.Set<int64_t>(L"Time",ticks) || !live()) return false;
    const bool ran=Call(p,f);
    bool result=false;
    return live() && ran && f.GetRaw(L"ReturnValue",&result,sizeof(result)) && result;
}
struct Selected {
    CachedObjRef owner;
    field_io::TArrayView array{};
    int offset=-1,index=-1;
    LayoutData layout;
    std::vector<uint8_t> bytes,original;
    std::wstring path;
    bool Current() const {
        if (!owner.Get() || !layout.Valid() || offset<0 || index<0) return false;
        field_io::TArrayView now{};
        std::memcpy(&now,static_cast<uint8_t*>(owner.Get())+offset,sizeof(now));
        return now.data==array.data && now.num==array.num && now.max==array.max && index<now.num &&
            !std::memcmp(now.data+static_cast<size_t>(index)*layout.stride,original.data(),original.size());
    }
    void BindPath() {
        R::FString link{path.data(),static_cast<int32_t>(path.size()+1),static_cast<int32_t>(path.size()+1)};
        std::memcpy(bytes.data()+layout.link,&link,sizeof(link));
    }
};
bool FindTrack(void* actor,uint64_t id,const LayoutData& layout,Selected& out,const Guard& current) {
    void* gm=Object(actor,g_gm,current);CachedObjRef gmRef;gmRef.Set(gm);
    if (!gmRef.Get() || !Allowed(current) || !layout.Valid() || !gmRef.Get()) return false;
    const int off=g_catalog.Of(gm);
    if (off<0 || !gmRef.Get() || !Allowed(current) || !layout.Valid() || !gmRef.Get()) return false;
    field_io::TArrayView array{};std::memcpy(&array,static_cast<uint8_t*>(gm)+off,sizeof(array));
    if (!array.data || array.num<0 || array.num>10000 || array.max<array.num || array.max>10000) return false;
    Selected selected;selected.owner=gmRef;selected.array=array;selected.offset=off;selected.layout=layout;
    for (int i=0;i<array.num;++i) {
        const uint8_t* row=array.data+static_cast<size_t>(i)*layout.stride;
        if (Id(row,layout)!=id) continue;
        R::FString link{};std::memcpy(&link,row+layout.link,sizeof(link));
        std::wstring path;if (!CopyString(link,path)) return false;
        if (selected.index>=0 && selected.path!=path) return false;
        selected.index=i;selected.path=std::move(path);selected.original.assign(row,row+layout.stride);
    }
    if (selected.index<0 || !selected.Current() || !Allowed(current)) return false;
    selected.bytes=selected.original;out=std::move(selected);return true;
}
struct Operation {
    CachedObjRef actor,player;
    LayoutData layout;
    Guard allowed;
    int data=-1,mode=-1,pause=-1,playerOffset=-1;
    bool Current() const {
        if (!actor.Get() || !player.Get() || !layout.Valid() || !Allowed(allowed)) return false;
        if (!actor.Get() || !player.Get() || !layout.Valid() || R::ClassOf(actor.Get())!=layout.cls.Get()) return false;
        void* bound=nullptr;std::memcpy(&bound,static_cast<uint8_t*>(actor.Get())+playerOffset,sizeof(bound));
        return player.Is(bound);
    }
};
bool Prepare(void* actor,Operation& op,const Guard& current,bool writing) {
    op.actor.Set(actor);op.allowed=current;
    const auto owned=[&] {return op.actor.Get() && Allowed(current) && op.actor.Get();};
    if (!owned() || !IsRadio(actor) || !owned() || !Layout(actor,op.layout,owned)) return false;
    op.playerOffset=g_player.Of(actor);
    if (op.playerOffset<0 || !owned() || !op.layout.Valid()) return false;
    void* player=nullptr;std::memcpy(&player,static_cast<uint8_t*>(actor)+op.playerOffset,sizeof(player));
    if (!player || !R::IsLive(player)) return false;
    op.player.Set(player);
    const auto live=[&] {return op.Current();};
    op.data=g_data.Of(actor);if (!live() || op.data<0) return false;
    op.mode=g_mode.Of(actor);if (!live() || op.mode<0) return false;
    if (writing) {op.pause=g_pause.Of(actor);if (!live() || op.pause<0) return false;}
    return live();
}
} // namespace
bool IsRadioClass(void* type) {
    CachedObjRef typeRef;typeRef.Set(type);
    const auto current=[&] {return typeRef.Get();};
    if (!current()) return false;
    void* cls=g_class.Get();
    if (!cls) {cls=object_index::ClassByName(L"prop_radio_internet_C");if(cls)g_class.Set(cls);}
    CachedObjRef base=g_class;
    if (!current() || !base.Get() || !R::IsDescendantOfAny(type,&cls,1) || !current() || !base.Get()) return false;
    void* atv=g_atvClass.Get();
    if (!atv) {atv=object_index::ClassByName(L"prop_radio_atv_C");if(atv)g_atvClass.Set(atv);}
    CachedObjRef excluded=g_atvClass;
    if (!current() || !base.Get() || !excluded.Get()) return false;
    const bool owns=!R::IsDescendantOfAny(type,&atv,1);
    return owns && current() && base.Get() && excluded.Get();
}
bool IsRadio(void* a) {
    CachedObjRef actor,typeRef;actor.Set(a);
    if (!actor.Get()) return false;
    void* type=R::ClassOf(a);typeRef.Set(type);
    return IsRadioClass(type) && actor.Get() && typeRef.Get() && R::ClassOf(a)==type;
}
void* UiRadio(void* widget) {
    CachedObjRef ref;ref.Set(widget);
    void* a=Object(widget,g_uiRadio);
    CachedObjRef actor;actor.Set(a);
    return ref.Get() && actor.Get() && IsRadio(a) && ref.Get() && actor.Get()?a:nullptr;
}
bool HasLocalTrack(void* actor) {
    CachedObjRef ref;ref.Set(actor);LayoutData layout;
    const auto current=[&] {return ref.Get();};
    if (!ref.Get() || !IsRadio(actor) || !ref.Get() || !Layout(actor,layout,current)) return false;
    const int data=g_data.Of(actor);
    return data>=0 && ref.Get() && layout.Valid() && Id(static_cast<uint8_t*>(actor)+data,layout)!=0;
}
bool OwnsPlayer(void* player) {
    CachedObjRef ref;ref.Set(player);if (!ref.Get()) return false;
    void* cls=g_class.Get();
    if (!cls) {cls=object_index::ClassByName(L"prop_radio_internet_C");if(cls)g_class.Set(cls);}
    CachedObjRef base=g_class;if (!ref.Get() || !base.Get()) return false;
    struct Search {CachedObjRef player;bool found=false;} search{ref};
    object_index::ForEachInstance(cls,[](void* ctx,void* a,int32_t) {
        auto& s=*static_cast<Search*>(ctx);CachedObjRef actor;actor.Set(a);
        const auto current=[&] {return s.player.Get() && actor.Get();};
        if (s.found || !current() || !IsRadio(a) || !current() ||
            world_identity::WorldOf(a)!=world_identity::CurrentWorld()) return;
        if (Object(a,g_player,current)!=s.player.Get() || !current()) return;
        LayoutData layout;if (!Layout(a,layout,current)) return;
        const int data=g_data.Of(a);
        if (data>=0 && current() && layout.Valid() && !static_cast<uint8_t*>(a)[data+layout.url]) s.found=true;
    },&search);
    return search.found && ref.Get() && base.Get();
}
bool Read(void* actor,State& out,const Guard& current) {
    Operation op;if (!Prepare(actor,op,current,false)) return false;
    const auto live=[&] {return op.Current();};
    void* player=op.player.Get();
    bool preparing=false;
    if (!Query(player,L"IsPreparing",preparing,live) || preparing || !live()) return false;
    State s{};s.track=Id(static_cast<uint8_t*>(actor)+op.data,op.layout);s.mode=static_cast<uint8_t*>(actor)[op.mode];
    if (!s.track || s.mode>3 || CurrentTrack(player,live)!=s.track || !live()) return false;
    bool playing=false;
    if (!Query(player,L"IsPlaying",playing,live) || !Query(player,L"GetTime",s.ticks,live) || !live() || s.ticks>kMaxTicks) return false;
    s.playing=playing;if(s.ticks<0)s.ticks=0;
    out=s;return true;
}
bool Apply(void* actor,const State& s,const Guard& current) {
    if (!s.track || s.mode>3 || s.playing>1 || s.ticks<0 || s.ticks>kMaxTicks) return false;
    Operation op;if (!Prepare(actor,op,current,true)) return false;
    const auto live=[&] {return op.Current();};
    void* player=op.player.Get();bool preparing=false,playing=false;
    if (!Query(player,L"IsPreparing",preparing,live) || !live()) return false;
    const bool same=Id(static_cast<uint8_t*>(actor)+op.data,op.layout)==s.track;
    const bool reopen=!same || (!preparing && CurrentTrack(player,live)!=s.track);
    if (!live()) return false;
    if (reopen) {
        Selected selected;if (!FindTrack(actor,s.track,op.layout,selected,live)) return false;
        const auto owned=[&] {return live() && selected.Current();};
        void* fn=op.layout.fn.Get();
        if (!fn || !owned()) return false;
        ParamFrame f(fn);if (!f.valid() || !owned()) return false;
        selected.BindPath();
        if (!f.SetRaw(L"data",selected.bytes.data(),op.layout.stride) || !owned() ||
            !f.Set<bool>(L"pausePlayer",!s.playing) || !owned()) return false;
        static_cast<uint8_t*>(actor)[op.pause]=!s.playing;static_cast<uint8_t*>(actor)[op.mode]=s.mode;
        if (!owned()) return false;
        Call(actor,f);
        return false; // asynchronous open; a later current-state reconciliation observes readiness
    }
    if (preparing || !Query(player,L"IsPlaying",playing,live) || !live()) return false;
    int64_t time=0;if (!Query(player,L"GetTime",time,live) || !live() || time<0 || time>kMaxTicks) return false;
    static_cast<uint8_t*>(actor)[op.pause]=!s.playing;static_cast<uint8_t*>(actor)[op.mode]=s.mode;
    if (time-s.ticks>10000000 || s.ticks-time>10000000) if (!Seek(player,s.ticks,live) || !live()) return false;
    if (playing!=bool(s.playing)) return Command(player,s.playing?L"Play":L"Pause",live) && live();
    return live();
}
} // namespace ue_wrap::radio_player
