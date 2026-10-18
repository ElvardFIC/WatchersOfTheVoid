#include "../../src/votv-coop/src/ue_wrap/actors/physics_gun.cpp"
#include <cassert>
#include <cwchar>
#include <limits>
#include <cstdio>
namespace test {
struct Obj { int index=0,serial=1; bool live=true,active=false,ownerOnly=true;
 void* cls=nullptr; void* parent=nullptr; ue_wrap::reflection::FName name{};
 uint8_t held=0; void* target=nullptr; void* component=nullptr;
 void* object=nullptr; void* parts[10]{}; ue_wrap::FVector at{}; } objects[30];
Obj& gun=objects[0];Obj& mirror=objects[1];Obj& phys=objects[2];Obj& grav=objects[3];
ue_wrap::FVector beamEnd{}; bool destroyOnRead=false; int nativeSets=0;
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return static_cast<test::Obj*>(p)->index;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<30&&p==&test::objects[i]&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<30?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
void* SuperStructOf(void* p){return static_cast<test::Obj*>(p)->parent;}
const FName& NameOf(void* p){return static_cast<test::Obj*>(p)->name;}
bool NameEquals(const FName& n,const wchar_t* s){return (n.ComparisonIndex==10&&!wcscmp(s,L"prop_physgun_C"))||
 (n.ComparisonIndex==11&&!wcscmp(s,L"prop_gravgun_C"));}
int32_t FindPropertyOffset(void*,const wchar_t* n){return !wcscmp(n,L"objectPickup")?
 offsetof(test::Obj,target):offsetof(test::Obj,component);}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,held);mask=1;return true;}
int32_t InstanceOffset::Of(void*) {
 const wchar_t* names[]={L"beam",L"eff_gphysgunBeam_start",L"eff_gphysgunBeam_end",L"softPivot",
 L"object_hold_loop",L"object_grab",L"object_throw",L"object_release",L"eff_gravigunN1",L"eff_gravigunN2"};
 if(!wcscmp(prop_,L"obj"))return offsetof(test::Obj,object);
 for(int i=0;i<10;++i)if(!wcscmp(prop_,names[i]))return offsetof(test::Obj,parts)+i*sizeof(void*);
 return -1;
}
void* FindDispatchFunctionCached(void*,const wchar_t* n){return const_cast<wchar_t*>(n);}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::prop {bool IsDescendantOfProp(void*){return true;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(128){}
bool ParamFrame::SetRaw(const wchar_t* n,const void* p,int32_t size){
 if(!wcscmp(n,L"obj")||!wcscmp(n,L"Condition"))std::memcpy(buf_.data(),p,size);
 if(!wcscmp(n,L"NewLocation"))std::memcpy(buf_.data(),p,size);
 if(!wcscmp(n,L"NewEndPoint"))std::memcpy(&test::beamEnd,p,size);
 if(!wcscmp(n,L"bSweep"))assert(!*static_cast<const bool*>(p));
 if(!wcscmp(n,L"bTeleport"))assert(*static_cast<const bool*>(p));
 if(!wcscmp(n,L"bNewOnlyOwnerSee")||!wcscmp(n,L"bNewOwnerNoSee"))assert(!*static_cast<const bool*>(p));
 return valid();
}
bool ParamFrame::GetRaw(const wchar_t*,void* p,int32_t size)const{std::memcpy(p,buf_.data(),size);return true;}
bool Call(void* p,ParamFrame& f){
 auto& o=*static_cast<test::Obj*>(p);const auto* name=static_cast<wchar_t*>(f.function());
 if(!wcscmp(name,L"K2_GetComponentLocation"))std::memcpy(f.data(),&o.at,sizeof(o.at));
 if(!wcscmp(name,L"K2_SetWorldLocation"))std::memcpy(&o.at,f.data(),sizeof(o.at));
 if(!wcscmp(name,L"IsActive")){std::memcpy(f.data(),&o.active,sizeof(bool));if(test::destroyOnRead)test::gun.live=false;}
 if(!wcscmp(name,L"set")){std::memcpy(&o.object,f.data(),sizeof(void*));++test::nativeSets;}
 if(!wcscmp(name,L"attract")){bool on=false;std::memcpy(&on,f.data(),sizeof(bool));static_cast<test::Obj*>(o.parts[9])->active=on;}
 if(!wcscmp(name,L"SetOnlyOwnerSee"))o.ownerOnly=false;
 return true;
}
}
namespace ue_wrap::component_calls {bool SetActive(void* p,bool active,bool reset){
 assert(!reset);if(!p)return false;static_cast<test::Obj*>(p)->active=active;return true;
}}
int main(){
 using namespace test;namespace G=ue_wrap::physics_gun;
 for(int i=0;i<30;++i){objects[i].index=i;objects[i].cls=&phys;}
 phys.name={10,0};grav.name={11,0};gun.cls=mirror.cls=&phys;
 for(int i=0;i<10;++i){gun.parts[i]=&objects[4+i];mirror.parts[i]=&objects[14+i];}
 gun.held=1;objects[7].at={100,200,300};objects[15].at={10,20,30};
 const auto fx=G::ReadVisuals(&gun);assert(fx.kind==1&&fx.flags==1&&fx.target.X==100);
 G::ApplyVisuals(&mirror,fx);
 assert(objects[14].active&&objects[15].active&&objects[16].active);
 assert(!objects[14].ownerOnly&&objects[17].at.X==100&&objects[16].at.Z==300);
 assert(beamEnd.X==10&&beamEnd.Y==20); // Beam's native endpoint is the muzzle.
 G::ApplyVisuals(&mirror,{});assert(!objects[14].active&&!objects[16].active);
 gun.cls=mirror.cls=&grav;objects[8].active=true;objects[9].active=true;objects[10].active=true;
 gun.object=&objects[24];objects[13].active=true;
 auto gravity=G::ReadVisuals(&gun);assert(gravity.kind==2&&gravity.flags==23);
 G::ApplyVisuals(&mirror,gravity);assert(objects[18].active&&objects[19].active&&objects[20].active&&!objects[21].active);
 assert(mirror.object==gun.object&&nativeSets==1);G::ApplyVisuals(&mirror,gravity);assert(nativeSets==1);
 G::ApplyVisuals(&mirror,fx);assert(!objects[18].active&&!objects[20].active&&!mirror.object&&nativeSets==2); // Wrong tool kind cannot enable effects.
 destroyOnRead=true;assert(G::ReadVisuals(&gun).kind==0); // Callback destroys the owner safely.
 gun.live=true;destroyOnRead=false;gun.cls=&phys;objects[7].at.X=std::numeric_limits<float>::quiet_NaN();
 assert(G::ReadVisuals(&gun).kind==0);
 puts("PASS physgun endpoint/third-person visibility, gravgun FX, off/type guards and callback lifetime");
}
