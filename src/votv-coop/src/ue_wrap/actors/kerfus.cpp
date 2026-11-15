// ue_wrap/actors/kerfus.cpp -- see ue_wrap/actors/kerfus.h.

#include "ue_wrap/actors/kerfus.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"

namespace ue_wrap::kerfus {
namespace {

namespace OI = ue_wrap::object_index;
namespace R  = ue_wrap::reflection;

// A group's latch: 0 not tried, 1 resolved, -1 a field missing (said once).
struct State {
    int latch = 0;
};

struct StateFields {
    CachedObjRef cls,base;
    std::vector<CachedObjRef> owners;
    int32_t activeOff=-1,chargingOff=-1,energyOff=-1,size=0;
    uint8_t activeMask=0,chargingMask=0;
};
std::unordered_map<void*,StateFields> g_state;
struct StateView {
    CachedObjRef actor,cls,base;
    uint32_t world=world_identity::Generation();void* worldObject=world_identity::CurrentWorld();
    std::function<bool()> caller;
    explicit StateView(void* value,const std::function<bool()>& current):caller(current){actor.Set(value);}
    bool Basic()const{return world==world_identity::Generation()&&worldObject==world_identity::CurrentWorld()&&
        actor.Get()&&cls.Get()&&R::ClassOf(actor.Raw())==cls.Raw()&&base.Get();}
    bool Current()const{return Basic()&&(!caller||caller())&&Basic();}
    bool Capture(){
        if(!actor.Get()||(caller&&!caller()))return false;
        cls.Set(R::ClassOf(actor.Raw()));base.Set(OI::ClassByName(kClassName));
        if(!Current())return false;
        void* parent=base.Raw();const bool kind=R::IsDescendantOfAny(cls.Raw(),&parent,1);
        return Current()&&kind;
    }
};
bool Span(int32_t offset,int32_t width,int32_t size){return offset>=0&&size>0&&offset<=size&&width<=size-offset;}
bool ResolveState(StateView& view,StateFields& result){
    if(!view.Capture())return false;
    if(auto it=g_state.find(view.cls.Raw());it!=g_state.end()){
        const auto cached=it->second;
        bool owned=cached.cls.Is(view.cls.Raw())&&cached.base.Is(view.base.Raw());
        for(size_t i=0;owned&&i<cached.owners.size();++i){
            owned=cached.owners[i].Get()!=nullptr;
            if(owned&&i+1<cached.owners.size())owned=R::SuperStructOf(cached.owners[i].Raw())==cached.owners[i+1].Raw();
        }
        for(const auto& owner:cached.owners)owned=owner.Get()&&owned;
        if(owned&&view.Current()){result=cached;return true;}
    }
    StateFields fields;fields.cls=view.cls;fields.base=view.base;fields.size=R::StructSize(view.cls.Raw());
    if(!view.Current()||!R::FindBoolProperty(view.cls.Raw(),L"active",fields.activeOff,fields.activeMask)||!view.Current()||
        !R::FindBoolProperty(view.cls.Raw(),L"charging",fields.chargingOff,fields.chargingMask)||!view.Current())return false;
    std::wstring type;
    if(!R::PropertyOffsetAndClass(view.cls.Raw(),L"energy",fields.energyOff,type)||!view.Current()||type!=L"FloatProperty")return false;
    bool scalar=false;std::unordered_set<void*> seen;std::vector<CachedObjRef> ancestors;
    for(void* owner=view.cls.Raw();owner;){
        if(ancestors.size()>=32||!seen.insert(owner).second||!view.Current())return false;
        CachedObjRef ref;ref.Set(owner);if(!ref.Get())return false;ancestors.push_back(ref);
        const auto members=R::EnumerateStructFields(owner);
        if(!view.Current()||!ref.Get())return false;
        for(const auto& member:members)if(_wcsicmp(member.name.c_str(),L"energy")==0){
            if(member.offset!=fields.energyOff||member.size!=sizeof(float))return false;scalar=true;break;
        }
        if(scalar)break;
        owner=R::SuperStructOf(owner);
    }
    for(size_t i=0;i<ancestors.size();++i){
        if(!ancestors[i].Get())return false;
        if(i+1<ancestors.size()&&R::SuperStructOf(ancestors[i].Raw())!=ancestors[i+1].Raw())return false;
    }
    if(!scalar||!view.Current()||!fields.activeMask||!fields.chargingMask||
        !Span(fields.activeOff,1,fields.size)||!Span(fields.chargingOff,1,fields.size)||!Span(fields.energyOff,sizeof(float),fields.size)||
        (fields.activeOff==fields.chargingOff&&(fields.activeMask&fields.chargingMask))||
        (fields.activeOff>=fields.energyOff&&fields.activeOff<fields.energyOff+sizeof(float))||
        (fields.chargingOff>=fields.energyOff&&fields.chargingOff<fields.energyOff+sizeof(float)))return false;
    fields.owners=std::move(ancestors);
    if(g_state.size()>=64)g_state.clear();g_state[view.cls.Raw()]=fields;result=fields;return true;
}
bool ReadStateBit(void* k,bool& value,bool charging,const std::function<bool()>& current){
    StateView view(k,current);StateFields fields;if(!ResolveState(view,fields)||!view.Current())return false;
    uint8_t byte=0;std::memcpy(&byte,static_cast<const uint8_t*>(k)+(charging?fields.chargingOff:fields.activeOff),1);
    if(!view.Current())return false;value=(byte&(charging?fields.chargingMask:fields.activeMask))!=0;return true;
}
bool WriteStateBit(void* k,bool value,bool charging,const std::function<bool()>& current,bool* committed=nullptr){
    if(committed)*committed=false;
    StateView view(k,current);StateFields fields;if(!ResolveState(view,fields)||!view.Current())return false;
    uint8_t byte=0;auto* target=static_cast<uint8_t*>(k)+(charging?fields.chargingOff:fields.activeOff);
    const auto mask=charging?fields.chargingMask:fields.activeMask;std::memcpy(&byte,target,1);
    byte=value?uint8_t(byte|mask):uint8_t(byte&~mask);std::memcpy(target,&byte,1);
    if(committed)*committed=true;return view.Current();
}

// The navigation pawn: its `pawn`.
struct PawnField {
    State   s;
    int32_t pawnOff = -1;
};
PawnField g_pawn;
const uint8_t* At(const void* object,int32_t offset){return static_cast<const uint8_t*>(object)+offset;}

// Latches `s` on the result and names a missing field once.
bool Settle(State& s, bool ok, const char* missing, const char* group) {
    s.latch = ok ? 1 : -1;
    if (!ok)
        UE_LOGW("kerfus: the Kerfus's `%s` did not resolve -- its %s are not read or written this process", missing,
                group);
    return ok;
}


bool ResolvePawn(void* k) {
    if (g_pawn.s.latch != 0) return g_pawn.s.latch > 0;
    if (!k || !IsKerfus(k)) return false;  // not tried: a Kerfus names the layout
    g_pawn.pawnOff = R::FindPropertyOffset(R::ClassOf(k), L"pawn");
    return Settle(g_pawn.s, g_pawn.pawnOff >= 0, "pawn", "navigation pawn");
}

}  // namespace

bool IsKerfus(void* obj) {
    return IsKerfusWhile(obj,{});
}
bool IsKerfusWhile(void* obj,const std::function<bool()>& current){StateView view(obj,current);return view.Capture();}

void* FindLive(bool (*accept)(void* obj, const void* arg), const void* arg) {
    // The Kerfus and its colour variants, each an exact class to the object index.
    static constexpr const wchar_t* kClasses[] = {
        L"p_kerfus_C", L"p_kerfus_p_C", L"p_kerfus_r_C", L"p_kerfus_y_C", L"p_kerfus_col_C", L"p_kerfus_col_gamer_C",
    };
    for (const wchar_t* name : kClasses) {
        void* cls = OI::ClassByName(name);
        if (!cls) continue;
        struct Ctx { bool (*accept)(void*, const void*); const void* arg; void* found; } ctx{accept, arg, nullptr};
        OI::ForEachInstance(cls, [](void* p, void* obj, int32_t index) {
            auto* c = static_cast<Ctx*>(p);
            if (c->found || !obj) return;
            if (R::SlotFlags(index) & (R::slot_flags::Dying | R::slot_flags::NotYetReadable)) return;
            if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
            if (!c->accept || c->accept(obj, c->arg)) c->found = obj;
        }, &ctx);
        if (ctx.found) return ctx.found;
    }
    return nullptr;
}

bool ReadActiveWhile(void* k,bool& on,const std::function<bool()>& current){return ReadStateBit(k,on,false,current);}
bool ReadChargingWhile(void* k,bool& on,const std::function<bool()>& current){return ReadStateBit(k,on,true,current);}
bool WriteActiveWhile(void* k,bool on,const std::function<bool()>& current,bool* committed){return WriteStateBit(k,on,false,current,committed);}
bool WriteChargingWhile(void* k,bool on,const std::function<bool()>& current){return WriteStateBit(k,on,true,current);}
bool ReadEnergyWhile(void* k,float& energy,const std::function<bool()>& current){
    StateView view(k,current);StateFields fields;if(!ResolveState(view,fields)||!view.Current())return false;
    float value=0;std::memcpy(&value,static_cast<const uint8_t*>(k)+fields.energyOff,sizeof value);
    if(!std::isfinite(value)||!view.Current())return false;energy=value;return true;
}
bool WriteEnergyWhile(void* k,float energy,const std::function<bool()>& current){
    if(!std::isfinite(energy))return false;
    StateView view(k,current);StateFields fields;if(!ResolveState(view,fields)||!view.Current())return false;
    std::memcpy(static_cast<uint8_t*>(k)+fields.energyOff,&energy,sizeof energy);return view.Current();
}
bool ReadActive(void* k,bool& on){return ReadActiveWhile(k,on,{});}
bool WriteActive(void* k,bool on){return WriteActiveWhile(k,on,{},nullptr);}
bool ReadEnergy(void* k,float& energy){return ReadEnergyWhile(k,energy,{});}
bool WriteEnergy(void* k,float energy){return WriteEnergyWhile(k,energy,{});}

void* NavPawn(void* k) {
    if (!ResolvePawn(k)) return nullptr;
    void* pawn = *reinterpret_cast<void* const*>(At(k, g_pawn.pawnOff));
    return (pawn && R::IsLive(pawn)) ? pawn : nullptr;
}

bool RunUpdWhile(void* k,bool skipFace,const std::function<bool()>& current,bool* dispatched){
    if(dispatched)*dispatched=false;
    StateView view(k,current);if(!view.Capture())return false;
    CachedObjRef function;function.Set(R::FindDispatchFunctionCached(view.cls.Raw(),L"upd"));
    const auto owned=[&]{
        if(!view.Current()||!function.Get())return false;
        void* resolved=R::FindDispatchFunctionCached(view.cls.Raw(),L"upd");
        return view.Current()&&function.Get()&&resolved==function.Raw();
    };
    if(!owned())return false;
    const int32_t size=R::FunctionFrameSize(function.Raw());int32_t offset=-1;uint8_t mask=0;
    if(!owned()||size<=0||size>4096||!R::FindBoolProperty(function.Raw(),L"skipFace",offset,mask)||
        !owned()||!mask||!Span(offset,1,size)||R::FindParamSize(function.Raw(),L"skipFace")!=1||!owned())return false;
    alignas(16) std::array<uint8_t,4096> bytes{};bytes[static_cast<size_t>(offset)]=skipFace?mask:0;
    if(!owned())return false;
    if(dispatched)*dispatched=true;
    const bool result=R::CallFunction(k,function.Raw(),bytes.data());
    return owned()&&result;
}
bool RunUpd(void* k,bool skipFace){return RunUpdWhile(k,skipFace,{},nullptr);}

bool RunActionOptionIndex(void* k, void* player, uint8_t action) {
    return RunActionOptionIndexWhile(k,player,action,{},nullptr);
}
bool RunActionOptionIndexWhile(void* k,void* player,uint8_t action,const std::function<bool()>& current,bool* dispatched){
    if(dispatched)*dispatched=false;
    StateView view(k,current);if(!view.Capture())return false;
    CachedObjRef who;if(player){who.Set(player);if(!who.Get()||!view.Current())return false;}
    CachedObjRef function;function.Set(R::FindDispatchFunctionCached(view.cls.Raw(),L"actionOptionIndex"));
    const auto owned=[&]{
        if(!view.Current()||(player&&!who.Get())||!function.Get())return false;
        void* resolved=R::FindDispatchFunctionCached(view.cls.Raw(),L"actionOptionIndex");
        return view.Current()&&(!player||who.Get())&&function.Get()&&resolved==function.Raw();
    };
    if(!owned())return false;
    const int32_t size=R::FunctionFrameSize(function.Raw());if(!owned()||size<=0||size>4096)return false;
    int32_t playerOff=-1,actionOff=-1;std::wstring playerType,actionType;
    if(!R::PropertyOffsetAndClass(function.Raw(),L"player",playerOff,playerType)||!owned()||playerType!=L"ObjectProperty"||
        !R::PropertyOffsetAndClass(function.Raw(),L"action",actionOff,actionType)||!owned()||
        (actionType!=L"ByteProperty"&&actionType!=L"EnumProperty"))return false;
    const auto playerSize=R::FindParamSize(function.Raw(),L"player");if(!owned()||playerSize!=sizeof(void*))return false;
    const auto actionSize=R::FindParamSize(function.Raw(),L"action");
    if(!owned()||actionSize!=1||!Span(playerOff,sizeof(void*),size)||!Span(actionOff,1,size)||
        (actionOff>=playerOff&&actionOff<playerOff+sizeof(void*)))return false;
    alignas(16) std::array<uint8_t,4096> bytes{};
    std::memcpy(bytes.data()+playerOff,&player,sizeof player);bytes[static_cast<size_t>(actionOff)]=action;
    if(!owned())return false;
    if(dispatched)*dispatched=true;
    const bool result=R::CallFunction(k,function.Raw(),bytes.data());
    return owned()&&result;
}

bool RunPossess(void* k, const FVector& at) {
    void* fn = k ? R::FindDispatchFunctionCached(R::ClassOf(k), L"possess") : nullptr;
    if (!fn) return false;
    ue_wrap::ParamFrame f(fn);
    return f.valid() && f.Set<FVector>(L"possessLoc", at) && ue_wrap::Call(k, f);
}



bool ReadCharging(void* k,bool& on){return ReadChargingWhile(k,on,{});}

bool WriteCharging(void* k,bool on){return WriteChargingWhile(k,on,{});}

}  // namespace ue_wrap::kerfus
