#pragma once
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <string>
namespace coop::npc_world_enum::lifetime {
inline bool OutputAllowed(const wchar_t* const* outputs,const std::wstring& name){
    if(!outputs)return true;for(size_t i=0;outputs[i];++i)if(!_wcsicmp(outputs[i],name.c_str()))return true;return false;
}
struct Pending {
    net::Session* session=nullptr;uint32_t serial=0,generation=0;void* world=nullptr;
    ue_wrap::CachedObjRef actor,cls;
    Pending(net::Session* s,void* object):session(s),serial(net::session_serial::Current()),
        generation(ue_wrap::world_identity::Generation()),world(ue_wrap::world_identity::CurrentWorld()){
        if(object&&ue_wrap::reflection::IsLive(object)){actor.Set(object);cls.Set(ue_wrap::reflection::ClassOf(object));}
    }
    bool Current(net::Session* s)const{return s&&s==session&&s->running()&&s->role()==net::Role::Host&&
        serial==net::session_serial::Current()&&world&&world==ue_wrap::world_identity::CurrentWorld()&&
        generation==ue_wrap::world_identity::Generation()&&actor.Get()&&cls.Get()&&ue_wrap::reflection::ClassOf(actor.Get())==cls.Get();}
};
}
