#ifndef CHILD_RELEASE_SOURCE
#define CHILD_RELEASE_SOURCE "../../src/votv-coop/src/coop/props/prop_child_release.cpp"
#endif
#include CHILD_RELEASE_SOURCE
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace CR=coop::prop_child_release;
namespace test {
struct Obj {void* cls=nullptr;int id=0,serial=1;bool live=true,held=false;void* mesh=nullptr;ue_wrap::FVector loc{1,2,3};ue_wrap::FRotator rot{4,5,6};};
Obj cls,otherClass,parent,parent2,mesh,mesh2;
std::array<Obj,80> doors;
std::vector<Obj*> objects;
Obj& door=doors[0];
uint64_t now=100;uint32_t world=1;
int locations=0,rotations=0,unparks=0,locReads=0,rotReads=0,meshReads=0;
bool readLoc=true,readRot=true,writeLoc=true,writeRot=true;
std::function<void()> onReadLoc,onReadRot,onLoc,onRot,onMesh,onUnpark,onHand;
void Fire(std::function<void()>& cb){auto f=std::move(cb);cb={};if(f)f();}
void Init(Obj& o){o.cls=&cls;o.live=true;o.held=false;o.mesh=&mesh;o.loc={1,2,3};o.rot={4,5,6};++o.serial;if(!o.id){o.id=int(objects.size()+1);objects.push_back(&o);}}
void Reset(){
    onReadLoc={};onReadRot={};onLoc={};onRot={};onMesh={};onUnpark={};onHand={};
    CR::g_waits.clear();++CR::g_epoch;++world;now=100;
    for(auto* o:{&cls,&otherClass,&parent,&parent2,&mesh,&mesh2})Init(*o);
    for(auto& d:doors)Init(d);
    locations=rotations=unparks=locReads=rotReads=meshReads=0;readLoc=readRot=writeLoc=writeRot=true;
}
void Defer(){CR::Defer(&door,&parent);}
void Release(){CR::OnParentReleased(&parent);}
void Pass(int n){std::printf("PASS CR%02d\n",n);}
}
namespace ue_wrap::reflection {
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
void* ObjectAt(int32_t i){return i>0&&size_t(i)<=test::objects.size()?test::objects[i-1]:nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}
int32_t SlotSerial(int32_t i){auto* p=static_cast<test::Obj*>(ObjectAt(i));return p?p->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long long CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 1;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::engine {
bool TryGetActorLocation(void* p,FVector& out){++test::locReads;out=static_cast<test::Obj*>(p)->loc;test::Fire(test::onReadLoc);return test::readLoc;}
bool TryGetActorRotation(void* p,FRotator& out){++test::rotReads;out=static_cast<test::Obj*>(p)->rot;test::Fire(test::onReadRot);return test::readRot;}
bool SetActorLocation(void* p,const FVector& v){++test::locations;if(test::writeLoc)static_cast<test::Obj*>(p)->loc=v;test::Fire(test::onLoc);return test::writeLoc;}
bool SetActorRotation(void* p,const FRotator& v){++test::rotations;if(test::writeRot)static_cast<test::Obj*>(p)->rot=v;test::Fire(test::onRot);return test::writeRot;}
}
namespace ue_wrap::prop {void* GetStaticMesh(void* p){++test::meshReads;void* m=static_cast<test::Obj*>(p)->mesh;test::Fire(test::onMesh);return m;}}
namespace coop::prop_drive_host {bool InAHand(void* p){test::Fire(test::onHand);return static_cast<test::Obj*>(p)->held;}}
namespace coop::prop_park {void Unpark(void*,void*,bool simulates){CHECK(simulates);++test::unparks;test::Fire(test::onUnpark);}}
int main(){
    using namespace test;
    Reset();CR::Defer(nullptr,&parent);door.live=false;Defer();CHECK(CR::g_waits.empty()&&locReads==0);Pass(1);
    Reset();CR::Defer(&door,&door);CHECK(CR::g_waits.empty()&&locReads==0);Pass(2);
    Reset();Defer();++world;Release();CHECK(unparks==0&&locations==0&&CR::g_waits.empty());Pass(3);
    Reset();onReadLoc=[]{for(size_t i=1;i<doors.size();++i)CR::Defer(&doors[i],&parent);};Defer();Release();CHECK(unparks==80&&locations==80);Pass(4);
    Reset();onReadLoc=[]{CR::Forget(&door);};Defer();CHECK(CR::g_waits.empty());Release();CHECK(unparks==0);Pass(5);
    Reset();onReadLoc=[]{door.loc.X=91;CR::Defer(&door,&parent2);};Defer();Release();CHECK(unparks==0);CR::OnParentReleased(&parent2);CHECK(unparks==1&&door.loc.X==91);Pass(6);
    Reset();door.loc.X=NAN;Defer();Release();CHECK(unparks==1&&locations==0&&rotations==0);Pass(7);
    Reset();door.rot.Yaw=INFINITY;Defer();Release();CHECK(unparks==1&&locations==0&&rotations==0);Pass(8);
    Reset();readLoc=false;Defer();Release();CHECK(unparks==1&&locations==0&&rotReads==0);Pass(9);
    Reset();readRot=false;Defer();Release();CHECK(unparks==1&&locations==0&&rotations==0);Pass(10);
    Reset();Defer();door.cls=&otherClass;Release();CHECK(unparks==0&&locations==0);Pass(11);
    Reset();Defer();parent.cls=&otherClass;CR::Tick(now);CHECK(unparks==1&&locations==0);Pass(12);
    Reset();Defer();++parent.serial;Release();CHECK(locations==0);Pass(13);
    Reset();Defer();CR::OnParentReleased(&parent2);CHECK(unparks==0);Release();CHECK(unparks==1);
    Reset();now=200;Defer();CR::Tick(100);CHECK(unparks==0&&!CR::g_waits.empty());Pass(14);
    Reset();Defer();parent.live=false;CR::Tick(now);CHECK(unparks==1&&locations==0&&CR::g_waits.empty());Pass(15);
    Reset();Defer();CR::Tick(now+CR::kChildWaitMs-1);CHECK(unparks==0);CR::Tick(now+CR::kChildWaitMs);CHECK(unparks==1&&locations==1);Pass(16);
    Reset();Defer();onLoc=[]{CR::Forget(&door);CR::Defer(&doors[1],&parent2);};Release();CHECK(unparks==0&&CR::g_waits.size()==1);CR::OnParentReleased(&parent2);CHECK(unparks==1);Pass(17);
    Reset();Defer();onLoc=[]{CR::Defer(&doors[1],&parent2);};CR::Tick(now+CR::kChildWaitMs);CHECK(unparks==1&&CR::g_waits.size()==1);Pass(18);
    Reset();Defer();onLoc=[]{door.loc.X=77;CR::Defer(&door,&parent2);};Release();CHECK(rotations==0&&unparks==0);CR::OnParentReleased(&parent2);CHECK(unparks==1&&door.loc.X==77);Pass(19);
    Reset();Defer();door.held=true;Release();CHECK(locations==0&&unparks==0&&CR::g_waits.empty());Pass(20);
    Reset();Defer();onLoc=[]{door.held=true;};Release();CHECK(locations==1&&rotations==0&&unparks==0);Pass(21);
    Reset();Defer();onRot=[]{door.held=true;};Release();CHECK(rotations==1&&unparks==0);Pass(22);
    Reset();Defer();onMesh=[]{door.mesh=&mesh2;};Release();CHECK(unparks==0&&CR::g_waits.size()==1);CR::Tick(now);CHECK(unparks==1);Pass(23);
    Reset();Defer();writeLoc=false;Release();CHECK(rotations==0&&unparks==0&&CR::g_waits.size()==1);writeLoc=true;CR::Tick(now);CHECK(unparks==1);Pass(24);
    Reset();Defer();writeRot=false;Release();CHECK(unparks==0&&CR::g_waits.size()==1);writeRot=true;CR::Tick(now);CHECK(unparks==1);Pass(25);
    Reset();Defer();CR::Defer(&doors[1],&parent2);onLoc=[]{CR::OnParentReleased(&parent2);CR::Tick(now);};Release();CR::Tick(now);CHECK(unparks==2&&locations==2&&CR::g_waits.empty());Pass(26);
    Reset();onReadRot=[]{Release();};Defer();CHECK(unparks==0);CR::Tick(now);CHECK(unparks==1);
    Reset();Defer();onUnpark=[]{Release();CR::Tick(now);};Release();CHECK(unparks==1&&CR::g_waits.empty());Pass(27);
    Reset();Defer();onUnpark=[]{CR::OnDisconnect();};Release();CHECK(unparks==1&&CR::g_waits.empty());
    Reset();Defer();onUnpark=[]{CR::Defer(&door,&parent2);CR::OnDisconnect();};CR::OnDisconnect();CHECK(unparks==1&&CR::g_waits.empty());Pass(28);
    Reset();Defer();door.held=true;CR::OnDisconnect();CHECK(unparks==0&&CR::g_waits.empty());Pass(29);
    Reset();Defer();onMesh=[]{onMesh=[]{door.held=true;};};CR::OnDisconnect();CHECK(unparks==0);
    Reset();onReadLoc=[]{CR::OnDisconnect();};Defer();CHECK(rotReads==0&&CR::g_waits.empty());
    Reset();Defer();onLoc=[]{CR::OnDisconnect();};Release();CHECK(rotations==0&&unparks==1&&CR::g_waits.empty());Pass(30);
    std::puts("PASS actual child release: 30 boundaries");
}
