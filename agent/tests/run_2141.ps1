param([string]$Python='python')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2141'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/cord.obj" "/Fe$output/cord.exe" agent/tests/cord_perf_2141_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Compile cord performance test failed'}
 & "$output/cord.exe"
 if($LASTEXITCODE -ne 0){throw 'Cord performance/behavior test failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/media.obj" "/Fe$output/media.exe" agent/tests/media_perf_2141_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Compile media performance test failed'}
 & "$output/media.exe"
 if($LASTEXITCODE -ne 0){throw 'Media performance/behavior test failed'}
 $sourceRoot=($root -replace '\\','/')+'/src/votv-coop/src/'
 $cases=@(
 @{Name='quadratic';Suite='cord';File='coop/interactables/cord_sync.cpp';Before='pass.Find(';After='Resolve('},
 @{Name='invalidate';Suite='cord';File='coop/interactables/cord_sync.cpp';Before='pass.Invalidate();';After=''},
 @{Name='serial';Suite='cord';File='coop/interactables/cord_sync.cpp';Before='!cordRef.Get()';After='!cordRef.Raw()'},
 @{Name='string_free';Suite='media';File='ue_wrap/world/media_player.cpp';Before='if(p) R::EngineFree(p);';After=''},
 @{Name='indexed';Suite='media';File='ue_wrap/world/media_player.cpp';Before='return actors;';After='R::FindObjectsByClass(L"MediaPlayer"); return actors;'}
 )
 foreach($case in $cases){
  $file=$sourceRoot+$case.File;$source=[IO.File]::ReadAllText($file)
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_perf_2141_test.cpp'))).Replace('../../src/votv-coop/src/',$sourceRoot)
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness.Replace($file,($mutant -replace '\\','/')))
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/fire_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $copy /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile mutation failed: $($case.Name)"}
  & "$output/$($case.Name).exe"
  if($LASTEXITCODE -ne 1){throw "Expected assertion failure1: $($case.Name), got $LASTEXITCODE"}
  Write-Host "PASS negative control: $($case.Name)"
 }
 $global:LASTEXITCODE=0
}finally{Pop-Location}
