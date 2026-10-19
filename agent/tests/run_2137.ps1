param([string]$Python='python')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2137'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . .\_toolchain\env.ps1
 function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  $rc=$LASTEXITCODE
  if($negative){if($rc -ne 1){throw "Negative assertion expected1, got $rc for $name"};Write-Host "PASS negative control: $name"}
  elseif($rc -ne 0){throw "Test failed: $name"}
 }
 Compile-AndRun 'controller' 'agent/tests/prop_fire_2137_test.cpp' $false
 Compile-AndRun 'wrapper' 'agent/tests/prop_fire_wrapper_2137_test.cpp' $false
 & $Python agent/tests/blueprint_fire_2137_test.py
 if($LASTEXITCODE -ne 0){throw 'Native evidence/integration failed'}
 $cases=@(
 @{Name='host_gate';Suite='prop_fire';File='coop/props/prop_fire_sync.cpp';Before='(!Client()||net_pump::IsInAnnouncedWorld(a))';After='net_pump::IsInAnnouncedWorld(a)'},
 @{Name='host_only';Suite='prop_fire';File='coop/props/prop_fire_sync.cpp';Before='sender!=0||p.op!=2';After='p.op!=2'},
 @{Name='reach';Suite='prop_fire';File='coop/props/prop_fire_sync.cpp';Before='if(!allowed)continue;';After=''},
 @{Name='duplicate';Suite='prop_fire';File='coop/props/prop_fire_sync.cpp';Before='if(r->have&&!After(p.sequence,r->state.sequence))return;';After=''},
 @{Name='age';Suite='prop_fire';File='coop/props/prop_fire_sync.cpp';Before='value.fuel-value.fuelSpeed*float(now>=r.received?now-r.received:0)/1000.f';After='value.fuel'},
 @{Name='cancel';Suite='prop_fire_wrapper';File='ue_wrap/actors/prop_fire.cpp';Before='actor.Is(a)&&world==world_identity::Generation()&&IsProp(a)&&current()';After='true'},
 @{Name='subclass';Suite='prop_fire_wrapper';File='ue_wrap/actors/prop_fire.cpp';Before='Verb(a,L"ignite",&state.fuel,base)';After='Verb(a,L"ignite",&state.fuel)'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2137_test.cpp')))
  $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
  $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
  Compile-AndRun $case.Name $copy $true
 }
 $global:LASTEXITCODE=0
} finally {Pop-Location}
