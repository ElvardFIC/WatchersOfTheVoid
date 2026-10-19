param([string]$Python='python')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2136'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . .\_toolchain\env.ps1
 function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/lift_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  $rc=$LASTEXITCODE
  if($negative){if($rc -ne 1){throw "Negative assertion expected1, got $rc for $name"};Write-Host "PASS negative control: $name"}
  elseif($rc -ne 0){throw "Test failed: $name"}
 }
 Compile-AndRun 'controller' 'agent/tests/cargo_lift_2136_test.cpp' $false
 Compile-AndRun 'wrapper' 'agent/tests/cargo_lift_wrapper_2136_test.cpp' $false
 & $Python agent/tests/blueprint_lift_2136_test.py
 if($LASTEXITCODE -ne 0){throw 'Native evidence/integration failed'}
 $cases=@(
 @{Name='host_gate';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='s->role()==net::Role::Client&&!net_pump::IsInAnnouncedWorld(a)';After='!net_pump::IsInAnnouncedWorld(a)'},
 @{Name='replay_echo';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='if(g_replaying ||';After='if('},
 @{Name='duplicate';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='(g_lastRequest[sender]&&!After(p.sequence,g_lastRequest[sender]))';After='false'},
 @{Name='host_only';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='sender!=0||';After=''},
 @{Name='heartbeat_repeat';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='if(row->applied)return;';After='row->applied=false;'},
 @{Name='reach';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='if(!authorized)continue;';After=''},
 @{Name='native_direction';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='state.moving?!state.direction:state.direction';After='state.direction'},
 @{Name='busy';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='if(!g_moving.Read(a,moving)||moving)return false;';After='if(!g_moving.Read(a,moving))return false;'},
 @{Name='identity';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='Current(parts)&&stillCurrent()';After='stillCurrent()'},
 @{Name='native_failure';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='if(!Position(parts.body.Get(),state.position,&state.rotation,true)||!valid())return false;';After='Position(parts.body.Get(),state.position,&state.rotation,true);'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2136_test.cpp')))
  $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
  $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
  Compile-AndRun $case.Name $copy $true
 }
 $global:LASTEXITCODE=0
} finally {Pop-Location}
