param([string]$Python='python')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2140'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . .\_toolchain\env.ps1
 & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/events_stubs /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/explosion_events_2140_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Compile general explosion controller failed'}
 & "$output/controller.exe"
 if($LASTEXITCODE -ne 0){throw 'General explosion controller failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/explosion_stubs /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/explosion_blast_2140_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Compile general blast wrapper failed'}
 & "$output/wrapper.exe"
 if($LASTEXITCODE -ne 0){throw 'General blast wrapper failed'}
 & $Python agent/tests/blueprint_explosion_2140_test.py
 if($LASTEXITCODE -ne 0){throw 'Cooked general explosion evidence failed'}
 $cases=@(
 @{Name='sender';Suite='explosion_events';File='coop/props/explosion_events.cpp';Before='sender!=0||p.op!=1';After='p.op!=1'},
 @{Name='reach';Suite='explosion_events';File='coop/props/explosion_events.cpp';Before='if(!reach)continue;';After=''},
 @{Name='duplicate';Suite='explosion_events';File='coop/props/explosion_events.cpp';Before='(g_lastBlast&&!After(p.sequence,g_lastBlast))';After='false'},
 @{Name='source_serial';Suite='explosion_events';File='coop/props/explosion_events.cpp';Before='R::SlotSerial(d.actor.Idx())==d.serial';After='true'},
 @{Name='world_damage';Suite='explosion_events';File='coop/props/explosion_events.cpp';Before='!Client()||c.object==hand_item::LocalHandActor()?sg::Verdict::Run:sg::Verdict::Cancel';After='sg::Verdict::Run'},
 @{Name='force';Suite='explosion_blast';File='ue_wrap/actors/explosion_blast.cpp';Before='b.force=0;b.debris=0;';After='b.force=5000;b.debris=0;'},
 @{Name='debris';Suite='explosion_blast';File='ue_wrap/actors/explosion_blast.cpp';Before='b.force=0;b.debris=0;';After='b.force=0;b.debris=6;'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2140_test.cpp')))
  $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
  $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/events_stubs /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $copy /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile mutation failed: $($case.Name)"}
  & "$output/$($case.Name).exe"
  if($LASTEXITCODE -ne 1){throw "Expected assertion failure1: $($case.Name), got $LASTEXITCODE"}
  Write-Host "PASS negative control: $($case.Name)"
 }
 $global:LASTEXITCODE=0
}finally{Pop-Location}
