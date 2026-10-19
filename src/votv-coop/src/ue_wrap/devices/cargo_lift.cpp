#include "ue_wrap/devices/cargo_lift.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine_physics.h"
#include "ue_wrap/engine/world_identity.h"
#include <cmath>
#include <cstring>

namespace ue_wrap::cargo_lift {
namespace {
namespace R=reflection;
struct Parts {
    CachedObjRef actor, anchor, body, timeline;
    uint32_t world=0;
};
struct Field {
    R::InstanceOffset offset;
    explicit Field(const wchar_t* name):offset(name){}
    template<class T> bool Read(void* a,T& value) {
        const int off=offset.Of(a); if(off<0)return false;
        std::memcpy(&value,static_cast<uint8_t*>(a)+off,sizeof(T)); return true;
    }
    template<class T> bool Write(void* a,const T& value) {
        const int off=offset.Of(a); if(off<0)return false;
        std::memcpy(static_cast<uint8_t*>(a)+off,&value,sizeof(T)); return true;
    }
};
struct Bit {
    const wchar_t* name; CachedObjRef cls; int off=-1; uint8_t mask=0;
    bool Resolve(void* a) {
        void* c=R::ClassOf(a);
        if(!cls.Is(c)||off<0){
            cls.Set(c);off=-1;mask=0;
            if(!R::FindBoolProperty(c,name,off,mask)){off=-1;mask=0;return false;}
        }
        return off>=0 && mask;
    }
    bool Read(void* a,bool& v){if(!Resolve(a))return false;v=(static_cast<uint8_t*>(a)[off]&mask)!=0;return true;}
    bool Write(void* a,bool v){if(!Resolve(a))return false;auto& b=static_cast<uint8_t*>(a)[off];b=uint8_t(v?b|mask:b&~mask);return true;}
};
Field g_anchor(L"attach"),g_body(L"comp"),g_move(L"move"),g_alpha(L"alpha");
Field g_relative(L"RelativeLocation");
Field g_buttons[]={Field(L"buttonsElev"),Field(L"buttonsElev1"),Field(L"buttonsElev2")};
Bit g_moving{L"moving"},g_dir{L"dir"};
void* Object(Field& f,void* a){void* p=nullptr;return f.Read(a,p)&&p&&R::IsLive(p)?p:nullptr;}
void* Function(void* a,const wchar_t* name){return R::FindDispatchFunctionCached(R::ClassOf(a),name);}
bool Current(const Parts& m) {
    void* a=m.actor.Get();
    return a && m.world==world_identity::Generation() &&
        world_identity::WorldOf(a)==world_identity::CurrentWorld() &&
        m.anchor.Get() && m.body.Get() && m.timeline.Get() &&
        Object(g_anchor,a)==m.anchor.Get() && Object(g_body,a)==m.body.Get() && Object(g_move,a)==m.timeline.Get();
}
bool Capture(void* a,Parts& m) {
    if(!IsLift(a))return false;
    m.actor.Set(a);m.anchor.Set(Object(g_anchor,a));m.body.Set(Object(g_body,a));m.timeline.Set(Object(g_move,a));
    m.world=world_identity::Generation();
    return Current(m) && engine::IsPrimitiveComponent(m.body.Get());
}
template<class T> bool Query(void* a,const wchar_t* name,T& v) {
    ParamFrame f(Function(a,name));return f.valid()&&Call(a,f)&&f.GetRaw(L"ReturnValue",&v,sizeof(v));
}
bool Position(void* a,const FVector& p,const FRotator* rot,bool snap) {
    ParamFrame f(Function(a,rot?L"K2_SetWorldLocationAndRotation":L"K2_SetRelativeLocation"));
    return f.valid()&&f.Set<FVector>(L"NewLocation",p)&&(!rot||f.Set<FRotator>(L"NewRotation",*rot))&&
        f.Set<bool>(L"bSweep",false)&&f.Set<bool>(L"bTeleport",snap)&&Call(a,f);
}
bool Finite(float v,float bound){return std::isfinite(v)&&std::fabs(v)<=bound;}
bool Vec(const FVector& v,float bound){return Finite(v.X,bound)&&Finite(v.Y,bound)&&Finite(v.Z,bound);}
}
bool IsLift(void* a) {
    return a && R::IsLive(a) && R::ClassNameOf(a)==L"cargoLift_C" &&
        !R::NameStartsWith(R::NameOf(a),L"Default__") && world_identity::CurrentWorld() &&
        world_identity::WorldOf(a)==world_identity::CurrentWorld();
}
std::wstring Key(void* a){return IsLift(a)?R::ToString(R::NameOf(a)):std::wstring{};}
void Enumerate(std::vector<CachedObjRef>& out) {
    out.clear();void* cls=object_index::ClassByName(L"cargoLift_C");if(!cls)return;
    object_index::ForEachInstance(cls,[](void* ctx,void* a,int32_t){
        auto& rows=*static_cast<std::vector<CachedObjRef>*>(ctx);
        if(IsLift(a)&&rows.size()<16){CachedObjRef r;r.Set(a);rows.push_back(r);}
    },&out);
}
int ButtonIndex(void* a,void* comp) {
    if(!IsLift(a)||!comp)return -1;
    for(int i=0;i<3;++i)if(Object(g_buttons[i],a)==comp)return i;
    return -1;
}
bool ButtonPosition(void* a,int button,FVector& out) {
    if(!IsLift(a)||button<0||button>2)return false;
    CachedObjRef actor,comp;actor.Set(a);comp.Set(Object(g_buttons[button],a));
    const auto world=world_identity::Generation();void* c=comp.Get();
    return c&&Query(c,L"K2_GetComponentLocation",out)&&world==world_identity::Generation()&&
        actor.Get()==a&&comp.Get()==c&&Object(g_buttons[button],a)==c&&Vec(out,1e7f);
}
bool Valid(const State& s) {
    return std::isfinite(s.alpha)&&s.alpha>=0&&s.alpha<=1&&Vec(s.anchor,2000)&&Vec(s.position,1e7f)&&
        Finite(s.rotation.Pitch,360)&&Finite(s.rotation.Yaw,360)&&Finite(s.rotation.Roll,360);
}
bool Read(void* a,State& out) {
    Parts m;if(!Capture(a,m))return false;State s{};
    if(!g_alpha.Read(a,s.alpha)||!g_moving.Read(a,s.moving)||!g_dir.Read(a,s.direction)||
        !g_relative.Read(m.anchor.Get(),s.anchor))return false;
    if(!Query(m.body.Get(),L"K2_GetComponentLocation",s.position)||!Current(m))return false;
    if(!Query(m.body.Get(),L"K2_GetComponentRotation",s.rotation)||!Current(m)||!Valid(s))return false;
    out=s;return true;
}
bool Activate(void* a,int button,void* player) {
    bool moving=false;
    if(!IsLift(a)||button<0||button>2||!player||!R::IsLive(player)||!g_moving.Read(a,moving)||moving)return false;
    // The real action entry checks moving, toggles dir, starts the speed curve/latent
    // obstruction loop. player_use is an EMPTY native entry and must not be replayed.
    void* comp=Object(g_buttons[button],a);if(!comp)return false;
    ParamFrame f(Function(a,L"actionOptionIndex"));
    return f.valid()&&f.Set<void*>(L"player",player)&&f.Set<uint8_t>(L"action",4)&&
        f.Set<void*>(L"lookAtComponent",comp)&&Call(a,f);
}
bool Replay(void* a,const State& state,int button,void* player,const std::function<bool()>& stillCurrent) {
    if(!Valid(state)||!stillCurrent())return false;
    Parts parts;if(!Capture(a,parts))return false;
    auto valid=[&]{return Current(parts)&&stillCurrent();};
    bool moving=false;
    // A local native trip owns its latent loop. Let it finish before aligning or
    // accepting the next host trip; never clear its latch or start a second loop.
    if(!g_moving.Read(a,moving)||moving)return false;
    if(state.moving && (!player||!R::IsLive(player)))return false;
    // Only a join/start/end alignment. Keep native simulation, constraints,
    // timelines and obstruction checks running throughout the actual trip.
    if(!Position(parts.anchor.Get(),state.anchor,nullptr,true)||!valid())return false;
    if(!Position(parts.body.Get(),state.position,&state.rotation,true)||!valid())return false;
    if(!g_alpha.Write(a,state.alpha)||!g_dir.Write(a,state.moving?!state.direction:state.direction))return false;
    if(!state.moving)return true;
    // actionOptionIndex toggles dir before running the native movement graph.
    const bool ok=Activate(a,button,player);
    return ok&&valid();
}
}
