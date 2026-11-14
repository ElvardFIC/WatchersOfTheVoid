param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2138'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . (Join-Path $root '../_toolchain/env.ps1')
 function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/hand_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  $rc=$LASTEXITCODE
  if($negative){if($rc -ne 1){throw "Expected assertion failure for $name, got $rc"};Write-Host "PASS negative control: $name"}
  elseif($rc -ne 0){throw "Test failed: $name"}
 }
 Compile-AndRun 'hand' 'agent/tests/hand_fire_2138_test.cpp' $false
 & $Python agent/tests/blueprint_hand_fire_2138_test.py
 if($LASTEXITCODE -ne 0){throw 'Native evidence failed'}
 $cases=@(
 @{Name='generation';File='hand_fire_sync.cpp';Before='hand_item::MirrorGeneration(slot) != p.generation';After='false'},
 @{Name='authority';File='hand_fire_sync.cpp';Before='sender != p.slot';After='false'},
 @{Name='duplicate';File='hand_fire_sync.cpp';Before='if (r.state.sequence && !After(p.sequence, r.state.sequence)) return;';After=''},
 @{Name='age';File='hand_fire_sync.cpp';Before='value.fuel - value.fuelSpeed * float(now >= r.received ? now - r.received : 0) / 1000.f';After='value.fuel'},
 @{Name='cancel';File='hand_fire_sync.cpp';Before='const auto valid = [&] { return Current(epoch) && identity.Is(a) && r.state.sequence == p.sequence &&';After='const auto valid = [&] { return true || Current(epoch) && identity.Is(a) && r.state.sequence == p.sequence &&'},
 @{Name='same_class';File='hand_item.cpp';Before='if (identityChanged || !g_ownHas || cls != g_ownCls';After='if (!g_ownHas || cls != g_ownCls'},
 @{Name='spawn_cancel';File='hand_item.cpp';Before='return epoch == g_handEpoch &&';After='return true || epoch == g_handEpoch &&'},
 @{Name='unplaced';File='hand_item.cpp';Before='pup && pup->valid() && pup->HasPose()';After='pup && pup->valid()'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/coop/player/'+$case.File)))
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'hand_fire_2138_test.cpp'))
  $harness=$harness.Replace('../../src/votv-coop/src/coop/player/'+$case.File,($mutant -replace '\\','/'))
  $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
  Compile-AndRun $case.Name $copy $true
 }
 $global:LASTEXITCODE=0
} finally {Pop-Location}
