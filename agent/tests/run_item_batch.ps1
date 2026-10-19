$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/item-batch-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . (Join-Path $root '../_toolchain/env.ps1')
 foreach($name in @('held_record_capture','local_hold_yield','prop_scale_sync','prop_drive_2123')) {
  $includes=if($name -ne 'prop_drive_2123'){@('/Iagent/tests/state_2145_stubs','/Iagent/tests/text_drone_stubs')}else{@()}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes /Iagent/tests/stubs /Isrc/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF *> (Join-Path $output "$name.log")
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name (see $output)"}
  & "$output/$name.exe" >> (Join-Path $output "$name.log")
  if($LASTEXITCODE -ne 0){throw "Regression failed: $name (see $output)"}
 }
 foreach($number in @('2142','2146','2148','2158','2159','2176','2188')) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File "agent/tests/run_$number.ps1" *> (Join-Path $output "$number.log")
  if($LASTEXITCODE -ne 0){throw "Regression failed: $number (see $output)"}
 }
 & "$env:USERPROFILE/.local/bin/python3.15.exe" agent/tests/item_batch_native_test.py *> (Join-Path $output 'native.log')
 if($LASTEXITCODE -ne 0){throw "Native evidence failed (see $output)"}
 Write-Output 'PASS item batch: live held records, scale, motion, knock, pry, birth provenance, food lifecycle, tool reader, inventory and native evidence'
} finally {Pop-Location}
