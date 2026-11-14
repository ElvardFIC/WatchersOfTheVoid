#pragma once
#include "coop/creatures/npc_mirror_brain.h"
#include "ue_wrap/actors/rufus.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/script_gate.h"
namespace coop::npc_mirror_brain::rufus_birth {
inline constexpr wchar_t kClass[]=L"rufus_C",kGraph[]=L"ExecuteUbergraph_rufus";
inline constexpr int kTag=0x52554655;
inline ue_wrap::script_gate::Verdict Before(const ue_wrap::script_gate::Call& call){
    return ue_wrap::rufus::IsFamily(call.object)&&IsMarkedMirror(call.object)?
        ue_wrap::script_gate::Verdict::Cancel:ue_wrap::script_gate::Verdict::Run;
}
inline bool Install(){return ue_wrap::script_gate::WatchClassName(kClass,kGraph,kTag,&Before,nullptr);}
inline bool ReadyForClass(void* actorClass){
    if(!ue_wrap::rufus::IsFamilyClass(actorClass))return true;
    ue_wrap::CachedObjRef cls;cls.Set(actorClass);
    const auto generation=ue_wrap::world_identity::Generation();void* world=ue_wrap::world_identity::CurrentWorld();
    if(!world||!Install())return false;ue_wrap::script_gate::ResolvePendingNames();
    return cls.Get()&&world==ue_wrap::world_identity::CurrentWorld()&&generation==ue_wrap::world_identity::Generation()&&
        ue_wrap::script_gate::ClassNameWatchLive(kClass,kGraph,kTag);
}
inline bool Prepare(void* actor){
    if(!ue_wrap::rufus::IsFamily(actor))return false;
    namespace R=ue_wrap::reflection;namespace W=ue_wrap::world_identity;namespace SG=ue_wrap::script_gate;
    ue_wrap::CachedObjRef live,cls;live.Set(actor);cls.Set(R::ClassOf(actor));
    const auto generation=W::Generation();void* world=W::CurrentWorld();
    const auto current=[&]{return world&&world==W::CurrentWorld()&&generation==W::Generation()&&live.Get()&&cls.Get()&&R::ClassOf(actor)==cls.Get();};
    if(!current()||!ReadyForClass(cls.Get()))return false;
    if(!current()||!SG::ClassNameWatchLive(kClass,kGraph,kTag))return false;
    const bool marked=IsMarkedMirror(actor);const int32_t index=live.Idx(),serial=R::SlotSerial(index);
    MarkMirror(actor);
    if(current()&&IsMarkedMirror(actor)&&SG::ClassNameWatchLive(kClass,kGraph,kTag))return true;
    if(!marked)UnmarkMirror(index,serial);return false;
}
}
