param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/chair-sync-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
 $header=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/include/coop/player/remote_player.h'))
 $predicate=[regex]::Match($header,'(?ms)^    bool IsSitting\(\) const \{.*?^    \}')
 if(-not $predicate.Success){throw 'Current IsSitting predicate missing'}
 [IO.File]::WriteAllText((Join-Path $output 'chair_sitting.inc'),$predicate.Value)
 function Compile-Chair($exe,$override=''){
  $extra=@();if($override){$extra+="/FI$override"}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 /I agent/tests/chair_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/I$output" @extra "/Fo$exe.obj" "/Fe$exe" agent/tests/chair_sync_test.cpp /link /OPT:REF
  if($LASTEXITCODE){throw 'Chair fixture compile failed'}
 }
 $exe=Join-Path $output 'chair.exe';Compile-Chair $exe
 & $exe
 if($LASTEXITCODE){throw 'Chair sync regression failed'}
 foreach($file in @('coop/player/sit_pose','coop/player/remote_player','coop/session/net_pump')){
  $leaf=Split-Path -Leaf $file
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE){throw "Chair production compile failed: $file"}
 }
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/coop/player/sit_pose.cpp')).Replace("`r`n","`n")
  $completion=@'
if(!Current(scope)||revision!=g_revision||!player.Get()||!playerClass.Get()||R::ClassOf(local)!=playerClass.Get()||
       coop::players::Registry::Get().Local()!=local||!E::ReadMainPlayerSittingOn(local,currentSeat)||
       (sitting&&(!capturedSeat.Get()||!capturedClass.Get()||R::ClassOf(seat)!=capturedClass.Get()||currentSeat!=seat))||
       (!sitting&&currentSeat&&currentSeat!=seat)||!Current(scope)||revision!=g_revision)return;
'@
  $controls=@(
   @{Name='small';Before='x*x+y*y+z*z>=1.f';After='x*x+y*y+z*z>=1600.f'},
   @{Name='axes';Before='turned(p.chairPitch,g_sent.chairPitch)';After='false'},
   @{Name='interpolation';Before='(std::min)(1.f,float(now-a.lerpAt)/50.f)';After='1.f'},
   @{Name='off';Before='if(!p.active){ClearAnchor(slot);return;}';After='if(false){ClearAnchor(slot);return;}'},
   @{Name='authority';Before='if(scope.role==coop::net::Role::Host){';After='if(false){'},
   @{Name='late';Before='if(!chair)return Admission::Pending;';After='if(!chair)return Admission::Refused;'},
   @{Name='lifetime';Before='if(!(now==g_scope)){ResetState();if(!(now==CaptureScope()))return false;g_scope=now;}';After='if(!(now==g_scope)){g_scope=now;}'},
   @{Name='sample';Before='return valid()&&ue_wrap::prop::GetInteractableKeyString(chair)==key&&valid();';After='return true;'},
   @{Name='transitions';Before='read&&!computer&&!ue_wrap::sleep::IsSleeping()';After='read&&!ue_wrap::sleep::IsSleeping()'},
   @{Name='completion';Before=$completion;After='if(false)return;'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing chair control anchor: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $override=Join-Path $output ($control.Name+'.h');[IO.File]::WriteAllText($override,('#define CHAIR_SYNC_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $exe=Join-Path $output ($control.Name+'.exe');Compile-Chair $exe $override
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Chair control did not reach assertion: $($control.Name)"}
   Write-Output ('PASS chair fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
}finally{Pop-Location}
