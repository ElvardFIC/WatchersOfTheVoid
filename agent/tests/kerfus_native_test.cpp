#ifndef KERFUS_NATIVE_SOURCE
#define KERFUS_NATIVE_SOURCE "../../src/votv-coop/src/ue_wrap/actors/kerfus.cpp"
#endif
#include KERFUS_NATIVE_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace K=ue_wrap::kerfus;
struct Object{int index=0,serial=1,cls=100;bool live=true;uint8_t flags=0x80,alternate=0x40;float energy=10,altEnergy=20;} objects[256];
uint32_t world=1;void* worldObject=&objects[255];
bool metadata=true,descendant=true,nativeResult=true,variant=false,fields=true;
int size=sizeof(Object),energyWidth=4,frameSize=8,paramSize=1,paramOffset=3,dispatches=0,functionSerial=0;
int boolOffset=offsetof(Object,flags),energyOffset=offsetof(Object,energy);uint8_t activeMask=1,chargingMask=2,paramMask=0x20;
std::wstring energyType=L"FloatProperty";
#ifdef INSERT_READ_NATIVE
std::wstring insertFieldName=L"CallFunc_getMainPlayer_AsMain_Player_1";
#endif
#if defined(KERFUS_ACTION_NATIVE) || defined(OMEGA_VERB_NATIVE)
int playerOff=8,actionOff=2,playerBytes=8,actionBytes=1;
std::wstring playerType=L"ObjectProperty",actionType=L"ByteProperty";
void* expectedPlayer=&objects[5];uint8_t expectedAction=8;
#ifdef OMEGA_VERB_NATIVE
std::wstring expectedText=L"follow";
#endif
#endif
std::function<void(const char*)> hook;
void Call(const char* name){auto fn=hook;if(fn)fn(name);}
void Reset(){hook={};++world;for(int i=0;i<256;++i){objects[i].index=i;++objects[i].serial;objects[i].cls=100;objects[i].live=true;objects[i].flags=0x80;objects[i].alternate=0x40;objects[i].energy=10;objects[i].altEnergy=20;}functionSerial=objects[200].serial;K::g_state.clear();metadata=descendant=nativeResult=fields=true;variant=false;size=sizeof(Object);energyWidth=4;frameSize=8;paramSize=1;paramOffset=3;paramMask=0x20;dispatches=0;boolOffset=offsetof(Object,flags);energyOffset=offsetof(Object,energy);activeMask=1;chargingMask=2;energyType=L"FloatProperty";worldObject=&objects[255];}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Object*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<256?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* p){return p&&static_cast<test::Object*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<256&&p==&test::objects[i]&&test::objects[i].live;}
void* ClassOf(void* p){test::Call("class");return p?&test::objects[static_cast<test::Object*>(p)->cls]:nullptr;}
int32_t StructSize(void*){test::Call("size");return test::size;}
bool FindBoolProperty(void* owner,const wchar_t* name,int32_t& offset,uint8_t& mask){if(!test::metadata)return false;
 if(owner==&test::objects[200]||owner==&test::objects[201]){CHECK(test::objects[200].serial==test::functionSerial);offset=test::paramOffset;mask=test::paramMask;test::Call("param");return true;}
 const bool alt=owner==&test::objects[101]&&test::variant;offset=alt?offsetof(test::Object,alternate):test::boolOffset;
 mask=std::wstring(name)==L"active"?(alt?4:test::activeMask):(alt?8:test::chargingMask);test::Call("bool");return true;}
bool PropertyOffsetAndClass(void* owner,const wchar_t* name,int32_t& offset,std::wstring& type){
#if defined(KERFUS_ACTION_NATIVE) || defined(OMEGA_VERB_NATIVE)
if(std::wstring(name)==L"player"||std::wstring(name)==L"action"||std::wstring(name)==L"Player"||std::wstring(name)==L"Name"){
 CHECK(test::objects[200].serial==test::functionSerial);const bool player=(std::wstring(name)==L"player"||std::wstring(name)==L"Player");offset=player?test::playerOff:test::actionOff;type=player?test::playerType:test::actionType;test::Call(player?"playerField":"actionField");return test::metadata;}
#endif
offset=owner==&test::objects[101]&&test::variant?offsetof(test::Object,altEnergy):test::energyOffset;type=test::energyType;test::Call("energyMeta");return test::metadata;}
std::vector<StructFieldInfo> EnumerateStructFields(void* owner){test::Call("fields");if(!test::fields)return {};
#ifdef INSERT_READ_NATIVE
return {{test::insertFieldName,test::energyOffset,test::energyWidth,0}};
#else
return {{L"energy",owner==&test::objects[101]&&test::variant?int(offsetof(test::Object,altEnergy)):test::energyOffset,test::energyWidth,0}};
#endif
}

void* SuperStructOf(void* owner){test::Call("super");return owner==&test::objects[101]?&test::objects[100]:nullptr;}
bool IsDescendantOfAny(void*,void* const*,size_t,int){test::Call("descendant");return test::descendant;}
void* FindDispatchFunctionCached(void*,const wchar_t*){test::Call("function");return test::metadata?&test::objects[200]:nullptr;}
int32_t FunctionFrameSize(void*){test::Call("frame");return test::frameSize;}
int32_t FindParamSize(void*,const wchar_t* name){
#if defined(KERFUS_ACTION_NATIVE) || defined(OMEGA_VERB_NATIVE)
if(std::wstring(name)==L"player"||std::wstring(name)==L"action"||std::wstring(name)==L"Player"||std::wstring(name)==L"Name"){const bool player=(std::wstring(name)==L"player"||std::wstring(name)==L"Player");test::Call(player?"playerSize":"actionSize");return player?test::playerBytes:test::actionBytes;}
#endif
test::Call("paramSize");return test::paramSize;}
bool CallFunction(void*,void*,void* bytes){++test::dispatches;
#ifdef OMEGA_VERB_NATIVE
void* player=nullptr;std::memcpy(&player,static_cast<uint8_t*>(bytes)+test::playerOff,sizeof player);CHECK(player==test::expectedPlayer);
struct Text{const wchar_t* data;int32_t num,max;} text{};std::memcpy(&text,static_cast<uint8_t*>(bytes)+test::actionOff,sizeof text);CHECK(text.num==int32_t(test::expectedText.size()+1)&&text.max==text.num&&std::wstring(text.data)==test::expectedText);
for(int i=0;i<test::frameSize;++i)if((i<test::playerOff||i>=test::playerOff+8)&&(i<test::actionOff||i>=test::actionOff+16))CHECK(static_cast<uint8_t*>(bytes)[i]==0);
#elif defined(KERFUS_ACTION_NATIVE)

void* player=nullptr;std::memcpy(&player,static_cast<uint8_t*>(bytes)+test::playerOff,sizeof player);CHECK(player==test::expectedPlayer);CHECK(static_cast<uint8_t*>(bytes)[test::actionOff]==test::expectedAction);
for(int i=0;i<test::frameSize;++i)if(i!=test::actionOff&&(i<test::playerOff||i>=test::playerOff+8))CHECK(static_cast<uint8_t*>(bytes)[i]==0);
#else
CHECK(static_cast<uint8_t*>(bytes)[test::paramOffset]==test::paramMask);for(int i=0;i<test::frameSize;++i)if(i!=test::paramOffset)CHECK(static_cast<uint8_t*>(bytes)[i]==0);
#endif
test::Call("dispatch");return test::nativeResult;}

int32_t FindPropertyOffset(void*,const wchar_t*){CHECK(false);return -1;}
int32_t SlotFlags(int32_t){CHECK(false);return 0;}
const FName& NameOf(void*){CHECK(false);static FName n;return n;}
bool NameStartsWith(const FName&,const wchar_t*){CHECK(false);return false;}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t*){test::Call("base");return test::metadata?&test::objects[100]:nullptr;}
size_t ForEachInstance(void*,InstanceFn,void*){CHECK(false);return 0;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return test::worldObject;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
namespace ue_wrap {
ParamFrame::ParamFrame(void*){CHECK(false);}bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){CHECK(false);return false;}bool Call(void*,ParamFrame&){CHECK(false);return false;}
}
#ifndef KERFUS_NATIVE_NO_MAIN
int main(){using namespace test;
 Reset();CHECK(K::WriteActive(&objects[0],true)&&objects[0].flags==0x81);CHECK(K::WriteCharging(&objects[0],true)&&objects[0].flags==0x83);CHECK(K::WriteActive(&objects[0],false)&&objects[0].flags==0x82);
 objects[1].cls=101;variant=true;CHECK(K::WriteActive(&objects[1],true)&&objects[1].alternate==0x44&&objects[1].flags==0x80);CHECK(K::WriteEnergy(&objects[1],333)&&objects[1].altEnergy==333&&objects[1].energy==10);
 ++objects[101].serial;variant=false;CHECK(K::WriteCharging(&objects[1],true)&&objects[1].flags==0x82);puts("PASS KD20");
 for(int fault=0;fault<9;++fault){Reset();if(fault==0)energyType=L"DoubleProperty";if(fault==1)energyWidth=8;if(fault==2)size=energyOffset+3;if(fault==3)chargingMask=activeMask;if(fault==4)energyOffset=boolOffset;if(fault==5)activeMask=0;if(fault==6)boolOffset=-1;if(fault==7)boolOffset=size;if(fault==8)fields=false;float value=99;CHECK(!K::ReadEnergy(&objects[0],value)&&value==99&&K::g_state.empty());}
 Reset();objects[0].energy=INFINITY;float value=99;CHECK(!K::ReadEnergy(&objects[0],value)&&value==99);CHECK(!K::WriteEnergy(&objects[0],INFINITY)&&objects[0].energy==INFINITY);Reset();bool okay=true;hook=[&](const char* name){if(std::string(name)=="energyMeta")okay=false;};value=99;CHECK(!K::ReadEnergyWhile(&objects[0],value,[&]{return okay;})&&value==99);puts("PASS KD21");
 Reset();bool dispatched=false;CHECK(K::RunUpdWhile(&objects[0],true,{},&dispatched)&&dispatched&&dispatches==1);
 for(int fault=0;fault<7;++fault){Reset();if(fault==0)frameSize=0;if(fault==1)frameSize=4097;if(fault==2)paramOffset=frameSize;if(fault==3)paramOffset=-1;if(fault==4)paramMask=0;if(fault==5)paramSize=4;if(fault==6)metadata=false;dispatched=true;CHECK(!K::RunUpdWhile(&objects[0],true,{},&dispatched)&&!dispatched&&!dispatches);}
 for(auto stage:{"frame","param","paramSize"})for(int cause=0;cause<4;++cause){Reset();bool fired=false;hook=[&](const char* name){if(!fired&&std::string(name)==stage){fired=true;if(cause==0)++world;if(cause==1)++objects[0].serial;if(cause==2)++objects[100].serial;if(cause==3)++objects[200].serial;}};dispatched=true;CHECK(!K::RunUpdWhile(&objects[0],true,{},&dispatched)&&fired&&!dispatched&&!dispatches);}
 Reset();nativeResult=false;dispatched=false;CHECK(!K::RunUpdWhile(&objects[0],true,{},&dispatched)&&dispatched&&dispatches==1);Reset();hook=[](const char* n){if(std::string(n)=="dispatch")++world;};CHECK(!K::RunUpdWhile(&objects[0],true,{},&dispatched)&&dispatched&&dispatches==1);puts("PASS KD22");
 Reset();metadata=false;value=99;CHECK(!K::ReadEnergy(&objects[0],value));metadata=true;CHECK(K::ReadEnergy(&objects[0],value)&&value==10);++objects[100].serial;energyOffset=offsetof(Object,altEnergy);CHECK(K::ReadEnergy(&objects[0],value)&&value==20);
 Reset();bool fired=false;hook=[&](const char* name){if(std::string(name)=="fields"&&!fired){fired=true;objects[1].cls=101;variant=true;bool active=false;CHECK(K::ReadActive(&objects[1],active));}};CHECK(K::ReadEnergy(&objects[0],value)&&value==10&&K::g_state.size()==2&&fired);bool active=false;CHECK(K::ReadActive(&objects[0],active)&&!active&&K::IsKerfus(&objects[0]));puts("PASS KD23");
 puts("PASS Kerfus native wrappers: actual layouts, weak references, guarded reads/writes and upd dispatch");
}


#endif
