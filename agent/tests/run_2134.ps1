param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2134'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . (Join-Path $root '../_toolchain/env.ps1')
 function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/lift_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  $rc=$LASTEXITCODE
  if($negative){if($rc -ne 1){throw "Negative assertion expected1, got $rc for $name"};Write-Host "PASS negative control: $name"}
  elseif($rc -ne 0){throw "Test failed: $name"}
 }
 Compile-AndRun 'controller' 'agent/tests/cargo_lift_2134_test.cpp' $false
 Compile-AndRun 'wrapper' 'agent/tests/cargo_lift_wrapper_2134_test.cpp' $false
 & $Python agent/tests/blueprint_lift_2134_test.py
 if($LASTEXITCODE -ne 0){throw 'Native evidence/integration failed'}
 $cases=@(
 @{Name='host_sender';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='sender!=0||';After=''},
 @{Name='duplicate';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='(g_lastRequest[sender]&&!After(p.sequence,g_lastRequest[sender]))';After='false'},
 @{Name='reach';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='if(!authorized)continue;';After=''},
 @{Name='stale_revision';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='if(row->have&&!After(p.sequence,row->canonical.sequence))return;';After=''},
 @{Name='expire';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='if(now-req.at>3000)continue;';After=''},
 @{Name='departure';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='std::erase_if(g_requests,[slot](const Request& r){return r.sender==slot;});';After=''},
 @{Name='depart_in_call';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='&&req.occupancy==g_occupancy[req.sender]';After=''},
 @{Name='reentry';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='!s||g_ticking';After='!s'},
 @{Name='epoch';Suite='cargo_lift';File='coop/world/cargo_lift_sync.cpp';Before='epoch==g_epoch&&';After=''},
 @{Name='physics_restore';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='if(body)Simulation(body,old.simulated);';After=''},
 @{Name='native_failure';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='if(!Position(m.body.Get(),s.position,&s.rotation,snap)||!valid())return false;';After='Position(m.body.Get(),s.position,&s.rotation,snap);'},
 @{Name='param_binding';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='(!rot||f.Set<FRotator>(L"NewRotation",*rot))';After='(!rot||(f.Set<FRotator>(L"NewRotation",*rot),true))'},
 @{Name='moving_latch';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='g_moving.Write(old.actor.Get(),false);';After=''},
 @{Name='field_mask';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='b=uint8_t(v?b|mask:b&~mask)';After='b=uint8_t(v?mask:0)'},
 @{Name='capture_cancel';Suite='cargo_lift_wrapper';File='ue_wrap/devices/cargo_lift.cpp';Before='||!stillCurrent())return false;';After=')return false;'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2134_test.cpp')))
  $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
  $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
  Compile-AndRun $case.Name $copy $true
 }
 $global:LASTEXITCODE=0
} finally {Pop-Location}
