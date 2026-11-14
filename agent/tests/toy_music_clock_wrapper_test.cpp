#include "ue_wrap/core/log.h"
#define DINGUS_WRAPPER_FIXTURE_ONLY
#include "dingus_music_wrapper_test.cpp"
int main(){using namespace test;namespace M=ue_wrap::dingus_music;
    const auto reset=[] {++world;for(int i=0;i<Count;++i){objects[i]={};objects[i].index=i;objects[i].serial=int(world*1000)+i;}
        objects[Actor].component=At(Audio);objects[Audio].sound=At(Sound);objects[OtherAudio].sound=At(Sound);objects[Sound].flags|=2;objects[OtherSound].flags|=2;
        plays=stops=0;fields=clockFields=result=parameters=allowSeek=true;callback={};after.clear();missing.clear();lastStart=0;};
    reset();M::Clock clock{};CHECK(M::ReadClock(At(Actor),clock) && clock.duration==69.1716f && clock.sound.Is(At(Sound)));
    M::State state{true,true,.1f,1.f,80.f,5};M::Playback playback{};
    CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==1 && playback.episode==5 && std::fabs(lastStart-std::fmod(80.f,69.1716f))<.001f);
    state.elapsed=81;CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==1);
    objects[Audio].playing=false;CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==2);
    state.episode=6;CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==3);
    objects[OtherSound].duration=30;objects[Audio].sound=At(OtherSound);CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==4 && lastStart==21);
    objects[Actor].component=At(OtherAudio);CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==5);
    state.pitch=1.5f;CHECK(M::ApplyPlayback(At(Actor),state,playback) && objects[OtherAudio].pitch==1.5f && plays==5);
    reset();clockFields=false;playback={};CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==1 && !playback.episode && lastStart==0);
    clockFields=true;CHECK(M::ApplyPlayback(At(Actor),state,playback) && plays==2 && playback.episode==6);
    reset();objects[Sound].flags&=uint8_t(~2);CHECK(!M::ReadClock(At(Actor),clock));playback={};CHECK(M::ApplyPlayback(At(Actor),state,playback) && lastStart==0 && !playback.episode);
    reset();objects[Audio].sound=At(Other);CHECK(!M::ReadClock(At(Actor),clock));
    for(int cause=0;cause<7;++cause){reset();playback={};bool current=true;after=L"Play";callback=[cause,&current]{
        if(cause==0)++world;if(cause==1)++objects[Actor].serial;if(cause==2)++objects[Audio].serial;if(cause==3)++objects[Sound].serial;
        if(cause==4)current=false;if(cause==5)objects[Audio].sound=At(OtherSound);if(cause==6)objects[Actor].component=At(OtherAudio);};
        CHECK(!M::ApplyPlayback(At(Actor),state,playback,[&]{return current;}) && !playback.episode);}
    reset();playback={};auto bad=state;bad.elapsed=NAN;CHECK(!M::ApplyPlayback(At(Actor),bad,playback) && !plays);
    reset();objects[Sound].duration=INFINITY;CHECK(!M::ReadClock(At(Actor),clock));
    reset();bool sourceCurrent=true;const int heard=chirps;after=L"SetPitchMultiplier";callback=[&]{sourceCurrent=false;};
    CHECK(!M::Squeak(At(Actor),1.7f,[&]{return sourceCurrent;}) && chirps==heard);
    sourceCurrent=true;after=L"Activate";callback=[&]{sourceCurrent=false;};
    CHECK(M::Squeak(At(Actor),1.7f,[&]{return sourceCurrent;}) && chirps==heard+1);
    puts("PASS toy clock wrapper/actual ParamFrame: looping-wave duration, native seek/modulo/rate, no heartbeat restart, stopped/replaced component/sound recovery, optional metadata and native lifetime fences");return 0;
}
