param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/seat-presentation-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
 $wrapper=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/ue_wrap/actors/sit_pose.cpp')).Replace("`r`n","`n")
 $controller=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/coop/player/sit_pose.cpp')).Replace("`r`n","`n")
 $view=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/coop/player/remote_player.cpp')).Replace("`r`n","`n")
 $engine=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/ue_wrap/engine/engine.cpp')).Replace("`r`n","`n")
 $yaw=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/include/coop/player/puppet_body_yaw.h')).Replace("`r`n","`n")
 function Extract($source,$names,$path){
  $bodies=foreach($name in $names){$body=[regex]::Match($source,"(?ms)^(?:bool|void) ${name}\(.*?^\}");if(-not $body.Success){throw "Missing production function: $name"};$body.Value}
  [IO.File]::WriteAllText($path,($bodies -join "`r`n"))
 }
 $remoteHeader=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/include/coop/player/remote_player.h'))
 $predicate=[regex]::Match($remoteHeader,'(?ms)^    bool IsSitting\(\) const \{.*?^    \}')
 if(-not $predicate.Success){throw 'Current IsSitting predicate missing'}
 [IO.File]::WriteAllText((Join-Path $output 'chair_sitting.inc'),$predicate.Value)
 function Prepare-Extracted($viewSource,$engineSource){
  Extract $viewSource @('RemotePlayer::RefreshSeatView','RemotePlayer::RefreshView') (Join-Path $output 'seat_view.inc')
  Extract $engineSource @('SetComponentWorldRotation') (Join-Path $output 'seat_rotation.inc')
 }
 function Compile-Seat($fixture,$exe,$override=''){
  $extra=@();if($override){$extra+="/FI$override"}
  if($fixture -eq 'controller'){$extra+=@('/DTEST_STATE_MAX_PEERS=32','/I','agent/tests/chair_stubs','/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @extra /I src/votv-coop/include "/I$output" "/Fo$exe.obj" "/Fe$exe" "agent/tests/seat_${fixture}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE){throw "Seat fixture compile failed: $fixture"}
 }
 Prepare-Extracted $view $engine
 foreach($fixture in @('controller','native')){
  $exe=Join-Path $output ($fixture+'.exe');Compile-Seat $fixture $exe
  & $exe
  if($LASTEXITCODE){throw "Seat regression failed: $fixture"}
 }
 foreach($file in @('coop/player/sit_pose','coop/player/remote_player','ue_wrap/actors/sit_pose','ue_wrap/engine/engine')){
  $leaf=$file.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE){throw "Seat production compile failed: $file"}
 }
 if($FaultControls){
  $controls=@(
   @{Id=1;Kind='controller';Before='result.outcome==coop::element::IntentOutcome::NoBody||';After='false||';Source='controller'},
   @{Id=2;Kind='controller';Before='if(dx*dx+dy*dy+dz*dz>450.0*450.0)';After='if(false)';Source='controller'},
   @{Id=3;Kind='controller';Before='ue_wrap::world_identity::WorldOf(chair)==scope.world&&';After='';Source='controller'},
   @{Id=4;Kind='controller';Before='if(!E::ReadMainPlayerSittingOn(local,confirmed)||confirmed!=localSeat||confirmed==chair||!localCurrent()||!valid())';After='if(false)';Source='controller'},
   @{Id=5;Kind='controller';Before='for(auto& ref:displayed){if(!current())return;if(void* body=ref.Get())ue_wrap::sit_pose::ClearMeshShift(body);}';After=';';Source='controller'},
   @{Id=6;Kind='native';Before='void* base=object_index::ClassByName(P::name::AnimBPKerfurRegularClass);';After='static void* base=object_index::ClassByName(P::name::AnimBPKerfurRegularClass);';Source='wrapper'},
   @{Id=7;Kind='native';Before='write(f.sit,f.sitMask,sitting);';After='if(f.sit>=0)*reinterpret_cast<bool*>(static_cast<uint8_t*>(anim)+f.sit)=sitting;';Source='wrapper'},
   @{Id=8;Kind='native';Before='write(f.ik,f.ikMask,!sitting);';After='if(f.ik>=0)*reinterpret_cast<bool*>(static_cast<uint8_t*>(anim)+f.ik)=!sitting;';Source='wrapper'},
   @{Id=9;Kind='native';Before='!f.Set<FVector>(L"NewLocation",loc)';After='false';Source='wrapper'},
   @{Id=10;Kind='native';Before='std::isfinite(half)&&half>10.f&&half<=1000.f';After='half>10.f';Source='wrapper'},
   @{Id=11;Kind='native';Before='if(!Finite(base)||!BindingCurrent(next))return false;';After=';';Source='wrapper'},
   @{Id=12;Kind='native';Before='return Finite(out)&&BindingCurrent(slot);';After='out=world;return Finite(out)&&BindingCurrent(slot);';Source='wrapper'},
   @{Id=13;Kind='native';Before='if(BindingCurrent(old))slot=old;';After=';';Source='wrapper'},
   @{Id=14;Kind='native';Before='PawnShift g_shift[32];';After='PawnShift g_shift[16];';Source='wrapper'},
   @{Id=15;Kind='native';Before='yaw_=next;turning_=false;lastMs_=nowMs;';After='turning_=false;lastMs_=nowMs;';Source='yaw'},
   @{Id=16;Kind='native';Before='!TryGetHeadPosition(head)';After='false';Source='view'},
   @{Id=17;Kind='native';Before='E::SetComponentWorldRotation(component,aim);';After='E::SetComponentWorldRotation(component,ue_wrap::FRotator{aim.Pitch,aim.Yaw-33.f,0});';Source='view'},
   @{Id=18;Kind='native';Before='!f.Set<FRotator>(L"NewRotation",rotation)';After='false';Source='engine'},
   @{Id=19;Kind='controller';Before='body.Get()&&rig.Get()&&cls.Get()';After='body.Get()&&cls.Get()';Source='controller'},
   @{Id=20;Kind='controller';Before='ue_wrap::sit_pose::AtvDriverSeat(atv)==component';After='true';Source='controller'},
   @{Id=21;Kind='controller';Before='if(!Finite(loc)||!Finite(rot)||!ready())continue;';After='if(!ready())continue;';Source='controller'},
   @{Id=22;Kind='controller';Before='!E::TryGetActorRotation(atv,rot)';After='false';Source='controller'},
   @{Id=23;Kind='controller';Before='if(!current()||!Finite(fwd)||length<.01||length>4.0)continue;';After='if(!current())continue;';Source='controller'},
   @{Id=24;Kind='controller';Before='!E::SetActorLocation(actor,loc)';After='(E::SetActorLocation(actor,loc),false)';Source='controller'},
   @{Id=25;Kind='controller';Before='if(coop::atv_sync::ActorSeatedBy(slot)){if(g_anchor[slot].on)ClearAnchor(slot);continue;}';After='if(false){continue;}';Source='controller'}
  )
  foreach($control in $controls){
   $original=Get-Variable -Name $control.Source -ValueOnly
   if(-not $original.Contains($control.Before)){throw "Missing seat control anchor: $($control.Id)"}
   $mutant=$original.Replace($control.Before,$control.After)
   Prepare-Extracted $(if($control.Source -eq 'view'){$mutant}else{$view}) $(if($control.Source -eq 'engine'){$mutant}else{$engine})
   $override=''
   if($control.Source -in @('wrapper','controller','yaw')){
    $path=Join-Path $output ('fault-'+$control.Id+'.cpp');[IO.File]::WriteAllText($path,$mutant)
    $macro=switch($control.Source){'wrapper'{'SEAT_WRAPPER_SOURCE'}'controller'{'CHAIR_SYNC_SOURCE'}'yaw'{'SEAT_BODY_YAW_HEADER'}}
    $override=Join-Path $output ('fault-'+$control.Id+'.h');[IO.File]::WriteAllText($override,('#define '+$macro+' "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   }
   $exe=Join-Path $output ('fault-'+$control.Id+'.exe');Compile-Seat $control.Kind $exe $override
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Id 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ('fault-'+$control.Id+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Seat control did not reach assertion: $($control.Id)"}
   Write-Output ('PASS seat fault '+$control.Id+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
  Prepare-Extracted $view $engine
 }
}finally{Pop-Location}
