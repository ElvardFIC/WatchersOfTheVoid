#include "../../src/votv-coop/src/coop/world/world_sound_sync.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Obj { int id = 0, serial = 1; bool live = true, actor = false; std::wstring name, cls; void* owner = nullptr; ue_wrap::reflection::FName fname{}; } objs[16];
uint32_t world = 1; bool assets = true, callOK = true, locationOK = true, thread = true;
int hooks = 0, plays = 0; int32_t entry = 5062;
ue_wrap::ufunction_hook::CallerFrame caller{};
std::function<void()> locationCallback, playCallback;
void Init() {
    for (int i = 0; i < 16; ++i) { objs[i].id = i; objs[i].fname.ComparisonIndex = i; }
    objs[0].actor = objs[1].actor = objs[2].actor = true;
    objs[1].cls = L"newsky_C"; objs[2].cls = L"noiser_C";
    objs[3].name = L"ExecuteUbergraph_newsky"; objs[3].owner = &objs[4]; objs[4].name = L"newsky_C";
    objs[5].name = L"GameplayStatics";
    caller = {&objs[3], reinterpret_cast<uint8_t*>(&entry)};
    coop::players::Registry::Get().local = &objs[0];
}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Obj*>(a)->id : -1; }
void* ObjectAt(int32_t id) { return id >= 0 && id < 16 ? &test::objs[id] : nullptr; }
int32_t SlotSerial(int32_t id) { return id >= 0 && id < 16 ? test::objs[id].serial : 0; }
int32_t AllocateSlotSerial(int32_t id) { return SlotSerial(id); }
bool IsLive(void* a) { return a && static_cast<test::Obj*>(a)->live; }
bool IsLiveByIndex(void* a, int32_t id) { return a && a == ObjectAt(id) && IsLive(a); }
void* ClassOf(void* a) { return a; }
std::wstring ClassNameOf(void* a) { return static_cast<test::Obj*>(a)->cls; }
const FName& NameOf(void* a) { return static_cast<test::Obj*>(a)->fname; }
bool NameEquals(const FName& n, const wchar_t* s) { return test::objs[n.ComparisonIndex].name == s; }
void* OuterOf(void* a) { return static_cast<test::Obj*>(a)->owner; }
void* FindClassDefaultObject(const wchar_t*) { return &test::objs[8]; }
void* FindObject(const wchar_t* name, const wchar_t*) {
    if (!test::assets) return nullptr;
    return &test::objs[!wcscmp(name,L"earthTP_in") ? 9 : !wcscmp(name,L"earthTP_out") ? 10 : !wcscmp(name,L"outsideAlien_Cue") ? 11 : !wcscmp(name,L"plantGather_Cue") ? 15 : !wcscmp(name,L"door0_open") ? 14 : 12];
}
void* FindFunction(void*, const wchar_t* name) { return &test::objs[!wcscmp(name,L"PlaySound2D") ? 6 : !wcscmp(name,L"SpawnSoundAttached") ? 7 : 13]; }
void* FindDispatchFunctionCached(void* c, const wchar_t* name) { return FindFunction(c,name); }
int32_t FunctionFrameSize(void*) { return 96; }
std::vector<ParamInfo> FunctionParams(void*) {
    const wchar_t* names[]{L"WorldContextObject",L"Sound",L"VolumeMultiplier",L"PitchMultiplier",L"StartTime",L"ConcurrencySettings",L"OwningActor",L"bIsUISound",L"Location",L"Rotation",L"AttenuationSettings"};
    int offsets[]{0,8,16,20,24,32,40,48,52,64,80}; std::vector<ParamInfo> out;
    for (int i=0;i<11;++i) { ParamInfo p{}; p.name=names[i];p.offset=offsets[i];out.push_back(p); }
    return out;
}
int32_t FindParamOffset(void*, const wchar_t* name) { return !wcscmp(name,L"EntryPoint") ? 0 : -1; }
bool CallFunction(void*, void* function, void* raw) {
    if (!test::callOK) return false;
    auto* bytes = static_cast<uint8_t*>(raw); float volume = 0, pitch = 0;
    std::memcpy(&volume,bytes+16,4);std::memcpy(&pitch,bytes+20,4);
    void* sound=nullptr;std::memcpy(&sound,bytes+8,sizeof(sound));
    if (function == &test::objs[6]) {
        const bool faucet=sound==&test::objs[14];
        CHECK((faucet||sound==&test::objs[9]||sound==&test::objs[10]) && volume==(faucet?.25f:1.5f) && pitch==(faucet?1.f:0.f) && bytes[48]);
    } else {
        const bool plant=sound==&test::objs[15];
        CHECK(function==&test::objs[13] && (plant||sound==&test::objs[11]) && volume==(plant?1.f:2.f) && pitch==1.f);
        ue_wrap::FVector pos{};std::memcpy(&pos,bytes+52,sizeof(pos));CHECK(pos.X==12&&pos.Y==34&&pos.Z==(plant?81.f:56.f));
    }
    ++test::plays; auto cb=std::move(test::playCallback);test::playCallback={};if(cb)cb();return true;
}
}
namespace ue_wrap::game_thread { bool IsGameThread() { return test::thread; } }
namespace ue_wrap::world_identity {
uint32_t Generation() { return test::world; }
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void* a) { return a && static_cast<test::Obj*>(a)->actor ? CurrentWorld() : nullptr; }
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::object_index { void* ClassByName(const wchar_t*) { return &test::objs[5]; } }
namespace ue_wrap::ufunction_hook {
CallerFrame CurrentCallerFrame() { return test::caller; }
bool InstallPostHook(void*,PostNativeCallback,bool) { ++test::hooks; return true; }
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out) { out={12,34,56};auto cb=std::move(test::locationCallback);test::locationCallback={};if(cb)cb();return test::locationOK; }
}
namespace ue_wrap::placed_save {Kind Type(void* a){return reflection::ClassNameOf(a)==L"growingPlant_C"?Kind::Plant:Kind::None;}}
int main() {
    using namespace test; namespace S=coop::world_sound_sync; using coop::net::Role;
    Init(); coop::net::Session s; S::Install(&s); CHECK(hooks==3);
    S::Sound2DPost(nullptr,&objs[1],nullptr); CHECK(S::g_out[1].size()==1);
    entry=437; S::Sound2DPost(nullptr,&objs[1],nullptr); s.blockedSlot=1;
    S::Install(&s); CHECK(s.sent.size()==4 && S::g_out[1].size()==2 && S::g_out[2].empty());
    CHECK(s.sent[0].payload.cueId==S::kEarthIn && s.sent[1].payload.cueId==S::kEarthOut);
    s.blockedSlot=-1; S::Install(&s); CHECK(s.sent.size()==6 && S::g_out[1].empty());
    S::Install(&s); CHECK(s.sent.size()==6 && hooks==3);
    entry=15; S::Sound2DPost(nullptr,&objs[1],nullptr); // unrelated latent entry
    entry=5062; S::Sound2DPost(nullptr,&objs[2],nullptr); CHECK(S::g_out[1].empty());
    s.ready[3]=false; S::AttachedPost(nullptr,&objs[2],&objs[15]); CHECK(S::g_out[3].empty());
    S::Install(&s); CHECK(s.sent.size()==8 && s.sent.back().payload.cueId==S::kAlien && s.sent.back().payload.x==12);
    locationCallback=[] { ++world; }; S::AttachedPost(nullptr,&objs[2],&objs[15]); CHECK(S::g_out[1].empty());
    locationOK=false;S::AttachedPost(nullptr,&objs[2],&objs[15]);CHECK(S::g_out[1].empty());locationOK=true;
    s.blockedSlot=1;S::Sound2DPost(nullptr,&objs[1],nullptr); S::Install(&s);
    S::OnPeerLeftSlot(1); s.blockedSlot=-1; const auto before=s.sent.size();S::Install(&s);CHECK(s.sent.size()==before);
    S::Sound2DPost(nullptr,&objs[1],nullptr);for(auto& q:S::g_out)for(auto& p:q)p.expires=0;
    S::Install(&s);CHECK(s.sent.size()==before);
    objs[1].cls=L"growingPlant_C";objs[3].name=L"harvestSound";objs[4].name=L"growingPlant_C";
    S::LocationPost(nullptr,&objs[1],nullptr);S::Install(&s);
    CHECK(s.sent.back().payload.cueId==S::kPlantGather && s.sent.back().payload.z==81.f);
    objs[1].cls=L"faucet_C";objs[3].name=L"ExecuteUbergraph_faucet";objs[4].name=L"faucet_C";
    S::Sound2DPost(nullptr,&objs[1],nullptr);S::Install(&s);CHECK(s.sent.back().payload.cueId==S::kFaucet);
    objs[1].cls=L"newsky_C";objs[3].name=L"ExecuteUbergraph_newsky";objs[4].name=L"newsky_C";
    s.mode=Role::Client;S::Clear(); assets=false;
    coop::net::EventCuePayload p{S::kEarthIn,0,0,0};CHECK(S::OnReliable(p)&&S::g_in.size()==1&&plays==0);
    assets=true;callOK=false;S::Install(&s);CHECK(plays==0&&S::g_in.size()==1);
    callOK=true;S::Install(&s);CHECK(plays==1&&S::g_in.empty());S::Install(&s);CHECK(plays==1);
    p.cueId=S::kEarthOut;CHECK(S::OnReliable(p)&&plays==2);
    p={S::kAlien,12,34,56};CHECK(S::OnReliable(p)&&plays==3);
    p.x=std::numeric_limits<float>::quiet_NaN();CHECK(S::OnReliable(p)&&plays==3);
    p={S::kEarthIn,1,0,0};CHECK(S::OnReliable(p)&&plays==3);p={99,0,0,0};CHECK(!S::OnReliable(p));
    p={S::kEarthIn,0,0,0};s.mode=Role::Host;CHECK(S::OnReliable(p)&&plays==3);
    s.mode=Role::Client;assets=false;S::OnReliable(p);CHECK(S::g_in.size()==1);
    ++world;assets=true;S::Install(&s);CHECK(S::g_in.empty()&&plays==3);
    playCallback=[] { S::OnDisconnect(); }; S::OnReliable(p);CHECK(plays==4&&S::g_in.empty());
    S::Install(&s);s.linked=false;CHECK(S::OnReliable(p)&&plays==4);
    s.linked=true;p={S::kPlantGather,12,34,81};CHECK(S::OnReliable(p)&&plays==5);
    p={S::kFaucet,0,0,0};CHECK(S::OnReliable(p)&&plays==6);
    p.x=1;CHECK(S::OnReliable(p)&&plays==6);
    puts("PASS world sounds: native black-hole phases, alien position, per-peer FIFO/retry, no duplicate accepted sends, readiness, missing assets, malformed cues, expiry/travel/teardown");
}
