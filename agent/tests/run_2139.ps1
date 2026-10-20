param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2139'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . (Join-Path $root '../_toolchain/env.ps1')
 & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/explosion_stubs /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/explosion_2139_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Compile explosion controller failed'}
 & "$output/controller.exe"
 if($LASTEXITCODE -ne 0){throw 'Explosion controller failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/explosion_stubs /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/explosion_wrapper_2139_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Compile explosion wrapper failed'}
 & "$output/wrapper.exe"
 if($LASTEXITCODE -ne 0){throw 'Explosion wrapper failed'}
 & $Python agent/tests/blueprint_explosion_2139_test.py
 if($LASTEXITCODE -ne 0){throw 'Native explosion evidence/integration failed'}
 $cases=@(
 @{Name='sender';Suite='explosion';File='coop/props/explosion_sync.cpp';Before='sender!=0||p.op!=1';After='p.op!=1'},
 @{Name='reach';Suite='explosion';File='coop/props/explosion_sync.cpp';Before='if(!allowed)continue;';After=''},
 @{Name='duplicate';Suite='explosion';File='coop/props/explosion_sync.cpp';Before='(g_lastBlast&&!After(p.sequence,g_lastBlast))';After='false'},
 @{Name='serial';Suite='explosion';File='coop/props/explosion_sync.cpp';Before='R::SlotSerial(m.actor.Idx())==m.serial';After='true'},
 @{Name='stack';Suite='explosion';File='coop/props/explosion_sync.cpp';Before='||!sg::IsBodyActive(source.function.Get())';After=''},
 @{Name='world_damage';Suite='explosion';File='coop/props/explosion_sync.cpp';Before='c.object==hand_item::LocalHandActor()?sg::Verdict::Run:sg::Verdict::Cancel';After='sg::Verdict::Run'},
 @{Name='force';Suite='explosion_wrapper';File='ue_wrap/actors/prop_explosion.cpp';Before='WriteField(a,L"force",0.f)';After='WriteField(a,L"force",5000.f)'},
 @{Name='debris';Suite='explosion_wrapper';File='ue_wrap/actors/prop_explosion.cpp';Before='WriteField(a,L"debris",int32_t(0))';After='WriteField(a,L"debris",int32_t(6))'},
 @{Name='cleanup_identity';Suite='explosion_wrapper';File='ue_wrap/actors/prop_explosion.cpp';Before='current()&&ref.Get()&&world==world_identity::Generation()';After='current()&&world==world_identity::Generation()'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2139_test.cpp')))
  $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
  $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/explosion_stubs /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $copy /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile mutation failed: $($case.Name)"}
  & "$output/$($case.Name).exe"
  if($LASTEXITCODE -ne 1){throw "Expected assertion failure1: $($case.Name), got $LASTEXITCODE"}
  Write-Host "PASS negative control: $($case.Name)"
 }
 $global:LASTEXITCODE=0
} finally {Pop-Location}
