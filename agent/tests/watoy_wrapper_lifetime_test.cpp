#define WATOY_WRAPPER_FIXTURE_ONLY
#include "watoy_wrapper_test.cpp"
#include <algorithm>
int main(int argc,char** argv){
 using namespace test;namespace W=ue_wrap::watoy;
 const std::string selected=argc>1?argv[1]:"";
 const auto run=[&](const char* name){return selected.empty() || selected==name;};
 const auto reset=[] {
  ++world;for(int i=0;i<Count;++i){objects[i]={};objects[i].index=i;classes[i]=nullptr;}
  objects[Actor].audio=At(Audio);objects[Actor].zero=At(Zero);objects[Zero].owner=At(Actor);
  fields=queryOk=spawnOk=valid=true;playing=false;starts=stops=spawns=destroys=activations=cues=0;
  callback={};metaCallback={};afterName=metaName=missingField=badOutputFunction=badOutputName=poisonOutput=callFail=L"";
  calls.clear();meshOverride=spawnResult=nullptr;
  schemaFunction.clear();schemaParameter.clear();schemaFault=0;dispatchOverrides.clear();
 };
 const W::State desired{true,.6f,2,true};
 const auto current=[] {return valid;};
 if(run("activation-admission")){
  reset();bool dispatched=true;schemaFunction=L"activate";schemaFault=4;
  CHECK(!W::Activate(At(Actor),current,&dispatched) && !dispatched && !activations);
  reset();callFail=L"activate";CHECK(!W::Activate(At(Actor),current,&dispatched) && dispatched);
  reset();afterName=L"activate";callback=[](const std::wstring&){valid=false;};
  CHECK(!W::Activate(At(Actor),current,&dispatched) && dispatched && activations==1);
  reset();CHECK(W::Activate(At(Actor),current,&dispatched) && dispatched && activations==1);
  puts("PASS watoy native admission reports refusal versus dispatched uncertain completion");
 }
 if(run("native-schema")){
  for(const auto& [verb,param]:std::vector<std::pair<std::wstring,std::wstring>>{
      {L"IsPlaying",L"ReturnValue"},{L"SetVolumeMultiplier",L"NewVolumeMultiplier"},
      {L"SetPitchMultiplier",L"NewPitchMultiplier"},{L"Play",L"StartTime"},
      {L"K2_GetComponentToWorld",L"ReturnValue"},{L"GetComponentBounds",L"Origin"},
      {L"InverseTransformLocation",L"Location"},{L"SpawnEmitterAttached",L"ReturnValue"}}){
   for(int fault:{1,2,4,5,6}){
    reset();schemaFunction=verb;schemaParameter=param;schemaFault=fault;W::View view;
    CHECK(!W::Apply(view,At(Actor),desired,current));
    CHECK(std::find(calls.begin(),calls.end(),verb)==calls.end() && !spawns);
   }
  }
  reset();schemaFunction=L"GetComponentBounds";schemaParameter=L"Origin";schemaFault=3;W::View view;
  CHECK(!W::Apply(view,At(Actor),desired,current) && !spawns);
  for(const auto* verb:{L"activate",L"Stop"}){
   reset();schemaFunction=verb;schemaFault=4;
   if(std::wstring(verb)==L"activate")CHECK(!W::Activate(At(Actor),current) && !activations);
   else {playing=true;CHECK(!W::ApplyAudio(At(Actor),{},current) && !stops);}
  }
  reset();CHECK(W::Activate(At(Actor),current) && activations==1); // BP locals do not imply inputs
  puts("PASS watoy native schemas: size/span/overlap/missing/duplicate/extra parameters and parameterless BP locals");
 }
 if(run("native-function")){
  reset();bool changed=false;
  CHECK(!W::Activate(At(Actor),[&]{if(!changed){changed=true;++static_cast<Obj*>(Fn(L"activate"))->serial;}return true;}) && !activations);
  reset();changed=false;
  CHECK(!W::Activate(At(Actor),[&]{if(!changed){changed=true;dispatchOverrides[L"activate"]=Fn(L"Stop");}return true;}) && !activations);
  reset();afterName=L"SetVolumeMultiplier";callback=[](const std::wstring&){++static_cast<Obj*>(Fn(L"SetVolumeMultiplier"))->serial;};
  CHECK(!W::ApplyAudio(At(Actor),desired,current) && !starts && objects[Audio].pitch==1);
  reset();W::View view;afterName=L"SpawnEmitterAttached";
  callback=[](const std::wstring&){++static_cast<Obj*>(Fn(L"SpawnEmitterAttached"))->serial;};
  CHECK(!W::Apply(view,At(Actor),desired,current) && spawns==1 && destroys==1 && !view.Effect().Get());
  puts("PASS watoy exact native function: serial retirement, dispatch replacement, stale completion and owned spawn cleanup");
 }
 if(run("field-class")){
  reset();W::State output{false,.9f,8,false};metaName=L"field:VolumeMultiplier";
  metaCallback=[]{classes[Audio]=At(Other);};CHECK(!W::Read(At(Actor),output) && output.volume==.9f && output.pitch==8);
  reset();metaName=L"bool:dir";metaCallback=[]{++objects[ZeroClass].serial;};CHECK(!W::Read(At(Actor),output));
  puts("PASS watoy wrapper field-class");
 }
 if(run("unknown-zero")){
  reset();missingField=L"a";W::State output{false,.9f,8,false};CHECK(!W::Read(At(Actor),output) && output.volume==.9f);
  missingField.clear();objects[Actor].zero=nullptr;CHECK(W::Read(At(Actor),output) && !output.active && !output.effect);
  puts("PASS watoy wrapper unknown-zero");
 }
 if(run("read-bindings")){
  for(int cause=0;cause<3;++cause){reset();W::State output{false,.9f,8,false};metaName=L"field:VolumeMultiplier";
   metaCallback=[cause]{if(cause==0)objects[Actor].audio=At(Other);if(cause==1)objects[Actor].zero=nullptr;if(cause==2)++objects[ToyClass].serial;};
   CHECK(!W::Read(At(Actor),output) && output.volume==.9f && output.pitch==8);
  }
  puts("PASS watoy wrapper read-bindings");
 }
 if(run("activate-current")){
  reset();metaName=L"resolve:activate";metaCallback=[]{valid=false;};CHECK(!W::Activate(At(Actor),current) && !activations);
  reset();CHECK(W::Activate(At(Actor),current) && activations==1);
  puts("PASS watoy wrapper activate-current");
 }
 if(run("audio-current")){
  reset();callback=[](const std::wstring&){valid=false;};CHECK(!W::ApplyAudio(At(Actor),desired,current) && !starts && objects[Audio].volume==.1f);
  for(int cause=0;cause<3;++cause){reset();callback=[cause](const std::wstring&){
   if(cause==0)objects[Actor].audio=At(Other);if(cause==1)++objects[Audio].serial;if(cause==2)classes[Audio]=At(Other);
  };CHECK(!W::ApplyAudio(At(Actor),desired,current) && !starts && objects[Audio].volume==.1f);}
  reset();W::View view;callback=[](const std::wstring&){valid=false;};CHECK(!W::Apply(view,At(Actor),desired,current) && !starts);
  puts("PASS watoy wrapper audio-current");
 }
 if(run("spawn-services")){
  for(auto target:{Library,System,Math}){reset();W::View view;afterName=L"K2_GetComponentToWorld";
   callback=[target](const std::wstring&){++objects[target].serial;};CHECK(!W::Apply(view,At(Actor),desired,current) && !spawns);
   CHECK(std::find(calls.begin(),calls.end(),L"GetComponentBounds")==calls.end());
  }
  puts("PASS watoy wrapper spawn-services");
 }
 if(run("spawn-binding")){
  for(int cause=0;cause<2;++cause){reset();W::View view;afterName=L"K2_GetComponentToWorld";
   callback=[cause](const std::wstring&){if(cause==0)meshOverride=At(Other);else ++objects[Template].serial;};
   CHECK(!W::Apply(view,At(Actor),desired,current) && !spawns);
   CHECK(std::find(calls.begin(),calls.end(),L"GetComponentBounds")==calls.end());
  }
  puts("PASS watoy wrapper spawn-binding");
 }
 if(run("spawn-output")){
  for(const auto* name:{L"K2_GetComponentToWorld",L"GetComponentBounds",L"InverseTransformLocation"}){
   reset();W::View view;badOutputFunction=name;badOutputName=std::wstring(name)==L"GetComponentBounds"?L"Origin":L"ReturnValue";
   CHECK(!W::Apply(view,At(Actor),desired,current) && !spawns);
   reset();W::View poisoned;poisonOutput=name;CHECK(!W::Apply(poisoned,At(Actor),desired,current) && !spawns);
  }
  puts("PASS watoy wrapper spawn-output");
 }
 if(run("spawn-candidate")){
  reset();W::View view;spawnResult=At(Other);CHECK(!W::Apply(view,At(Actor),desired,current) && !view.Effect().Get() && !destroys);
  reset();W::View stale;afterName=L"SpawnEmitterAttached";callback=[](const std::wstring&){valid=false;};
  CHECK(!W::Apply(stale,At(Actor),desired,current) && !stale.Effect().Get() && spawns==1 && destroys==1);
  puts("PASS watoy wrapper spawn-candidate");
 }
 if(run("view-reentry")){
  reset();W::View view;objects[Zero].effect=At(NativeEffect);
  callback=[&](const std::wstring&){W::State fresh{true,.9f,4,true};CHECK(W::Apply(view,At(Actor),fresh,current));};
  CHECK(!W::Apply(view,At(Actor),desired,current) && view.Effect().Is(At(NativeEffect)) && objects[Audio].volume==.9f && objects[Audio].pitch==4);
  callback=[&](const std::wstring&){W::State fresh{true,.8f,3,true};CHECK(W::Apply(view,At(Actor),fresh,current));};
  W::Clear(view);CHECK(view.Effect().Is(At(NativeEffect)) && objects[NativeEffect].active && objects[Audio].volume==.8f);
  reset();W::View spawning;afterName=L"SpawnEmitterAttached";
  callback=[&](const std::wstring&){objects[Zero].effect=At(NativeEffect);CHECK(W::Apply(spawning,At(Actor),desired,current));};
  CHECK(!W::Apply(spawning,At(Actor),desired,current) && spawning.Effect().Is(At(NativeEffect)) && destroys==1 && objects[NativeEffect].active);
  puts("PASS watoy wrapper view-reentry");
 }
 if(run("view-copies")){
  reset();W::View view;CHECK(W::Apply(view,At(Actor),desired,current));
  auto stale=view;auto updated=view;W::State newer{true,.9f,4,true};
  CHECK(W::Apply(updated,At(Actor),newer,current));
  const auto before=calls.size();W::Clear(stale);W::Clear(view);
  CHECK(calls.size()==before && playing && !destroys && objects[Audio].volume==.9f);
  auto duplicate=updated;W::Clear(updated);CHECK(destroys==1 && !playing);
  const auto released=calls.size();W::Clear(duplicate);CHECK(destroys==1 && calls.size()==released);
  reset();objects[Zero].effect=At(NativeEffect);W::View clearing;
  CHECK(W::Apply(clearing,At(Actor),desired,current));auto independent=clearing;
  callback=[&](const std::wstring&){CHECK(W::Apply(independent,At(Actor),newer,current));};
  W::Clear(clearing);
  CHECK(playing && objects[NativeEffect].active && objects[Audio].volume==.9f && !destroys);
  W::Clear(independent);CHECK(!playing && !objects[NativeEffect].active);
  reset();W::View first;CHECK(W::Apply(first,At(Actor),desired,current));W::View fresh;
  CHECK(W::Apply(fresh,At(Actor),newer,current) && spawns==1);const auto claimed=calls.size();
  W::Clear(first);CHECK(calls.size()==claimed && playing && !destroys);
  ++world;const auto departed=calls.size();W::Clear(fresh);CHECK(calls.size()==departed && !destroys);
  puts("PASS watoy independent copied views: stale retirement, single release and reentrant restart");
 }
 return 0;
}
