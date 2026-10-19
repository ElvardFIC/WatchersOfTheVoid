#include "../../src/votv-coop/src/ue_wrap/actors/sleep_display.cpp"
#include <cassert>
#include <cstdio>
#include <map>
namespace test {
struct Object { int index = 0, serial = 1; bool live = true; void* bed = nullptr; void* player = nullptr; } objects[8];
Object &pawn=objects[0], &source=objects[1], &mesh=objects[2], &asset=objects[3], &bed=objects[4], &gm=objects[5];
bool sleeping=false, traceOk=true, failCopy=false, standingVisible=true;
int created=0, destroyed=0, copies=0, materials=0;
ue_wrap::FVector origin{}, boundsOrigin{100,200,50}, surface{100,200,45};
ue_wrap::FRotator rotation{};
float scale=1;
std::map<uint16_t,ue_wrap::FTransform> boneWrites;
size_t Offset(const wchar_t* name) {
    static std::map<std::wstring,size_t> offsets;
    auto [it, added] = offsets.emplace(name, offsets.size()*64);
    return it->second;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p) { return p && static_cast<test::Object*>(p)->live; }
void* ClassOf(void* p) { return p; }
void* FindClass(const wchar_t*) { return &test::objects[7]; }
void* FindDispatchFunctionCached(void*, const wchar_t* name) { return const_cast<wchar_t*>(name); }
int32_t FindPropertyOffset(void*, const wchar_t* name) {
    return std::wstring(name)==L"bed" ? offsetof(test::Object,bed) :
           std::wstring(name)==L"mainPlayer" ? offsetof(test::Object,player) : -1;
}
int32_t InternalIndexOf(void* p) { return static_cast<test::Object*>(p)->index; }
bool IsLiveByIndex(void* p,int32_t i) { return i>=0 && i<8 && p==&test::objects[i] && IsLive(p); }
int32_t SlotSerial(int32_t i) { return test::objects[i].serial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
}
namespace ue_wrap::world_identity { void* CurrentWorld(){return nullptr;} void* WorldOf(void*){return nullptr;} }
namespace ue_wrap::world_singleton { void* Gamemode(){return &test::gm;} }
namespace ue_wrap::sleep { bool EnsureResolved(){return true;} bool IsSleeping(){return test::sleeping;} }
namespace ue_wrap::trace {
bool FindFloor(void*,const FVector&,const FVector&,FVector& point){point=test::surface;return test::traceOk;}
}
namespace ue_wrap::puppet {
void* GetMeshPlayerVisibleComponent(void*) { return &test::source; }
void* GetNativeBodyMeshComponent(void*) { return &test::source; }
void* GetComponentSkeletalMeshAsset(void*) { return &test::asset; }
}
namespace ue_wrap::engine {
bool SetSkeletalMesh(void* p,void* a){assert(p==&test::mesh&&a==&test::asset);return true;}
bool SetComponentMaterial(void*,int32_t,void*){++test::materials;return true;}
bool SetSceneComponentVisibility(void* p,bool visible,bool){if(p==&test::source)test::standingVisible=visible;return true;}
FVector GetComponentLocation(void*) { return test::origin; }
bool GetBoneWorldLocationByName(void*,const wchar_t*,FVector& point){point=test::origin;point.Z+=40;return true;}
bool TryGetActorRotation(void*,FRotator& rot){rot={0,55,0};return true;}
bool TryGetActorLocation(void*,FVector& loc){loc={10,20,100};return true;}
float GetActorCharacterHalfHeight(void*){return 88;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* function):fn_(function),buf_(4096){}
bool ParamFrame::SetRaw(const wchar_t* name,const void* data,int32_t size){
    std::memcpy(buf_.data()+test::Offset(name),data,size);return valid();
}
bool ParamFrame::GetRaw(const wchar_t* name,void* data,int32_t size)const{
    std::memcpy(data,buf_.data()+test::Offset(name),size);return valid();
}
bool Call(void*,ParamFrame& frame){
    if(!frame.valid())return false;
    const std::wstring name=static_cast<wchar_t*>(frame.function());
    if(name==L"GetActorBounds"){
        frame.Set<FVector>(L"Origin",test::boundsOrigin);frame.Set<FVector>(L"BoxExtent",{80,120,60});
    }else if(name==L"AddComponentByClass"){
        ++test::created;test::mesh.live=true;++test::mesh.serial;
        frame.Set<void*>(L"ReturnValue",&test::mesh);
    }else if(name==L"K2_GetComponentToWorld"){
        FTransform transform{};transform.SX=2;frame.Set<FTransform>(L"ReturnValue",transform);
    }else if(name==L"K2_SetWorldTransform"){
        const auto transform=frame.Get<FTransform>(L"NewTransform");test::scale=transform.SX;
        test::origin={transform.TX,transform.TY,transform.TZ};
    }else if(name==L"CopyPoseFromSkeletalComponent"){
        ++test::copies;assert(frame.Get<void*>(L"InComponentToCopy")==&test::source);
        return !test::failCopy;
    }else if(name==L"GetNumMaterials")frame.Set<int32_t>(L"ReturnValue",2);
    else if(name==L"GetMaterial")frame.Set<void*>(L"ReturnValue",&test::asset);
    else if(name==L"K2_SetWorldRotation")test::rotation=frame.Get<FRotator>(L"NewRotation");
    else if(name==L"K2_SetWorldLocation")test::origin=frame.Get<FVector>(L"NewLocation");
    else if(name==L"SetCollisionEnabled")assert(frame.Get<uint8_t>(L"NewType")==0);
    else if(name==L"K2_DestroyComponent"){++test::destroyed;test::mesh.live=false;}
    else if(name==L"GetBoneName")frame.Set<reflection::FName>(L"ReturnValue",{frame.Get<int32_t>(L"BoneIndex"),0});
    else if(name==L"GetBoneTransformByName"){
        FTransform transform{};transform.SX=test::scale;frame.Set<FTransform>(L"ReturnValue",transform);
    }else if(name==L"SetBoneTransformByName"){
        assert(frame.Get<uint8_t>(L"BoneSpace")==0);
        test::boneWrites[static_cast<uint16_t>(frame.Get<reflection::FName>(L"BoneName").ComparisonIndex)]=frame.Get<FTransform>(L"InTransform");
    }else assert(false&&"unexpected reflected call");
    return true;
}
}
int main(){
    using namespace test;using namespace ue_wrap;
    for(int i=0;i<8;++i)objects[i].index=i;
    gm.bed=&bed;gm.player=&pawn;
    FVector anchor{};float yaw=0;
    assert(!sleep_display::LocalAnchor(anchor,yaw));
    sleeping=true;assert(sleep_display::LocalAnchor(anchor,yaw));
    assert(anchor.X==100&&anchor.Y==200&&anchor.Z==57&&yaw==55); // Mattress, not headboard at 110.
    assert(sleep_display::Drive(&pawn,true,anchor,yaw));
    assert(created==1&&materials==2&&!standingVisible&&scale==2);
    assert(origin.Z+40==57&&rotation.Pitch==90&&rotation.Yaw==-35);
    surface.Z+=20;assert(sleep_display::LocalAnchor(anchor,yaw));
    assert(sleep_display::Drive(&pawn,true,anchor,yaw));
    assert(created==1&&origin.Z+40==77); // Moving bed updates the existing body.
    sleep_display::Drive(&pawn,false,{},0);
    assert(destroyed==1&&standingVisible&&!sleep_display::Mesh(&pawn));
    failCopy=true;assert(!sleep_display::Drive(&pawn,true,anchor,yaw));
    assert(destroyed==2&&standingVisible); // Failed entry leaves no orphan or hidden player.
    failCopy=false;
    std::vector<engine::ArticulatedBone> bones(1);bones[0].index=7;bones[0].world.TX=123;
    assert(sleep_display::DriveArticulated(&pawn,bones));
    assert(boneWrites[7].TX==123&&boneWrites[7].SX==2);
    sleep_display::Remove(&pawn);assert(standingVisible);
    gm.bed=nullptr;traceOk=false;
    assert(sleep_display::LocalAnchor(anchor,yaw)&&anchor.Z==24); // Ground sleep.
    puts("PASS sleep model/material/scale, mattress and moving anchor, entry/exit/failure and articulated pose");
}
