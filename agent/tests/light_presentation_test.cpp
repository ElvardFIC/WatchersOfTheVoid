// Use the same actual wrapper/ParamFrame and extended reflection/native doubles.
#define main LightingLegacyMain
#include "lightswitch_native_test.cpp"
#undef main
namespace test {
void LampLifetime(){
 for(int mode=0;mode<5;++mode){Reset();onFrame=[mode]{
  if(mode==0)++objects[18].serial;if(mode==1)++world;if(mode==2)objects[18].cls=&objects[2];
  if(mode==3)++objects[19].serial;if(mode==4)lampFunction=&objects[28];
 };CHECK(!L::ApplyLampActive(&objects[18],true) && !calls);}
 Reset();onCall=[]{++world;};CHECK(!L::ApplyLampActive(&objects[18],true) && calls==1);
 Reset();CHECK(L::ApplyLampActive(&objects[18],true) && calls==1 && objects[18].flags==0x88);
}
void AmbientLifetime(){
 for(int mode=0;mode<5;++mode){Reset();onFrame=[mode]{
  if(mode==0)++objects[21].serial;if(mode==1)++world;if(mode==2)objects[21].cls=&objects[2];
  if(mode==3)++objects[24].serial;if(mode==4)ambientFunction=&objects[28];
 };CHECK(!L::ApplyAmbientActive(&objects[21],true) && !calls);}
 Reset();onCall=[]{++world;};CHECK(!L::ApplyAmbientActive(&objects[21],true) && calls==1);
 Reset();CHECK(L::ApplyAmbientActive(&objects[21],true) && calls==1 && objects[23].flags==0xA0);
}
void AmbientBinding(){
 Reset();onFrame=[]{objects[21].bottom=&objects[31];};CHECK(!L::ApplyAmbientActive(&objects[21],true) && !calls);
 Reset();onCall=[]{objects[21].bottom=&objects[31];};CHECK(!L::ApplyAmbientActive(&objects[21],true) && calls==1);
 Reset();onBool=[]{objects[21].bottom=&objects[31];};bool value=true;
 CHECK(!L::TryReadAmbientActive(&objects[21],value) && value);
 Reset();onBool=[]{++objects[23].serial;};value=true;CHECK(!L::TryReadAmbientActive(&objects[21],value) && value);
}
void SoundMetadata(){
 Reset();emitReplay=true;CHECK(L::CallUse(&objects[9]) && plays==1 && installs==1);
 ++objects[6].serial;CHECK(L::CallUse(&objects[9]) && plays==2 && installs==2);
 soundFunction=&objects[28];CHECK(L::CallUse(&objects[9]) && plays==3 && installs==3);
 L::NativeScope identity(&objects[9],useFunction,&L::g_switchRevision);L::ReplaySwitchScope scope(&objects[9],identity);
 callbackFunction=&objects[6];CHECK(soundHook(nullptr,&objects[9],reinterpret_cast<const uint8_t*>(&sound))==ue_wrap::script_gate::Verdict::Cancel && plays==3);
 callbackFunction=soundFunction;CHECK(soundHook(nullptr,&objects[9],reinterpret_cast<const uint8_t*>(&sound))==ue_wrap::script_gate::Verdict::Cancel && plays==4);
 Reset();soundLoaded=false;CHECK(!L::CallUse(&objects[9]) && !calls);soundLoaded=true;CHECK(L::CallUse(&objects[9]));
 Reset();hookOk=false;CHECK(!L::CallUse(&objects[9]) && !calls);hookOk=true;CHECK(L::CallUse(&objects[9]));
}
void SoundBounds(){
 for(int mode=0;mode<6;++mode){Reset();
  if(mode==0)soundFrame=-1;if(mode==1)soundOffset=-1;if(mode==2)soundOffset=12;
  if(mode==3)volumeOffset=14;if(mode==4)pitchOffset=16;if(mode==5)soundFrame=0;
  CHECK(!L::CallUse(&objects[9]) && !calls && !installs);
 }
 Reset();CHECK(L::EnsureSwitchResolved() && L::EnsureReplaySound());
 L::NativeScope identity(&objects[9],useFunction,&L::g_switchRevision);L::ReplaySwitchScope scope(&objects[9],identity);
 callbackFunction=soundFunction;CHECK(soundHook(nullptr,&objects[9],nullptr)==ue_wrap::script_gate::Verdict::Cancel && !plays);
 L::g_pitchOff=16;CHECK(soundHook(nullptr,&objects[9],reinterpret_cast<const uint8_t*>(&sound))==ue_wrap::script_gate::Verdict::Cancel && !plays);
}
void PacketCopy(){
 Reset();emitReplay=true;onLocation=[]{sound.volume=3;sound.pitch=2;sound.cue=&objects[10];};
 CHECK(L::CallUse(&objects[9]) && plays==1 && playedVolume==.75f && playedPitch==1.2f && playedCue==&objects[25]);
}
void Attenuation(){
 Reset();emitReplay=true;CHECK(L::CallUse(&objects[9]) && plays==1 && spawns==1);
 ++objects[26].serial;CHECK(L::CallUse(&objects[9]) && plays==2 && spawns==2);
 objects[26].live=false;attenuationValue=&objects[27];CHECK(L::CallUse(&objects[9]) && plays==3 && spawns==3 && playedAttenuation==&objects[27]);
 objects[27].cls=&objects[29];CHECK(L::CallUse(&objects[9]) && plays==4 && spawns==4);
 Reset();emitReplay=true;attenuationOk=false;CHECK(L::CallUse(&objects[9]) && !plays && spawns==1);
 attenuationOk=true;CHECK(L::CallUse(&objects[9]) && plays==1 && spawns==2);
}
void FiniteAudio(){
 for(int mode=0;mode<8;++mode){Reset();emitReplay=true;
  if(mode==0)sound.volume=std::numeric_limits<float>::quiet_NaN();if(mode==1)sound.volume=-1;
  if(mode==2)sound.pitch=0;if(mode==3)sound.pitch=std::numeric_limits<float>::infinity();
  if(mode==4)position.X=std::numeric_limits<float>::quiet_NaN();if(mode==5)position.Y=std::numeric_limits<float>::infinity();
  if(mode==6)position.Z=std::numeric_limits<float>::quiet_NaN();if(mode==7)locationOk=false;
  CHECK(L::CallUse(&objects[9]) && !plays);
 }
 Reset();emitReplay=true;sound.volume=0;CHECK(L::CallUse(&objects[9]) && plays==1 && playedVolume==0);
}
void SourceLifetime(){
 for(int mode=0;mode<8;++mode){Reset();emitReplay=true;onLocation=[mode]{
  if(mode==0)++objects[9].serial;if(mode==1)objects[9].cls=&objects[2];if(mode==2)++world;
  if(mode==3)++objects[25].serial;if(mode==4)objects[25].cls=&objects[2];if(mode==5)++objects[26].serial;
  if(mode==6)++objects[6].serial;if(mode==7){soundFunction=&objects[28];CHECK(L::EnsureReplaySound());}
 };L::CallUse(&objects[9]);CHECK(!plays && !L::g_replayedSwitch && !L::g_replayIdentity);}
 Reset();CHECK(L::EnsureSwitchResolved() && L::EnsureReplaySound());
 CHECK(soundHook(nullptr,&objects[9],reinterpret_cast<const uint8_t*>(&sound))==ue_wrap::script_gate::Verdict::Run);
 L::NativeScope identity(&objects[9],useFunction,&L::g_switchRevision);L::ReplaySwitchScope scope(&objects[9],identity);callbackFunction=soundFunction;
 CHECK(soundHook(nullptr,&objects[15],reinterpret_cast<const uint8_t*>(&sound))==ue_wrap::script_gate::Verdict::Run && !plays);
}
void Reentry(){
 Reset();emitReplay=true;onSpawn=[]{CHECK(L::g_buildingAttenuation);CHECK(L::EnsureReplaySound());};
 CHECK(L::CallUse(&objects[9]) && plays==1 && spawns==1 && !L::g_buildingAttenuation);
 Reset();emitReplay=true;nestedUse=true;CHECK(L::CallUse(&objects[9]) && calls==2 && plays==2 && !L::g_replayedSwitch && !L::g_replayIdentity);
}
}
int main(int argc,char** argv){
 using namespace test;struct Case{const char* name;void(*run)();};
 const Case cases[]={{"lamp",LampLifetime},{"ambient",AmbientLifetime},{"binding",AmbientBinding},{"metadata",SoundMetadata},{"bounds",SoundBounds},
  {"packet",PacketCopy},{"attenuation",Attenuation},{"finite",FiniteAudio},{"source",SourceLifetime},{"reentry",Reentry}};
 bool ran=false;for(const auto& item:cases)if(argc<2 || std::string(argv[1])==item.name){item.run();std::printf("PASS light presentation %s\n",item.name);ran=true;}CHECK(ran);
}
