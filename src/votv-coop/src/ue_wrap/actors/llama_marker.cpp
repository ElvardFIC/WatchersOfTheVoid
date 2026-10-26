#include "ue_wrap/actors/llama_marker.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/keyed_objects.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
namespace ue_wrap::llama_marker {
namespace {
namespace R=reflection;namespace S=save_record;
constexpr wchar_t kClass[]=L"llamaSoul_C";
bool Ancestry(void* cls,const wchar_t* name){for(int i=0;cls&&i<64;++i,cls=R::SuperStructOf(cls)){
    if(!R::IsLive(cls))return false;if(R::NameEquals(R::NameOf(cls),name))return true;}return false;}
struct Live {
    CachedObjRef actor,cls;void* world=world_identity::CurrentWorld();uint32_t generation=world_identity::Generation();
    explicit Live(void* p=nullptr){if(p&&R::IsLive(p)){actor.Set(p);cls.Set(R::ClassOf(p));}}
    bool Current()const{return world&&world==world_identity::CurrentWorld()&&generation==world_identity::Generation()&&actor.Get()&&cls.Get()&&
        R::ClassOf(actor.Get())==cls.Get()&&world_identity::WorldOf(actor.Get())==world&&R::NameEquals(R::NameOf(cls.Get()),kClass)&&Ancestry(cls.Get(),L"actor_save_C");}
};
std::optional<Live> g_cleanup;bool g_cleaning=false;uint64_t g_cleanupRevision=0;
bool Same(const Live& a,const Live& b){return a.actor.Is(b.actor.Get())&&a.cls.Is(b.cls.Get())&&a.world==b.world&&a.generation==b.generation;}
void Cleanup(){if(g_cleaning||!g_cleanup)return;const auto owned=*g_cleanup;
    if(!owned.Current()){g_cleanup.reset();return;}const auto revision=g_cleanupRevision;
    struct Guard{Guard(){g_cleaning=true;}~Guard(){g_cleaning=false;}}guard;engine::DestroyActor(owned.actor.Get());
    if(g_cleanup&&revision==g_cleanupRevision&&!owned.actor.Get())g_cleanup.reset();}
void Dispose(const Live& owned){if(!owned.Current())return;g_cleanup=owned;++g_cleanupRevision;Cleanup();}
bool KeyOffset(const Live& source,int& offset){std::wstring type;int32_t found=-1;
    if(!source.Current()||!R::PropertyOffsetAndClass(source.cls.Get(),L"key",found,type)||type!=L"NameProperty")return false;
    const int size=R::StructSize(source.cls.Get());if(found<0||size<8||found>size-8||!source.Current())return false;offset=found;return true;}
bool ReadKey(const Live& source,std::wstring& output){int offset=-1;if(!KeyOffset(source,offset))return false;
    auto value=S::ReadFNameAt(source.actor.Get(),offset);if(!source.Current())return false;int actual=-1;if(!KeyOffset(source,actual)||actual!=offset)return false;output=std::move(value);return true;}
bool Vacant(const Live& source,const std::wstring& key){if(!source.Current()||!ValidKey(key)||!keyed_objects::Available()||!source.Current())return false;
    void* found=keyed_objects::Resolve(key.c_str());return source.Current()&&(!found||found==source.actor.Get());}
struct Function {
    Live source;CachedObjRef fn,owner;const wchar_t* name;const wchar_t* declaring;
    Function(const Live& s,const wchar_t* n,const wchar_t* d=nullptr):source(s),name(n),declaring(d){if(source.Current()){
        fn.Set(R::FindDispatchFunctionCached(source.cls.Get(),name));if(fn.Get())owner.Set(R::OuterOf(fn.Get()));}}
    bool Current()const{return source.Current()&&fn.Get()&&owner.Get()&&R::NameEquals(R::NameOf(fn.Get()),name)&&
        (!declaring||R::NameEquals(R::NameOf(owner.Get()),declaring))&&R::OuterOf(fn.Get())==owner.Get()&&
        R::FindDispatchFunctionCached(source.cls.Get(),name)==fn.Get()&&source.Current()&&fn.Get()&&owner.Get();}
};
bool Span(const ParamFrame& frame,const wchar_t* name,int bytes){if(!frame.valid()||bytes<=0)return false;const int offset=frame.ParamOffset(name),size=frame.FrameSize();
    if(offset<0||size<bytes||offset>size-bytes)return false;const auto fields=R::FunctionParams(frame.function());
    return std::count_if(fields.begin(),fields.end(),[&](const auto& p){return !_wcsicmp(p.name.c_str(),name)&&p.offset==offset&&p.size==bytes;})==1;}
bool RecordSpan(const Function& f,const ParamFrame& frame,bool load){
    if(!f.Current()||!frame.valid())return false;int bytes=-1;for(const auto& p:R::FunctionParams(f.fn.Get()))if(!_wcsicmp(p.name.c_str(),L"data"))bytes=p.size;
    if(bytes<S::kSaveRecordBytes||bytes>S::kSaveStride||!Span(frame,L"data",bytes))return false;
    CachedObjRef type;type.Set(R::PropertyInnerStruct(f.fn.Get(),L"data"));
    if(!type.Get()||!R::NameEquals(R::NameOf(type.Get()),L"struct_save")||R::StructSize(type.Get())<S::kSaveRecordBytes||R::StructSize(type.Get())>S::kSaveStride)return false;
    if(load){if(!Span(frame,L"return",1))return false;const int a=frame.ParamOffset(L"data"),b=frame.ParamOffset(L"return");if(b>=a&&b<a+bytes)return false;}
    return f.Current()&&type.Get();
}
bool RegisterKey(const Live& source,const Function& get){
    std::wstring prior;if(!ReadKey(source,prior)||(!prior.empty()&&prior!=L"None"&&(!ValidKey(prior)||!Vacant(source,prior))))return false;
    if(!get.Current())return false;ParamFrame frame(get.fn.Get());
    if(!Span(frame,L"key",8)||!get.Current()||!source.Current()||(!prior.empty()&&prior!=L"None"&&!Vacant(source,prior))||!get.Current()||!Call(source.actor.Get(),frame)||!source.Current()||!get.Current())return false;
    std::wstring actual;if(!ReadKey(source,actual)||!ValidKey(actual)||(!prior.empty()&&prior!=L"None"&&actual!=prior)||!keyed_objects::Available()||!source.Current())return false;
    void* registered=keyed_objects::Resolve(actual.c_str());return source.Current()&&get.Current()&&registered==source.actor.Get();
}
bool ReadTransform(const Live& source,FTransform& out){const Function get(source,L"GetTransform");if(!get.Current())return false;ParamFrame frame(get.fn.Get());FTransform value;
    if(!Span(frame,L"ReturnValue",48)||!get.Current()||!source.Current()||!Call(source.actor.Get(),frame)||!get.Current()||!source.Current()||!frame.GetRaw(L"ReturnValue",&value,48))return false;
    const float values[]={value.RotX,value.RotY,value.RotZ,value.RotW,value.TX,value.TY,value.TZ,value.SX,value.SY,value.SZ};
    for(float v:values)if(!std::isfinite(v))return false;out=value;return true;
}
FTransform Transform(const S::SaveRecord& r){FTransform t;t.RotX=r.xform[0];t.RotY=r.xform[1];t.RotZ=r.xform[2];t.RotW=r.xform[3];
    t.TX=r.xform[4];t.TY=r.xform[5];t.TZ=r.xform[6];t.SX=r.xform[7];t.SY=r.xform[8];t.SZ=r.xform[9];return t;}
bool Equal(const FTransform& a,const FTransform& b){const float av[]={a.TX,a.TY,a.TZ,a.SX,a.SY,a.SZ},bv[]={b.TX,b.TY,b.TZ,b.SX,b.SY,b.SZ};
    for(int i=0;i<6;++i)if(std::fabs(av[i]-bv[i])>1e-4f)return false;
    const float q[]={a.RotX,a.RotY,a.RotZ,a.RotW},r[]={b.RotX,b.RotY,b.RotZ,b.RotW};bool same=true,opposite=true;
    for(int i=0;i<4;++i){same&=std::fabs(q[i]-r[i])<=1e-4f;opposite&=std::fabs(q[i]+r[i])<=1e-4f;}return same||opposite;}
bool ApplyTransform(const Live& source,const S::SaveRecord& record){FTransform before,wanted=Transform(record);if(!ReadTransform(source,before))return false;
    // Rootless native markers are normally identity transforms. No setter is needed
    // when GetTransform already confirms the exact saved value.
    if(Equal(before,wanted))return true;const Function set(source,L"K2_SetActorTransform");if(!set.Current())return false;ParamFrame frame(set.fn.Get());
    int hit=-1;for(const auto& p:R::FunctionParams(set.fn.Get()))if(!_wcsicmp(p.name.c_str(),L"SweepHitResult"))hit=p.size;
    if(hit<1||hit>1024||!Span(frame,L"NewTransform",48)||!Span(frame,L"bSweep",1)||!Span(frame,L"bTeleport",1)||!Span(frame,L"ReturnValue",1)||!Span(frame,L"SweepHitResult",hit))return false;
    const wchar_t* names[]={L"NewTransform",L"bSweep",L"SweepHitResult",L"bTeleport",L"ReturnValue"};const int sizes[]={48,1,hit,1,1};
    for(int i=0;i<5;++i)for(int j=0;j<i;++j){const int a=frame.ParamOffset(names[i]),b=frame.ParamOffset(names[j]);if(a<b+sizes[j]&&b<a+sizes[i])return false;}
    if(!frame.Set(L"NewTransform",wanted)||!frame.Set<bool>(L"bSweep",false)||!frame.Set<bool>(L"bTeleport",true)||!set.Current()||!source.Current())return false;
    const bool called=Call(source.actor.Get(),frame);uint8_t accepted=0;
    if(!source.Current()||!set.Current()||!frame.GetRaw(L"ReturnValue",&accepted,1)||accepted>1)return false;
    FTransform after;return ReadTransform(source,after)&&source.Current()&&Equal(after,wanted)&&(called||accepted);
}
}
bool Is(void* actor){return Live(actor).Current();}
bool Ready(void* actor){const Live source(actor);int offset=-1;return source.Current()&&KeyOffset(source,offset)&&keyed_objects::Available()&&source.Current();}
bool ValidKey(const std::wstring& key){if(key.empty()||key==L"None"||key.size()>128)return false;for(wchar_t c:key)if(c<32||c>126)return false;return true;}
bool Normalize(const S::SaveRecord& in,S::SaveRecord& out){
    if(in.className!=kClass||!ValidKey(in.key)||!in.bools.empty()||!in.floats.empty()||!in.ints.empty()||!in.strings.empty()||!in.signals.empty()||
        !in.classes.empty()||!in.vectors.empty()||!in.rotators.empty()||!in.transforms.empty()||!in.bytes.empty()||!in.names.empty())return false;
    for(float v:in.xform)if(!std::isfinite(v))return false;double q=0;for(int i=0;i<4;++i)q+=double(in.xform[i])*in.xform[i];if(q<.9||q>1.1)return false;
    for(int i=4;i<7;++i)if(std::fabs(in.xform[i])>2'000'000.f)return false;for(int i=7;i<10;++i)if(in.xform[i]<.01f||in.xform[i]>100.f)return false;
    out=in;return true;
}
std::wstring Key(void* actor){std::wstring key;return ReadKey(Live(actor),key)?key:std::wstring{};}
bool SetKey(void* actor,const std::wstring& key){const Live source(actor);std::wstring prior;
    if(!ValidKey(key)||!ReadKey(source,prior)||(!prior.empty()&&prior!=L"None"&&prior!=key)||!Vacant(source,key))return false;
    const Function get(source,L"getKey",L"actor_save_C");if(!get.Current())return false;int offset=-1;if(!KeyOffset(source,offset)||!Vacant(source,key)||!source.Current())return false;
    S::WriteFNameField(static_cast<uint8_t*>(source.actor.Get())+offset,key);
    std::wstring actual;return source.Current()&&ReadKey(source,actual)&&actual==key&&RegisterKey(source,get)&&ReadKey(source,actual)&&actual==key;
}
bool Capture(void* actor,S::SaveRecord& out){const Live source(actor);if(!source.Current()||(g_cleanup&&Same(*g_cleanup,source)))return false;
    const Function get(source,L"getKey",L"actor_save_C"),data(source,L"getData",L"actor_save_C");if(!get.Current()||!data.Current())return false;ParamFrame frame(data.fn.Get());
    if(!RecordSpan(data,frame,false)||!RegisterKey(source,get)||!data.Current()||!source.Current()||!Call(source.actor.Get(),frame)||!data.Current()||!source.Current())return false;
    S::SaveRecord raw,checked;S::ReadSaveRecord(static_cast<const uint8_t*>(frame.data())+frame.ParamOffset(L"data"),raw);
    std::wstring key;if(!source.Current()||!data.Current()||!Normalize(raw,checked)||!ReadKey(source,key)||key!=checked.key||!Vacant(source,key))return false;
    if(keyed_objects::Resolve(key.c_str())!=source.actor.Get()||!source.Current())return false;out=std::move(checked);return true;
}
bool Apply(void* actor,const S::SaveRecord& incoming){S::SaveRecord record;if(!Normalize(incoming,record))return false;const Live source(actor);std::wstring prior;
    if(g_cleaning&&g_cleanup&&Same(*g_cleanup,source))return false;
    if(!ReadKey(source,prior)||(!prior.empty()&&prior!=L"None"&&prior!=record.key)||!Vacant(source,record.key))return false;
    const Function load(source,L"loadData",L"actor_save_C"),get(source,L"getKey",L"actor_save_C");if(!load.Current()||!get.Current())return false;ParamFrame frame(load.fn.Get());
    if(!RecordSpan(load,frame,true)||!ApplyTransform(source,record)||!source.Current()||!load.Current()||!Vacant(source,record.key))return false;
    const int offset=frame.ParamOffset(L"data");std::memset(static_cast<uint8_t*>(frame.data())+offset,0,S::kSaveRecordBytes);
    if(!S::WriteSaveRecordChecked(static_cast<uint8_t*>(frame.data())+offset,record,&object_index::ClassByName)||!source.Current()||!load.Current()||!Vacant(source,record.key)||!load.Current()||!Call(source.actor.Get(),frame)||!source.Current()||!load.Current())return false;
    uint8_t accepted=0;if(!frame.GetRaw(L"return",&accepted,1)||accepted!=1||!SetKey(source.actor.Get(),record.key)||!source.Current())return false;
    std::wstring key;if(!ReadKey(source,key)||key!=record.key)return false;if(g_cleanup&&Same(*g_cleanup,source))g_cleanup.reset();return true;
}
void* Spawn(const S::SaveRecord& incoming){S::SaveRecord record;if(!Normalize(incoming,record)||!keyed_objects::Available())return nullptr;
    const auto world=world_identity::CurrentWorld();const auto generation=world_identity::Generation();if(!world)return nullptr;
    void* existing=keyed_objects::Resolve(record.key.c_str());if(existing)return Is(existing)&&Apply(existing,record)?existing:nullptr;
    Cleanup();if(g_cleanup||world!=world_identity::CurrentWorld()||generation!=world_identity::Generation())return nullptr;
    CachedObjRef cls;cls.Set(object_index::ClassByName(kClass));if(!cls.Get()||!R::NameEquals(R::NameOf(cls.Get()),kClass)||!Ancestry(cls.Get(),L"actor_save_C"))return nullptr;
    const FVector position{record.xform[4],record.xform[5],record.xform[6]};const auto rotation=engine::QuatToRotator(record.xform[0],record.xform[1],record.xform[2],record.xform[3]);
    if(world!=world_identity::CurrentWorld()||generation!=world_identity::Generation()||!cls.Get())return nullptr;
    void* actor=engine::BeginDeferredSpawn(cls.Get(),position,rotation);const Live source(actor);
    if(!source.Current()||source.world!=world||source.generation!=generation||!cls.Is(source.cls.Get()))return nullptr;
    if(!engine::FinishDeferredSpawn(actor,position,rotation)||!source.Current()||!cls.Is(source.cls.Get())||!Apply(actor,record)){
        Dispose(source);return nullptr;
    }return source.Current()?source.actor.Get():nullptr;
}
void OnDisconnect(){Cleanup();}
}
