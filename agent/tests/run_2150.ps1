$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2150'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/desk.obj" "/Fe$output/desk.exe" agent/tests/desk_circuit_2150_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Desk test compile failed'}
 & "$output/desk.exe"
 if($LASTEXITCODE -ne 0){throw 'Desk test failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/floor_stubs /I src/votv-coop/include "/Fo$output/birth.obj" "/Fe$output/birth.exe" agent/tests/floor_host_2132_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Finished birth test compile failed'}
 & "$output/birth.exe"
 if($LASTEXITCODE -ne 0){throw 'Finished birth test failed'}
 foreach($file in @('coop/world/desk_short_circuit_sync','ue_wrap/desk/console_desk','coop/props/host_spawn_watcher','coop/dispatch/event_dispatch_world','coop/session/subsystems')) {
  $name=$file -replace '/','_'
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE -ne 0){throw "Source compilation failed: $file"}
 }
}finally{Pop-Location}
