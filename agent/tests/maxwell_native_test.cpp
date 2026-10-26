#define DINGUS_WRAPPER_FIXTURE_ONLY
#include "dingus_music_wrapper_test.cpp"
#include <limits>
namespace test {
namespace M=ue_wrap::dingus_music;
void Reset(){
    ++world;for(int i=0;i<Count;++i){objects[i]={};objects[i].index=i;objects[i].serial=int(world*1000)+i;}
    objects[Actor].component=At(Audio);objects[Variant].component=At(OtherAudio);objects[Plush].component=At(Audio);
    objects[Audio].sound=At(Sound);objects[OtherAudio].sound=At(OtherSound);
    objects[Sound].flags|=2;objects[OtherSound].flags|=2;
    fields=result=parameters=clockFields=worldReady=allowSeek=true;worldPointer=nullptr;
    plays=stops=chirps=touches=toggles=walks=0;lastStart=0;usedMask=4;usedOffset=offsetof(Obj,flags);
    missing=after=fieldAfter=frameAfter=badParameter=malformedEmpty=L"";badBytes=-1;
    callback=fieldCallback=frameCallback=visitCallback={};instances={At(Actor),At(Variant)};
    calls.clear();functions.clear();names.clear();
}
M::State Desired(){return {true,true,.1f,1.f,80.f,5};}
void RetireFrame(const wchar_t* verb){frameAfter=verb;frameCallback=[verb]{++functions.at(verb)->serial;};}
void Buffer(){Reset();ue_wrap::CachedObjRef out[2];out[1].Set(At(Other));
    CHECK(!M::Collect(nullptr,5)&&!walks);CHECK(!M::Collect(out,0)&&!walks);
    CHECK(M::Collect(out,1)==1&&out[0].Is(At(Actor))&&out[1].Is(At(Other)));}
void WorldCollection(){for(int cause=0;cause<3;++cause){Reset();ue_wrap::CachedObjRef out[4];visitCallback=[cause]{
    if(cause==0)++world;if(cause==1)worldPointer=reinterpret_cast<void*>(999);if(cause==2)worldReady=false;};
    CHECK(!M::Collect(out,4)&&!out[0].Raw()&&walks==1);}
    Reset();objects[Variant].worldObject=reinterpret_cast<void*>(999);ue_wrap::CachedObjRef out[4];CHECK(M::Collect(out,4)==1);}
void Dedup(){Reset();instances={At(Actor),At(Actor),At(Variant),At(Variant)};ue_wrap::CachedObjRef out[4];
    CHECK(M::Collect(out,4)==2&&out[0].Is(At(Actor))&&out[1].Is(At(Variant)));}
void ActorClass(){for(bool reuse:{false,true}){Reset();M::State out{};out.volume=9;after=L"IsPlaying";callback=[reuse]{
    if(reuse)++objects[Class].serial;else objects[Actor].cls=At(ForeignClass);};
    CHECK(!M::Read(At(Actor),out)&&out.volume==9);}Reset();CHECK(M::IsDingus(At(Variant))&&!M::IsDingus(At(Other)));}
void AudioType(){for(int cause=0;cause<3;++cause){Reset();if(cause==0)objects[Audio].cls=At(ForeignClass);
    if(cause==1)objects[AudioClass].live=false;if(cause==2)objects[Audio].live=false;M::State out{};
    CHECK(!M::Read(At(Actor),out)&&!calls[L"IsPlaying"]);}Reset();CHECK(M::Apply(At(Variant),{true,true,1}));}
void ComponentBinding(){Reset();M::State out{};out.volume=9;after=L"IsPlaying";callback=[]{objects[Actor].component=At(OtherAudio);};
    CHECK(!M::Read(At(Actor),out)&&out.volume==9);
    Reset();fieldAfter=L"ws1";fieldCallback=[]{++objects[Actor].serial;};CHECK(!M::Read(At(Actor),out)&&!calls[L"IsPlaying"]);}
void UsedField(){for(bool offset:{false,true}){Reset();M::State out{};after=L"IsPlaying";callback=[offset]{
    if(offset)usedOffset=offsetof(Obj,volume);else usedMask=8;};CHECK(!M::Read(At(Actor),out));}
    Reset();M::State s{false,true,1};CHECK(M::Apply(At(Actor),s)&&objects[Actor].flags==0xA4);
    s.used=false;CHECK(M::Apply(At(Actor),s)&&objects[Actor].flags==0xA0);}
void CoherentState(){for(int field=0;field<3;++field){Reset();M::State out{};out.volume=9;after=L"IsPlaying";callback=[field]{
    if(field==0)objects[Audio].volume=.4f;if(field==1)objects[Audio].pitch=2;if(field==2)objects[Actor].flags^=4;};
    CHECK(!M::Read(At(Actor),out)&&out.volume==9);CHECK(M::Read(At(Actor),out)&&out.volume==objects[Audio].volume);}}
void BadPitch(){for(float pitch:{NAN,INFINITY,0.f,-1.f,10.1f}){Reset();objects[Audio].pitch=pitch;M::State out{};out.pitch=3;
    CHECK(!M::Read(At(Actor),out)&&out.pitch==3&&!calls[L"IsPlaying"]);}}
void SqueakRead(){Reset();float out=3;objects[Audio].pitch=NAN;CHECK(!M::ReadSqueakPitch(At(Actor),out)&&out==3);
    Reset();fieldAfter=L"PitchMultiplier";fieldCallback=[]{++objects[AudioClass].serial;};CHECK(!M::ReadSqueakPitch(At(Actor),out)&&out==3);
    Reset();CHECK(M::ReadSqueakPitch(At(Actor),out)&&out==1);}
void ClockSource(){Reset();M::Clock out{};out.duration=99;int count=0;
    CHECK(!M::ReadClock(At(Actor),out,[&]{if(++count==2)objects[Audio].sound=At(OtherSound);return true;})&&out.duration==99);}
void SoundIdentity(){for(int cause=0;cause<3;++cause){Reset();M::Clock out{};out.duration=99;fieldAfter=L"Duration";fieldCallback=[cause]{
    if(cause==0)++objects[Sound].serial;if(cause==1)++objects[WaveClass].serial;if(cause==2)objects[Sound].cls=At(ForeignClass);};
    CHECK(!M::ReadClock(At(Actor),out)&&out.duration==99);}Reset();M::Clock out{};objects[Sound].cls=At(ForeignClass);CHECK(!M::ReadClock(At(Actor),out));}
void ClockValues(){for(int field=0;field<3;++field){Reset();M::Clock out{};out.duration=99;int count=0;
    CHECK(!M::ReadClock(At(Actor),out,[&]{if(++count==3){if(field==0)objects[Sound].duration=30;
        if(field==1)objects[Audio].pitch=2;if(field==2)objects[Sound].flags&=uint8_t(~2);}return true;})&&out.duration==99);}
    for(float duration:{NAN,INFINITY,0.f,86401.f}){Reset();objects[Sound].duration=duration;M::Clock out{};CHECK(!M::ReadClock(At(Actor),out));}}
void QueryFunction(){for(int cause=0;cause<4;++cause){Reset();M::State out{};out.volume=9;
    if(cause==0)RetireFrame(L"IsPlaying");if(cause==1){badParameter=L"ReturnValue";badBytes=0;}
    if(cause==2){badParameter=L"ReturnValue";badBytes=4;}if(cause==3){frameAfter=L"IsPlaying";frameCallback=[]{functions[L"IsPlaying"]=&objects[50];names[&objects[50]]=L"IsPlaying";};}
    CHECK(!M::Read(At(Actor),out)&&out.volume==9&&!calls[L"IsPlaying"]);}
    Reset();after=L"IsPlaying";callback=[]{++functions.at(L"IsPlaying")->serial;};M::State out{};CHECK(!M::Read(At(Actor),out));}
void OwnedInput(){Reset();auto s=Desired();M::Playback playback;after=L"SetVolumeMultiplier";
    callback=[&]{s.playing=false;s.pitch=NAN;s.elapsed=NAN;s.episode=999;};
    CHECK(M::ApplyPlayback(At(Actor),s,playback)&&playback.episode==5&&objects[Audio].playing&&objects[Audio].pitch==1&&
        std::fabs(lastStart-std::fmod(80.f,objects[Sound].duration))<.001f);}
void Shape(){for(int cause=0;cause<7;++cause)for(bool position:{false,true}){Reset();auto s=Desired();M::Playback playback;
    if(cause==0)s.pitch=NAN;if(cause==1)s.pitch=0;if(cause==2)s.elapsed=INFINITY;if(cause==3)s.elapsed=-1;
    if(cause==4)s.playing=false;if(cause==5)s.episode=0;if(cause==6)s.volume=11;
    CHECK(!(position?M::ApplyPlayback(At(Actor),s,playback):M::Apply(At(Actor),s))&&objects[Actor].flags==0xA0&&calls.empty());}}
void VolumeFunction(){for(bool size:{false,true}){Reset();auto s=Desired();M::Playback playback;
    if(size){badParameter=L"NewVolumeMultiplier";badBytes=8;}else RetireFrame(L"SetVolumeMultiplier");
    CHECK(!M::ApplyPlayback(At(Actor),s,playback)&&!calls[L"SetVolumeMultiplier"]&&!plays);}
    Reset();auto s=Desired();after=L"SetVolumeMultiplier";callback=[]{++objects[AudioClass].serial;};CHECK(!M::Apply(At(Actor),s)&&!plays);}
void PitchFunction(){for(bool size:{false,true}){Reset();auto s=Desired();s.pitch=2;M::Playback playback;
    if(size){badParameter=L"NewPitchMultiplier";badBytes=8;}else RetireFrame(L"SetPitchMultiplier");
    CHECK(!M::ApplyPlayback(At(Actor),s,playback)&&!calls[L"SetPitchMultiplier"]&&!plays);}}
void PlayFunction(){for(int cause=0;cause<3;++cause){Reset();auto s=Desired();M::Playback playback;
    if(cause==0)RetireFrame(L"Play");if(cause==1){badParameter=L"StartTime";badBytes=0;}if(cause==2){badParameter=L"StartTime";badBytes=8;}
    CHECK(!M::ApplyPlayback(At(Actor),s,playback)&&!plays&&!playback.episode);}
    Reset();auto s=Desired();M::Playback playback;CHECK(M::ApplyPlayback(At(Actor),s,playback)&&plays==1);
    s.elapsed=90;CHECK(M::ApplyPlayback(At(Actor),s,playback)&&plays==1);}
void SeekClock(){for(bool post:{false,true})for(bool pitch:{false,true}){Reset();auto s=Desired();M::Playback playback;
    const auto change=[pitch]{if(pitch)objects[Audio].pitch=2;else objects[Sound].duration=25;};
    if(post){after=L"Play";callback=change;}else{frameAfter=L"Play";frameCallback=change;}
    CHECK(!M::ApplyPlayback(At(Actor),s,playback)&&!playback.episode&&plays==int(post));
    CHECK(M::ApplyPlayback(At(Actor),s,playback)&&playback.episode==5);}}
void StopFunction(){Reset();objects[Audio].playing=true;RetireFrame(L"Stop");CHECK(!M::StopMusic(At(Actor))&&!stops);
    Reset();objects[Audio].playing=true;frameAfter=L"Stop";frameCallback=[]{worldPointer=reinterpret_cast<void*>(999);};CHECK(!M::StopMusic(At(Actor))&&!stops);
    Reset();objects[Audio].playing=true;CHECK(M::StopMusic(At(Actor))&&stops==1&&!objects[Audio].playing);}
void TouchFunction(){Reset();RetireFrame(L"touch");CHECK(!M::Interact(At(Actor),true)&&!touches&&!toggles);
    Reset();after=L"touch";callback=[]{++objects[Class].serial;};CHECK(!M::Interact(At(Actor),true)&&touches==1&&!toggles);
    Reset();missing=L"ToggleActive";CHECK(M::Interact(At(Actor),false)&&touches==1&&!toggles);}
void ToggleFunction(){for(bool replace:{false,true}){Reset();after=L"touch";callback=[replace]{
    if(replace){functions[L"ToggleActive"]=&objects[50];names[&objects[50]]=L"ToggleActive";}else ++functions.at(L"ToggleActive")->serial;};
    CHECK(!M::Interact(At(Actor),true)&&touches==1&&!toggles);}
    Reset();RetireFrame(L"ToggleActive");CHECK(!M::Interact(At(Actor),true)&&!touches&&!toggles);
    Reset();malformedEmpty=L"ToggleActive";CHECK(!M::Interact(At(Actor),true)&&!touches&&!toggles);}
void SqueakFunctions(){for(int cause=0;cause<3;++cause){Reset();if(cause==0)RetireFrame(L"SetPitchMultiplier");if(cause==1)RetireFrame(L"Activate");
    if(cause==2){badParameter=L"bReset";badBytes=4;}CHECK(!M::Squeak(At(Actor),1.5f)&&!chirps);}
    Reset();after=L"Activate";callback=[]{++world;};CHECK(M::Squeak(At(Actor),1.5f)&&chirps==1);
    Reset();float out=0;CHECK(!M::IsDingus(At(Plush))&&M::IsSqueakyToy(At(Plush))&&M::ReadSqueakPitch(At(Plush),out)&&out==1);
    CHECK(M::Squeak(At(Plush),1.6f)&&chirps==1&&objects[Audio].pitch==1.6f);}
void NestedPlayback(){Reset();auto s=Desired();M::Playback playback;after=L"Play";callback=[&]{objects[Audio].sound=At(OtherSound);
    auto fresh=s;fresh.episode=9;fresh.elapsed=20;CHECK(M::ApplyPlayback(At(Actor),fresh,playback));};
    CHECK(!M::ApplyPlayback(At(Actor),s,playback)&&playback.episode==9&&playback.sound.Is(At(OtherSound))&&objects[Audio].playing);
    Reset();playback={};after=L"SetVolumeMultiplier";callback=[&]{M::State stopped{false,false,.1f};CHECK(M::ApplyPlayback(At(Actor),stopped,playback));};
    CHECK(!M::ApplyPlayback(At(Actor),s,playback)&&!playback.episode&&!plays&&!objects[Audio].playing);}
}
int main(){using namespace test;void(*groups[])()={Buffer,WorldCollection,Dedup,ActorClass,AudioType,ComponentBinding,UsedField,CoherentState,
    BadPitch,SqueakRead,ClockSource,SoundIdentity,ClockValues,QueryFunction,OwnedInput,Shape,VolumeFunction,PitchFunction,PlayFunction,
    SeekClock,StopFunction,TouchFunction,ToggleFunction,SqueakFunctions,NestedPlayback};
    for(int i=0;i<25;++i){groups[i]();std::printf("PASS Maxwell native %d\n",i+1);}return 0;}
