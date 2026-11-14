#include "ue_wrap/actors/llama_events.h"
#include "ue_wrap/actors/llama.h"
#include "ue_wrap/world/world_singleton.h"
#include <algorithm>
#include <cstring>
#include <vector>
namespace ue_wrap::llama_events {
namespace {
namespace R=reflection;
struct Array {void** data=nullptr;int32_t num=0,max=0;};static_assert(sizeof(Array)==16);
struct Snapshot {
    Owner source;CachedObjRef gm,cls;int countOffset=-1,arrayOffset=-1,hOffset=-1;uint8_t hMask=0;
    int32_t count=0;Array array;bool active=false;std::vector<void*> senders;int own=0;
};
bool Current(const Owner& owner){return owner.world&&owner.world==world_identity::CurrentWorld()&&owner.generation==world_identity::Generation()&&
    owner.actor.Get()&&owner.cls.Get()&&R::ClassOf(owner.actor.Get())==owner.cls.Get()&&world_identity::WorldOf(owner.actor.Get())==owner.world;}
bool Typed(void* cls,const wchar_t* name){for(int i=0;cls&&i<64;++i,cls=R::SuperStructOf(cls)){
    if(!R::IsLive(cls))return false;if(R::NameEquals(R::NameOf(cls),name))return true;}return false;}
bool Field(void* cls,const wchar_t* name,const wchar_t* kind,int size,int& out){
    int32_t offset=-1;std::wstring type;if(!cls||!R::PropertyOffsetAndClass(cls,name,offset,type)||type!=kind)return false;
    const int bytes=R::StructSize(cls);if(offset<0||size<=0||bytes<size||offset>bytes-size)return false;out=offset;return true;
}
bool H(const Owner& owner,int& offset,uint8_t& mask){int32_t actual=-1;uint8_t bits=0;
    if(!Current(owner)||!R::FindBoolProperty(owner.cls.Get(),L"h",actual,bits)||actual<0||!bits||actual>=R::StructSize(owner.cls.Get())||!Current(owner))return false;
    offset=actual;mask=bits;return true;
}
bool Bound(const Snapshot& s){
    if(!Current(s.source)||!s.gm.Get()||!s.cls.Get()||R::ClassOf(s.gm.Get())!=s.cls.Get()||world_identity::WorldOf(s.gm.Get())!=s.source.world||
        world_singleton::Gamemode()!=s.gm.Get()||!Typed(s.cls.Get(),L"mainGamemode_C"))return false;
    int count=-1,array=-1,h=-1;uint8_t mask=0;
    return Field(s.cls.Get(),L"activeEvents",L"IntProperty",4,count)&&count==s.countOffset&&
        Field(s.cls.Get(),L"activeEvents_senders",L"ArrayProperty",16,array)&&array==s.arrayOffset&&H(s.source,h,mask)&&h==s.hOffset&&mask==s.hMask&&
        Current(s.source)&&s.gm.Get()&&s.cls.Get()&&R::ClassOf(s.gm.Get())==s.cls.Get();
}
bool Stable(const Snapshot& s){
    if(!Bound(s))return false;int32_t count=0;Array array;
    std::memcpy(&count,static_cast<uint8_t*>(s.gm.Get())+s.countOffset,4);std::memcpy(&array,static_cast<uint8_t*>(s.gm.Get())+s.arrayOffset,16);
    if(count!=s.count||array.data!=s.array.data||array.num!=s.array.num||array.max!=s.array.max)return false;
    const bool active=(static_cast<uint8_t*>(s.source.actor.Get())[s.hOffset]&s.hMask)!=0;
    return active==s.active&&(!array.num||!std::memcmp(array.data,s.senders.data(),size_t(array.num)*sizeof(void*)))&&Current(s.source)&&s.gm.Get()&&s.cls.Get();
}
bool Capture(const Owner& owner,Snapshot& out,const std::function<bool()>& current){
    if(!current||!current()||!Current(owner))return false;Snapshot s;s.source=owner;
    void* gm=world_singleton::Gamemode();if(!gm||!R::IsLive(gm))return false;s.gm.Set(gm);s.cls.Set(R::ClassOf(gm));
    if(!s.cls.Get()||!Typed(s.cls.Get(),L"mainGamemode_C")||world_identity::WorldOf(gm)!=owner.world||
        !Field(s.cls.Get(),L"activeEvents",L"IntProperty",4,s.countOffset)||!Field(s.cls.Get(),L"activeEvents_senders",L"ArrayProperty",16,s.arrayOffset)||
        !H(owner,s.hOffset,s.hMask)||!Bound(s))return false;
    if(s.countOffset<s.arrayOffset+16&&s.arrayOffset<s.countOffset+4)return false;
    std::memcpy(&s.count,static_cast<uint8_t*>(s.gm.Get())+s.countOffset,4);std::memcpy(&s.array,static_cast<uint8_t*>(s.gm.Get())+s.arrayOffset,16);
    if(s.count<0||s.array.num<0||s.array.num>65536||s.array.max<s.array.num||s.array.max>65536||(s.array.max&&!s.array.data))return false;
    s.active=(static_cast<uint8_t*>(owner.actor.Get())[s.hOffset]&s.hMask)!=0;
    if(s.array.num)s.senders.assign(s.array.data,s.array.data+s.array.num);
    s.own=int(std::count(s.senders.begin(),s.senders.end(),owner.actor.Raw()));
    if(!current()||!Stable(s))return false;out=std::move(s);return true;
}
struct Param {const wchar_t* name;int size;int offset=-1;};
bool Params(void* function,std::initializer_list<Param*> fields){
    const int size=R::FunctionFrameSize(function);if(size<=0||size>65536)return false;const auto params=R::FunctionParams(function);std::vector<std::pair<int,int>> ranges;
    for(auto* field:fields){int count=0;for(const auto& p:params)if(!_wcsicmp(p.name.c_str(),field->name)){if(p.size!=field->size)return false;field->offset=p.offset;++count;}
        if(count!=1||field->offset<0||size<field->size||field->offset>size-field->size)return false;
        for(const auto& range:ranges)if(field->offset<range.second&&range.first<field->offset+field->size)return false;ranges.emplace_back(field->offset,field->offset+field->size);
    }return true;
}
}
bool Identify(void* actor,Owner& out){
    if(!actor||!llama::IsLlama(actor))return false;Owner owner;owner.actor.Set(actor);owner.cls.Set(R::ClassOf(actor));
    owner.world=world_identity::CurrentWorld();owner.generation=world_identity::Generation();if(!Current(owner))return false;out=owner;return true;
}
bool Retired(const Owner& owner){const auto world=world_identity::CurrentWorld();
    return !owner.actor.Is(owner.actor.Raw())||!owner.cls.Is(owner.cls.Raw())||R::ClassOf(owner.actor.Raw())!=owner.cls.Raw()||
        owner.generation!=world_identity::Generation()||(world&&world!=owner.world);
}
Result Cleanup(const Owner& owner,bool& guarded,const std::function<bool()>& current){
    if(Retired(owner))return Result::Retired;Snapshot s;if(!Capture(owner,s,current))return Retired(owner)?Result::Retired:Result::Retry;
    if(s.count<s.own)return Result::Retry;
    // The last callback-capable predicate runs before revalidating the complete
    // owned snapshot. The commit below contains no native/reflection callbacks.
    if(!current()||!Stable(s))return Retired(owner)?Result::Retired:Result::Retry;
    if(s.active||s.own)guarded=true;
    if(s.own){int32_t keep=0;for(void* sender:s.senders)if(sender!=owner.actor.Raw())s.array.data[keep++]=sender;
        for(int i=keep;i<s.array.num;++i)s.array.data[i]=nullptr;
        const int32_t count=s.count-s.own;auto* base=static_cast<uint8_t*>(s.gm.Raw());
        std::memcpy(base+s.countOffset,&count,4);std::memcpy(base+s.arrayOffset+offsetof(Array,num),&keep,4);
    }
    if(s.active)static_cast<uint8_t*>(owner.actor.Raw())[s.hOffset]&=uint8_t(~s.hMask);
    return Result::Done;
}
bool Absent(const Owner& owner,bool& out,const std::function<bool()>& current){
    Snapshot s;if(!Capture(owner,s,current)||!current()||!Stable(s))return false;out=s.own==0;return true;
}
bool ReadEvent(void* function,const uint8_t* parameters,Event& out){
    if(!function||!parameters||!R::IsLive(function))return false;CachedObjRef fn;fn.Set(function);
    if(!R::NameEquals(R::NameOf(function),L"setEvent"))return false;Param active{L"isEventActive",1},ambient{L"deactivateAmbientTrack",1},context{L"__WorldContext",8};
    if(!Params(function,{&active,&ambient,&context})||!fn.Get())return false;uint8_t a=parameters[active.offset],b=parameters[ambient.offset];void* actor=nullptr;
    std::memcpy(&actor,parameters+context.offset,8);if(a>1||b>1||!actor||!R::IsLive(actor)||!llama::IsLlama(actor)||!fn.Get())return false;
    Event result;result.actor.Set(actor);result.cls.Set(R::ClassOf(actor));result.active=a!=0;result.ambient=b!=0;
    if(!fn.Get()||!result.actor.Get()||!result.cls.Get()||R::ClassOf(result.actor.Get())!=result.cls.Get())return false;out=result;return true;
}
}
