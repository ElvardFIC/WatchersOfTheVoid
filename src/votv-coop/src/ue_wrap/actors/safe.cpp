#include "ue_wrap/actors/safe.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
namespace ue_wrap::safe {
namespace R=reflection;
bool IsSafe(void* actor) {
    if(!actor||!R::IsLive(actor))return false;
    int depth=0;
    for(void* c=R::ClassOf(actor);c&&depth++<64;c=R::SuperStructOf(c))
        if(R::NameEquals(R::NameOf(c),L"prop_safe_C"))return true;
    return false;
}
bool ReadLocked(void* actor,bool& locked) {
    int32_t off=-1;uint8_t mask=0;
    if(!IsSafe(actor)||!R::FindBoolProperty(R::ClassOf(actor),L"locked",off,mask)||off<0||!mask)return false;
    locked=(static_cast<uint8_t*>(actor)[off]&mask)!=0;return true;
}
bool Unlock(void* actor) {
    bool locked=true;if(!ReadLocked(actor,locked))return false;
    if(!locked)return true; // repeated completion must not create the loot again
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(actor),L"hacked"));
    return frame.valid()&&Call(actor,frame);
}
bool PreserveUnlocked(void* actor,save_record::SaveRecord& record) {
    bool locked=true;
    if(!ReadLocked(actor,locked)||locked)return false;
    return PreserveHostLock(actor,record);
}
bool PreserveHostLock(void* actor,save_record::SaveRecord& record) {
    bool locked=true;if(!ReadLocked(actor,locked))return false;
    if(record.bools.size()>1&&!record.bools[1].empty()&&record.bools[1][0]==uint8_t(locked))return false;
    if(record.bools.size()<2)record.bools.resize(2);
    if(record.bools[1].empty())record.bools[1].resize(1);
    record.bools[1][0]=uint8_t(locked);return true;
}
}
