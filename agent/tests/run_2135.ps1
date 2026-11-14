param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'),[switch]$Quick)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2135'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . (Join-Path $root '../_toolchain/env.ps1')
 foreach($suite in @('notebook','drone_intent','drone_state','drone_wrapper')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$suite.obj" "/Fe$output/$suite.exe" "agent/tests/${suite}_2135_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $suite"}
  & "$output/$suite.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $suite"}
 }
 & $Python agent/tests/blueprint_text_drone_2135_test.py
 if($LASTEXITCODE -ne 0){throw 'Native evidence failed'}
 if(-not $Quick) {
  $cases=@(
   @{Name='close_ref';Suite='notebook';File='coop/props/notebook_refresh.cpp';Before='if (!c.fromOurCode) Mark(Clipboard(c.object));';After=''},
   @{Name='new_edit';Suite='notebook';File='coop/props/notebook_refresh.cpp';Before='if (row.version != copy.version) continue;';After=''},
   @{Name='reach';Suite='drone_intent';File='coop/interactables/drone_call_intent.cpp';Before='if (ok) console = ref.Get();';After='console = ref.Get();'},
   @{Name='expiry';Suite='drone_intent';File='coop/interactables/drone_call_intent.cpp';Before='if (coop::active_drive::NowMs() - r.at > 3000) continue;';After=''},
   @{Name='depart';Suite='drone_intent';File='coop/interactables/drone_call_intent.cpp';Before='occupant == g_occupancy[slot]';After='true'},
   @{Name='heartbeat';Suite='drone_state';File='coop/interactables/drone_sync.cpp';Before=' && nowMs - g_lastSendMs < 2000';After=''},
   @{Name='mask';Suite='drone_wrapper';File='ue_wrap/devices/drone.cpp';Before='b = uint8_t(value ? b | mask : b & ~mask);';After='b = value ? mask : 0;'},
   @{Name='failed_cache';Suite='drone_wrapper';File='ue_wrap/devices/drone.cpp';Before='cachedOffset = -1; cachedMask = 0;'+"`n"+'            return false;';After='return false;'}
  )
  foreach($case in $cases) {
   $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File))).Replace("`r`n","`n")
   if(-not $source.Contains($case.Before)){throw "Mutation missing: $($case.Name)"}
   $mutant=Join-Path $output ($case.Name+'_source.cpp');[IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
   $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2135_test.cpp')))
   $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
   $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $copy /link /OPT:REF
   if($LASTEXITCODE -ne 0){throw "Mutant compile failed: $($case.Name)"}
   & "$output/$($case.Name).exe"
   if($LASTEXITCODE -ne 1){throw "Expected assertion exit 1: $($case.Name), got $LASTEXITCODE"}
   Write-Host "PASS negative control: $($case.Name)"
  }
 }
 $global:LASTEXITCODE=0
} finally {Pop-Location}
