// ue_wrap/actors/kerfur.cpp -- see ue_wrap/actors/kerfur.h for the contract.

#include "ue_wrap/actors/kerfur.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/kismet_timer.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/devices/floppy_slot.h"       // MeshForSlotType -- the disc the reports slot shows
#include "ue_wrap/engine/engine_component.h"  // SetStaticMesh / ClearStaticMesh / SetComponentVisible

#include <cstring>
#include <cwchar>

namespace ue_wrap::kerfur {
namespace {

namespace R = ue_wrap::reflection;
namespace P = ue_wrap::profile;

// kerfurOmega_C resolves once (loads with the level; re-resolve while still null).
void* ResolveKerfurClass() {
    static void* sCls = nullptr;
    if (!sCls) sCls = R::FindClass(P::name::NpcClass_KerfurOmega);
    return sCls;
}

// Cached field offsets (resolved ONCE vs kerfurOmega_C; the offsets are inherited by every
// skin subclass, so resolving on the base is correct + stable). Sentinel -2 = "not attempted";
// FindPropertyOffset returns -1 when absent (>= 0 is a valid offset). The hot reads
// (ReadKerfurState per tick) never re-walk once these settle.
int32_t g_offState     = -2;  // TEnumAsByte<enum_kerfurCommand> "State"
int32_t g_offSpooky    = -2;  // bool "isSpooky"
int32_t g_offFace      = -2;  // int32 "faceMaterialIndex"
int32_t g_offKill      = -2;  // bool "kill" (murderfur-mode guard)
struct BoolField { int32_t offset = -2; uint8_t mask = 0; };
BoolField g_animation, g_remoteControl, g_serverValid;

void ResolveKerfurOffsets() {
    void* kc = ResolveKerfurClass();
    if (!kc) return;
    if (g_offState  == -2) g_offState  = R::FindPropertyOffset(kc, L"State");
    if (g_offSpooky == -2) g_offSpooky = R::FindPropertyOffset(kc, L"isSpooky");
    if (g_offFace   == -2) g_offFace   = R::FindPropertyOffset(kc, L"faceMaterialIndex");
    if (g_offKill   == -2) g_offKill   = R::FindPropertyOffset(kc, L"kill");
}

}  // namespace

bool IsKerfurActor(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    void* kc = ResolveKerfurClass();
    if (!kc) return false;
    void* cls = R::ClassOf(actor);
    if (!cls) return false;
    if (cls == kc) return true;
    void* bases[1] = {kc};
    return R::IsDescendantOfAny(cls, bases, 1);
}

bool TryHasSaveKey(void* actor,bool& present,const std::function<bool()>& callerCurrent) {
    CachedObjRef object,cls;object.Set(actor);
    if(!object.Get())return false;
    cls.Set(R::ClassOf(actor));
    const auto world=world_identity::Generation();
    const auto current=[&]{return (!callerCurrent||callerCurrent()) && object.Get() && cls.Get() &&
        world==world_identity::Generation() && R::ClassOf(actor)==cls.Raw();};
    if(!current())return false;
    static CachedObjRef memo;
    static int32_t memoOffset=-1;
    static bool memoFound=false;
    static std::wstring memoType;
    int32_t offset=-1;std::wstring type;bool found=false;
    if(!memo.Is(cls.Raw())){
        found=R::PropertyOffsetAndClass(cls.Raw(),L"Key",offset,type);
        if(!current())return false;
        memo=cls;
        memoOffset=offset;memoFound=found;memoType=type;
    }else{offset=memoOffset;found=memoFound;type=memoType;}
    if(!found){if(!current())return false;present=false;return true;}
    const int32_t size=R::StructSize(cls.Raw());
    if(type!=L"NameProperty" || offset<0 || offset>size || sizeof(R::FName)>static_cast<size_t>(size-offset) || !current())return false;
    R::FName key{};std::memcpy(&key,static_cast<const uint8_t*>(actor)+offset,sizeof key);
    if(!key.ComparisonIndex&&!key.Number){present=false;return true;}
    const auto text=R::ToString(key);
    if(!current()||text.empty())return false;
    present=text!=L"None";return true;
}

bool HasSaveKey(void* actor) {
    bool present=false;
    return TryHasSaveKey(actor,present)&&present;
}

bool ReadKerfurState(void* actor, uint8_t& state, bool& spooky, uint8_t& face) {
    if (!IsKerfurActor(actor)) return false;
    ResolveKerfurOffsets();
    if (g_offState < 0) return false;
    state  = *reinterpret_cast<const uint8_t*>(reinterpret_cast<const uint8_t*>(actor) + g_offState);
    spooky = (g_offSpooky >= 0) &&
             *reinterpret_cast<const bool*>(reinterpret_cast<const uint8_t*>(actor) + g_offSpooky);
    face   = (g_offFace >= 0)
                 ? static_cast<uint8_t>(*reinterpret_cast<const int32_t*>(
                       reinterpret_cast<const uint8_t*>(actor) + g_offFace))
                 : uint8_t{0};
    return true;
}

void DriveKerfurState(void* actor, uint8_t state, bool spooky) {
    if (!IsKerfurActor(actor)) return;
    ResolveKerfurOffsets();
    if (g_offState >= 0)
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(actor) + g_offState) = state;
    if (g_offSpooky >= 0)
        *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(actor) + g_offSpooky) = spooky;
}

void NeutralizeAiTimers(void* actor) {
    if (!IsKerfurActor(actor)) return;
    if (!kismet_timer::Resolved()) {
        static bool sWarned = false;
        if (!sWarned) { sWarned = true;
            UE_LOGW("kerfur: K2_ClearTimer unresolved -- mirror AI timers NOT neutralized "
                    "(timer_face/timer_kerf/checkDoor may run local AI on the parked mirror)"); }
        return;
    }
    // K2_ClearTimer(Object, FunctionName) -- the exact inverse of the BP's K2_SetTimerDelegate
    // ({self, name}) arm (kerfur BeginPlay @8184/@8714/@24444).
    static const wchar_t* kTimers[3] = {L"timer_face", L"timer_kerf", L"checkDoor"};
    for (const wchar_t* fn : kTimers) kismet_timer::Clear(actor, fn);
}

bool ReadKill(void* actor) {
    if (!IsKerfurActor(actor)) return false;
    ResolveKerfurOffsets();
    if (g_offKill < 0) return false;
    return *reinterpret_cast<const bool*>(reinterpret_cast<const uint8_t*>(actor) + g_offKill);
}

bool RunActionName(void* actor,void* player,const wchar_t* name){return RunActionNameWhile(actor,player,name,{},nullptr);}
bool RunActionNameWhile(void* actor,void* player,const wchar_t* name,const std::function<bool()>& current,bool* dispatched){
    if(dispatched)*dispatched=false;if(!actor||!name||(current&&!current()))return false;
    size_t count=0;while(count<64&&name[count])++count;if(count==64)return false;
    const std::wstring ownedName(name,count);++count;
    const auto world=world_identity::Generation();void* object=world_identity::CurrentWorld();
    CachedObjRef who,cls,base,fn,body;who.Set(actor);if(!who.Get())return false;
    cls.Set(R::ClassOf(actor));base.Set(ue_wrap::object_index::ClassByName(P::name::NpcClass_KerfurOmega));
    if(player)body.Set(player);
    const auto fresh=[&]{return world==world_identity::Generation()&&object==world_identity::CurrentWorld()&&
        who.Get()&&cls.Get()&&base.Get()&&(!player||body.Get())&&(!current||current())&&who.Get()&&cls.Get()&&R::ClassOf(actor)==cls.Raw();};
    if(!fresh())return false;void* parent=base.Raw();if(!R::IsDescendantOfAny(cls.Raw(),&parent,1)||!fresh())return false;
    fn.Set(R::FindDispatchFunctionCached(cls.Raw(),L"actionName"));
    const auto owned=[&]{if(!fresh()||!fn.Get())return false;void* selected=R::FindDispatchFunctionCached(cls.Raw(),L"actionName");return fresh()&&fn.Get()&&selected==fn.Raw();};
    if(!owned())return false;const auto size=R::FunctionFrameSize(fn.Raw());if(!owned()||size<=0||size>4096)return false;
    int32_t playerOff=-1,nameOff=-1;std::wstring playerType,nameType;
    if(!R::PropertyOffsetAndClass(fn.Raw(),L"Player",playerOff,playerType)||!owned()||playerType!=L"ObjectProperty"||
        !R::PropertyOffsetAndClass(fn.Raw(),L"Name",nameOff,nameType)||!owned()||nameType!=L"StrProperty")return false;
    const auto psize=R::FindParamSize(fn.Raw(),L"Player");if(!owned()||psize!=sizeof(void*))return false;
    const auto nsize=R::FindParamSize(fn.Raw(),L"Name");
    const auto span=[&](int32_t offset,int32_t width){return offset>=0&&offset<=size&&width<=size-offset;};
    if(!owned()||nsize!=16||!span(playerOff,sizeof(void*))||!span(nameOff,16)||
        (playerOff<nameOff+16&&nameOff<playerOff+sizeof(void*)))return false;
    struct Text{const wchar_t* data;int32_t num,max;};Text text{ownedName.c_str(),int32_t(count),int32_t(count)};
    alignas(16) uint8_t bytes[4096]{};std::memcpy(bytes+playerOff,&player,sizeof player);std::memcpy(bytes+nameOff,&text,sizeof text);
    if(!owned())return false;if(dispatched)*dispatched=true;
    const bool ran=R::CallFunction(actor,fn.Raw(),bytes);return owned()&&ran;
}

bool IsInsertPlayerReadWhile(void* callerFunction,const uint8_t* callerLocals,const uint8_t* out,const std::function<bool()>& current){
    if(!callerFunction||!callerLocals||!out||(current&&!current()))return false;
    struct Layout{CachedObjRef cls,fn;int32_t offset=-1,size=0;};static Layout cached;
    const auto world=world_identity::Generation();void* object=world_identity::CurrentWorld();
    CachedObjRef caller;caller.Set(callerFunction);if(!caller.Get())return false;
    const auto fresh=[&]{return caller.Get()&&world==world_identity::Generation()&&object==world_identity::CurrentWorld()&&(!current||current());};
    void* cls=ue_wrap::object_index::ClassByName(P::name::NpcClass_KerfurOmega);if(!fresh()||!cls)return false;
    Layout layout=cached;
    if(!layout.cls.Is(cls)||!layout.fn.Is(callerFunction)){
        layout={};layout.cls.Set(cls);if(!fresh()||!layout.cls.Get())return false;
        layout.fn.Set(R::FindFunction(cls,L"ExecuteUbergraph_kerfurOmega"));
        if(!fresh()||!layout.cls.Get()||!layout.fn.Is(callerFunction)||R::OuterOf(callerFunction)!=cls)return false;
        layout.size=R::FunctionFrameSize(callerFunction);if(!fresh()||layout.size<=0||layout.size>1024*1024)return false;
        std::wstring type;
        if(!R::PropertyOffsetAndClass(callerFunction,L"CallFunc_getMainPlayer_AsMain_Player_1",layout.offset,type)||
            !fresh()||type!=L"ObjectProperty"||layout.offset<0||layout.offset>layout.size||sizeof(void*)>layout.size-layout.offset)return false;
        const auto fields=R::EnumerateStructFields(callerFunction);if(!fresh())return false;
        bool scalar=false;for(const auto& field:fields)if(_wcsicmp(field.name.c_str(),L"CallFunc_getMainPlayer_AsMain_Player_1")==0){
            scalar=field.offset==layout.offset&&field.size==sizeof(void*);break;
        }
        if(!scalar)return false;
    }
    if(!fresh()||!layout.cls.Get()||!layout.fn.Is(callerFunction)||R::OuterOf(callerFunction)!=layout.cls.Raw())return false;
    void* resolved=R::FindFunction(layout.cls.Raw(),L"ExecuteUbergraph_kerfurOmega");
    if(!fresh()||!layout.cls.Get()||!layout.fn.Get()||resolved!=callerFunction)return false;
    cached=layout;
    return out==callerLocals+layout.offset;
}

bool IsInsertPlayerRead(void* callerFunction,const uint8_t* callerLocals,const uint8_t* out){
    return IsInsertPlayerReadWhile(callerFunction,callerLocals,out,{});
}

bool ReadHasFloppy(void* actor, bool& has) {
    if (!IsKerfurActor(actor)) return false;
    static int32_t sOff = -2;
    static uint8_t sMask = 0;
    if (sOff == -2 && !R::FindBoolProperty(ResolveKerfurClass(), L"hasFloppy", sOff, sMask)) sOff = -1;
    if (sOff < 0) return false;
    has = (static_cast<const uint8_t*>(actor)[sOff] & sMask) != 0;
    return true;
}

namespace {
bool ReadKerfurBool(void* actor, const wchar_t* name, BoolField& field, bool& value) {
    if (!IsKerfurActor(actor)) return false;
    if (field.offset == -2 && !R::FindBoolProperty(ResolveKerfurClass(), name, field.offset, field.mask))
        field.offset = -1;
    if (field.offset < 0) return false;
    value = (static_cast<const uint8_t*>(actor)[field.offset] & field.mask) != 0;
    return true;
}
}  // namespace

bool ReadAnimation(void* actor, bool& active) {
    return ReadKerfurBool(actor, L"animation", g_animation, active);
}

bool ReadRemoteControl(void* actor, bool& active) {
    return ReadKerfurBool(actor, L"remoteControl", g_remoteControl, active);
}

bool ReadServerValid(void* actor, bool& active) {
    if (!IsKerfurActor(actor)) return false;
    static int32_t offset = -2;
    if (offset == -2) offset = R::FindPropertyOffset(ResolveKerfurClass(), L"server");
    if (offset < 0) return false;
    void* server = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(actor) + offset);
    active = server && R::IsLive(server);
    return true;
}

// hasFloppy and floppyType, resolved once. -1 offset means the field is absent on this class.
struct FloppyFields { int32_t hasOff = -2; uint8_t hasMask = 0; int32_t typeOff = -2; int32_t meshOff = -2; };
static FloppyFields g_floppy;

static void ResolveFloppyFields() {
    void* kc = ResolveKerfurClass();
    if (!kc) return;
    if (g_floppy.hasOff == -2 && !R::FindBoolProperty(kc, L"hasFloppy", g_floppy.hasOff, g_floppy.hasMask))
        g_floppy.hasOff = -1;
    if (g_floppy.typeOff == -2) g_floppy.typeOff = R::FindPropertyOffset(kc, L"floppyType");
    if (g_floppy.meshOff == -2) g_floppy.meshOff = R::FindPropertyOffset(kc, L"floppyMesh");
}

bool ReadKerfurFloppy(void* actor, bool& has, uint8_t& type) {
    if (!IsKerfurActor(actor)) return false;
    ResolveFloppyFields();
    if (g_floppy.hasOff < 0 || g_floppy.typeOff < 0) return false;
    const auto* base = static_cast<const uint8_t*>(actor);
    has = (base[g_floppy.hasOff] & g_floppy.hasMask) != 0;
    const int32_t raw = *reinterpret_cast<const int32_t*>(base + g_floppy.typeOff);
    // lib_C::floppyFromType indexes seven meshes and nothing else. A type outside that, or a flag
    // with no type, is a robot that is not showing a disc.
    if (!has || raw < 0 || raw >= ue_wrap::floppy_slot::kSlotTypeCount) {
        has = false;
        type = 0;
        return true;
    }
    type = static_cast<uint8_t>(raw);
    return true;
}

bool DriveKerfurFloppy(void* actor, bool has, uint8_t type) {
    if (!IsKerfurActor(actor)) return true;
    ResolveFloppyFields();
    // Absent fields are a fact about this class, not a delay. Stop asking.
    if (g_floppy.hasOff < 0 || g_floppy.typeOff < 0 || g_floppy.meshOff < 0) return true;
    void* comp = *reinterpret_cast<void**>(static_cast<uint8_t*>(actor) + g_floppy.meshOff);
    if (!comp || !R::IsLive(comp)) return false;
    const int32_t written = (has && type < ue_wrap::floppy_slot::kSlotTypeCount)
                                ? static_cast<int32_t>(type) : -1;
    if (written >= 0) {
        void* mesh = nullptr;
        if (!ue_wrap::floppy_slot::MeshForSlotType(actor, written, mesh)) return false;
        // A Static component ignores SetStaticMesh. The disc rides the skeleton, so it is Movable;
        // saying so again is what makes a mirror that spawned Static actually show the mesh.
        // A successful ask that names no mesh is an empty answer, not a delay: retrying it would
        // call the library every frame.
        if (mesh) {
            ue_wrap::engine::SetComponentMobility(comp, 2);
            if (!ue_wrap::engine::SetStaticMesh(comp, mesh)) return false;
            ue_wrap::engine::SetComponentVisible(comp, true, false);
        } else {
            ue_wrap::engine::ClearStaticMesh(comp);
            ue_wrap::engine::SetComponentVisible(comp, false, false);
        }
    } else {
        ue_wrap::engine::ClearStaticMesh(comp);
        ue_wrap::engine::SetComponentVisible(comp, false, false);
    }
    auto* base = static_cast<uint8_t*>(actor);
    if (written >= 0) base[g_floppy.hasOff] = static_cast<uint8_t>(base[g_floppy.hasOff] | g_floppy.hasMask);
    else              base[g_floppy.hasOff] = static_cast<uint8_t>(base[g_floppy.hasOff] & ~g_floppy.hasMask);
    *reinterpret_cast<int32_t*>(base + g_floppy.typeOff) = written;
    return true;
}

void* ReadHoldObject(void* actor) {
    if (!IsKerfurActor(actor)) return nullptr;
    static int32_t sOff = -2;
    if (sOff == -2) sOff = R::FindPropertyOffset(ResolveKerfurClass(), L"holdObject");
    if (sOff < 0) return nullptr;
    void* t = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(actor) + sOff);
    return (t && R::IsLive(t)) ? t : nullptr;
}

void* ReadTargetActor(void* actor) {
    if (!IsKerfurActor(actor)) return nullptr;
    static int32_t sOff = -2;
    if (sOff == -2) sOff = R::FindPropertyOffset(ResolveKerfurClass(), L"targetActor");
    if (sOff < 0) return nullptr;
    void* t = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(actor) + sOff);
    return (t && R::IsLive(t)) ? t : nullptr;
}





}  // namespace ue_wrap::kerfur
