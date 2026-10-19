#include "ue_wrap/devices/service_panel.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/engine/engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace ue_wrap::service_panel {
namespace {
namespace R=reflection;
struct Array { uint8_t* data; int32_t num, max; };
int Offset(void* a,const wchar_t* name) {
    // All callers use static field-name literals. InstanceOffset is slot/serial
    // aware; never walk the very large desk property list on every frame.
    static std::unordered_map<std::wstring,R::InstanceOffset> offsets;
    return offsets.try_emplace(name,name).first->second.Of(a);
}
bool ArrayOf(void* a,const wchar_t* name,Array& out,int maxCount) {
    if (!a) return false;
    const int off=Offset(a,name);
    if (off<0) return false;
    std::memcpy(&out,static_cast<uint8_t*>(a)+off,sizeof(out));
    return out.data && out.num>0 && out.num<=maxCount && out.max>=out.num;
}
template<class T> bool Field(void* a,const wchar_t* name,T& v,bool write=false) {
    if (!a) return false;
    const int off=Offset(a,name);
    if (off<0) return false;
    auto* p=static_cast<uint8_t*>(a)+off;
    if (write) std::memcpy(p,&v,sizeof(v)); else std::memcpy(&v,p,sizeof(v));
    return true;
}
bool Bit(void* a,const wchar_t* name,bool& v,bool write=false) {
    int32_t off=-1; uint8_t mask=0;
    if (!a || !R::FindBoolProperty(R::ClassOf(a),name,off,mask) || off<0 || !mask) return false;
    auto& b=static_cast<uint8_t*>(a)[off];
    if (write) b=static_cast<uint8_t>(v ? b|mask : b&~mask); else v=(b&mask)!=0;
    return true;
}
void* Object(void* a,const wchar_t* name) {
    void* obj=nullptr;
    return Field(a,name,obj) && obj && R::IsLive(obj) ? obj : nullptr;
}
void* Part(void* a,uint8_t op,uint8_t index) {
    const wchar_t* array=Family(a)==1 ? L"panels" : (op==1 ? L"puzzleButtons" : L"fusesButtons");
    if (Family(a)==2 && (op==2 || op==3)) return Object(a,op==2 ? L"leverReset" : L"leverConfirm");
    Array arr{};
    if (!ArrayOf(a,array,arr,32) || index>=arr.num) return nullptr;
    void* c=nullptr; std::memcpy(&c,arr.data+index*sizeof(void*),sizeof(c));
    return c && R::IsLive(c) ? c : nullptr;
}
bool Call(void* a,const wchar_t* name) { return component_calls::CallParamlessNamed(a,name); }
bool SetBroken(void* a,bool broken) {
    void* fn=R::FindDispatchFunctionCached(R::ClassOf(a),L"setBroken");
    if (!fn) return false;
    ParamFrame p(fn);
    return p.valid() && p.Set<bool>(L"isBroken",broken) && p.Set<bool>(L"pullLever",false) && ue_wrap::Call(a,p);
}
}
uint8_t Family(void* a) {
    if (!a) return 0;
    const auto n=R::ClassNameOf(a);
    return n==L"analogDScreenTest_C" ? 1 : (n==L"radiotower_C" ? 2 : 0);
}
bool Read(void* a,uint8_t out[16]) {
    uint8_t v[16]{};
    if (Family(a)==1) {
        Array covers{},screws{};
        if (!ArrayOf(a,L"panelsState",covers,4) || covers.num!=4 ||
            !ArrayOf(a,L"panelsUnscrew",screws,4) || screws.num!=4) return false;
        for (int i=0;i<4;++i) {
            if (covers.data[i]) v[0]|=1u<<i;
            if (screws.data[i]) v[1]|=1u<<i;
        }
    } else if (Family(a)==2) {
        Array lights{},fuses{}; bool broken=false,animation=false;
        if (!ArrayOf(a,L"puzzleData",lights,25) || !ArrayOf(a,L"fuses",fuses,6) || fuses.num!=6 ||
            !Bit(a,L"isBroken",broken) || !Bit(a,L"animation",animation)) return false;
        v[0]=6; v[1]=static_cast<uint8_t>(lights.num); v[2]=broken; v[3]=animation;
        std::memcpy(v+4,fuses.data,6);
        for (int i=0;i<lights.num;++i) if (lights.data[i]) v[10+i/8]|=1u<<(i%8);
        FRotator rot{};
        if (void* lever=Object(a,L"leverConfirm")) Field(lever,L"RelativeRotation",rot);
        if (!std::isfinite(rot.Roll)) return false;
        v[14]=static_cast<uint8_t>(std::clamp(std::lround(rot.Roll),0l,180l));
        if (void* lever=Object(a,L"leverReset"))
            v[15]=static_cast<uint8_t>(std::clamp(std::lround(engine::GetComponentRelativeLocation(lever).X*10.f),0l,90l));
    } else return false;
    std::memcpy(out,v,16); return true;
}
bool Apply(void* a,const uint8_t v[16]) {
    uint8_t before[16]{};
    if (!Read(a,before)) return false;
    if (std::memcmp(before,v,16)==0) return true;
    if (Family(a)==1) {
        if ((v[0]|v[1])&0xf0) return false;
        Array covers{},screws{};
        if (!ArrayOf(a,L"panelsState",covers,4) || covers.num!=4 ||
            !ArrayOf(a,L"panelsUnscrew",screws,4) || screws.num!=4) return false;
        if (!R::FindDispatchFunctionCached(R::ClassOf(a),L"updPanels")) return false;
        for (int i=0;i<4;++i) { covers.data[i]=(v[0]>>i)&1; screws.data[i]=(v[1]>>i)&1; }
        if (Call(a,L"updPanels")) return true;
        for (int i=0;i<4;++i) { covers.data[i]=(before[0]>>i)&1; screws.data[i]=(before[1]>>i)&1; }
        return false;
    }
    if (v[0]!=6 || before[1]!=v[1] || v[2]>1 || v[3]>1 || v[14]>180 || v[15]>90) return false;
    Array lights{},fuses{};
    if (!ArrayOf(a,L"puzzleData",lights,25) || !ArrayOf(a,L"fuses",fuses,6) || fuses.num!=6) return false;
    for (int i=0;i<6;++i) if (v[4+i]>2) return false;
    for (int i=0;i<lights.num;++i) lights.data[i]=(v[10+i/8]>>(i%8))&1;
    std::memcpy(fuses.data,v+4,6);
    if (before[2]!=v[2]) SetBroken(a,v[2]!=0);
    bool animation=v[3]!=0; Bit(a,L"animation",animation,true);
    if (std::memcmp(before+4,v+4,6)) Call(a,L"updFuses");
    if (std::memcmp(before+10,v+10,4) || before[2]!=v[2]) Call(a,L"updPuzzle");
    for (int i=0;i<2;++i) {
        void* c=Object(a,i==0 ? L"leverConfirm" : L"leverReset");
        if (!c) continue;
        const wchar_t* verb=i==0 ? L"K2_SetRelativeRotation" : L"K2_SetRelativeLocation";
        void* fn=R::FindDispatchFunctionCached(R::ClassOf(c),verb);
        if (!fn) continue;
        ParamFrame p(fn);
        if (!p.valid()) continue;
        if (i==0) p.Set<FRotator>(L"NewRotation",FRotator{0,0,static_cast<float>(v[14])});
        else p.Set<FVector>(L"NewLocation",FVector{v[15]/10.f,0,0});
        ue_wrap::Call(c,p);
    }
    return true;
}
bool FuseLook(void* a,uint8_t& index) {
    int32_t i=-1;
    if (!Field(a,L"lookAtFuseIndex",i) || i<0 || i>=6) return false;
    index=static_cast<uint8_t>(i); return true;
}
bool RadioLook(void* a,uint8_t& op,uint8_t& index) {
    bool b=false; index=0;
    if (Bit(a,L"lookAtFuse",b) && b) { op=4; return FuseLook(a,index); }
    if (Bit(a,L"isLookAtButton",b) && b) {
        int32_t i=-1; uint8_t state[16]{};
        if (!Field(a,L"lookAtIndex",i) || !Read(a,state) || i<0 || i>=state[1]) return false;
        op=1; index=static_cast<uint8_t>(i); return true;
    }
    if (Bit(a,L"lookAtReset",b) && b) { op=2; return true; }
    if (Bit(a,L"lookAtConfirm",b) && b) { op=3; return true; }
    return false;
}
bool RadioUse(void* a,uint8_t op,uint8_t index) {
    uint8_t state[16]{};
    if (!Read(a,state) || !state[2] || state[3]) return false;
    if (op==1) {
        if (index>=state[1]) return false;
        void* fn=R::FindDispatchFunctionCached(R::ClassOf(a),L"switchNeighbors");
        if (!fn) return false;
        ParamFrame p(fn);
        return p.valid() && p.Set<int32_t>(L"index",index) && p.Set<bool>(L"update",true) && ue_wrap::Call(a,p);
    }
    if (op!=2 && op!=3) return false;
    bool anim=true; Bit(a,L"animation",anim,true);
    const bool ok=Call(Object(a,op==2 ? L"TL_reset" : L"TL_lever"),L"PlayFromStart");
    if (!ok) { anim=false; Bit(a,L"animation",anim,true); }
    return ok;
}
bool PartLocation(void* a,uint8_t op,uint8_t index,FVector& at) {
    void* c=Part(a,op,index);
    if (!c) return false;
    at=engine::GetComponentLocation(c); return true;
}
bool SetFuse(void* a,uint8_t index,uint8_t value) {
    Array f{};
    if (index>=6 || value>2 || !ArrayOf(a,L"fuses",f,6) || f.num!=6) return false;
    if (!R::FindDispatchFunctionCached(R::ClassOf(a),L"updFuses")) return false;
    const uint8_t before=f.data[index]; f.data[index]=value;
    if (Call(a,L"updFuses")) return true;
    f.data[index]=before; return false;
}
bool DeskLook(void* a,uint8_t& index) {
    int32_t i=-1;
    if (!Field(a,L"lookingAtPanel",i) || i<0 || i>=4) return false;
    index=static_cast<uint8_t>(i); return true;
}
void* SpawnLoosePart(void* a,uint8_t index,bool goodFuse,const FVector* dropAt) {
    FVector at{};
    if (!PartLocation(a,4,index,at)) return nullptr;
    // Leave room for the new rigid body instead of spawning inside its socket.
    void* part=Part(a,4,index);
    const FRotator rot=engine::GetComponentWorldRotation(part);
    void* cls=nullptr;
    const bool desk=Family(a)==1;
    if (desk) {
        static const wchar_t* names[]={L"prop_computerBackPanel_TL_C",L"prop_computerBackPanel_BL_C",
            L"prop_computerBackPanel_TR_C",L"prop_computerBackPanel_BR_C"};
        if (index>=4) return nullptr;
        cls=R::FindClass(names[index]);
    } else cls=R::FindClass(goodFuse ? L"prop_fuse_C" : L"prop_C");
    if (!cls) return nullptr;
    if (dropAt) at=*dropAt;
    at.Z+=40.f;
    bool wasTaking=false, taking=true;
    if (desk) { Bit(a,L"panelTakeOff",wasTaking); Bit(a,L"panelTakeOff",taking,true); }
    void* spawned=engine::BeginDeferredSpawn(cls,at,rot);
    if (!spawned) { if (desk) Bit(a,L"panelTakeOff",wasTaking,true); return nullptr; }
    if (!desk && !goodFuse) {
        const auto name=fname_utils::StringToFName(L"fuse_0");
        if (!name.ComparisonIndex || !prop::WriteSpParityIdentity(spawned,name,false,false,false,false)) {
            Call(spawned,L"K2_DestroyActor"); return nullptr;
        }
    }
    const bool ok=engine::FinishDeferredSpawn(spawned,at,rot);
    if (desk) Bit(a,L"panelTakeOff",wasTaking,true);
    if (!ok) Call(spawned,L"K2_DestroyActor");
    return ok ? spawned : nullptr;
}
int CoverIndex(void* a) {
    if (!a) return -1;
    const auto cls=R::ClassNameOf(a);
    const wchar_t* names[]={L"prop_computerBackPanel_TL_C",L"prop_computerBackPanel_BL_C",
        L"prop_computerBackPanel_TR_C",L"prop_computerBackPanel_BR_C"};
    for (int i=0;i<4;++i) if (cls==names[i]) return i;
    return -1;
}
bool CanReturnCover(void* desk,void* cover) {
    const int index=CoverIndex(cover);
    uint8_t state[16]{};
    bool taking=false,busy=false;
    if (index<0 || !Read(desk,state) || (state[0]&(1<<index)) ||
        !Bit(desk,L"panelTakeOff",taking) || !Bit(desk,L"panelsProcess",busy) || taking || busy) return false;
    const wchar_t* lids[]={L"lid1",L"lid2",L"lid3",L"lid4"};
    void* lid=Object(desk,lids[index]);
    if (!lid) return false;
    ParamFrame overlap(R::FindDispatchFunctionCached(R::ClassOf(lid),L"IsOverlappingActor"));
    bool touches=false;
    if (!overlap.valid() || !overlap.Set<void*>(L"Other",cover) || !ue_wrap::Call(lid,overlap) ||
        !overlap.GetRaw(L"ReturnValue",&touches,sizeof(touches)) || !touches) return false;
    return true;
}
bool TryReturnCover(void* desk,void* cover) {
    if (!CanReturnCover(desk,cover)) return false;
    const int index=CoverIndex(cover);
    if (index==2) {
        // The cooked desk has native overlap branches for TL/BL/BR only.
        uint8_t before[16]{},state[16]{};
        if (!Read(desk,before)) return false;
        std::memcpy(state,before,16);state[0]|=1<<index;
        if (!Apply(desk,state)) return false;
        if (!Call(cover,L"K2_DestroyActor")) { Apply(desk,before);return false; }
        return true;
    }
    ParamFrame use(R::FindDispatchFunctionCached(R::ClassOf(desk),L"ReceiveActorBeginOverlap"));
    if (!use.valid() || !use.Set<void*>(L"OtherActor",cover) || !ue_wrap::Call(desk,use)) return false;
    uint8_t state[16]{};
    return Read(desk,state) && (state[0]&(1<<index));
}
}
