#include "support_2135.h"
#include "../../src/votv-coop/src/ue_wrap/devices/drone.cpp"
// Unexercised flight/FX services must never be reached by a preference read/write.
namespace ue_wrap::reflection {
void* FindClass(const wchar_t*){std::abort();}void* FindFunction(void*,const wchar_t*){std::abort();}
void* FindObjectByClass(const wchar_t*){std::abort();}
int32_t InstanceOffset::Of(void*){std::abort();}
void* FindDispatchFunctionCached(void*,const wchar_t*){std::abort();}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void*){std::abort();}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){std::abort();}
bool ParamFrame::GetRaw(const wchar_t*,void*,int32_t)const{std::abort();}
bool Call(void*,ParamFrame&){std::abort();}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector&){std::abort();}bool TryGetActorRotation(void*,FRotator&){std::abort();}
bool SetActorLocation(void*,const FVector&){std::abort();}bool SetActorRotation(void*,const FRotator&){std::abort();}
bool SetActorTickEnabled(void*,bool){std::abort();}
bool TryGetRootPoseRaw(void*,FVector&,FRotator&){std::abort();}
}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){std::abort();}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){std::abort();}}
namespace ue_wrap::world_singleton {void* Find(const wchar_t*){std::abort();}}
int main(){using namespace test;using namespace ue_wrap::drone;bool value=false;
 actor.flags=0xA0;CHECK(ReadDeparturePreference(&actor,value)&&!value);
 CHECK(WriteDeparturePreference(&actor,true)&&actor.flags==0xA1);
 CHECK(WriteDeparturePreference(&actor,false)&&actor.flags==0xA0);
 ++actor.serial;fields=false;CHECK(!ReadDeparturePreference(&actor,value));CHECK(!WriteDeparturePreference(&actor,true));
 fields=true;CHECK(WriteDeparturePreference(&actor,true)&&actor.flags==0xA1);
 actor.live=false;CHECK(!ReadDeparturePreference(&actor,value));CHECK(!WriteDeparturePreference(nullptr,true));
 puts("PASS actual drone reflection wrapper: preserved adjacent bits, class serial invalidation, missing field, dead/null actor");
}
