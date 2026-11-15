param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2161'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($testName in @('location_2161','teleport_2161')) {
  if($testName -eq 'teleport_2161'){
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/config_registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
   if($LASTEXITCODE -ne 0){throw 'Teleport config registry compile failed'}
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include /c "/Fo$output/config_registry_log.obj" "$PSScriptRoot/config_registry_log_stub.cpp"
   if($LASTEXITCODE -ne 0){throw 'Teleport config log stub compile failed'}
  }
  if($testName -eq 'teleport_2161'){
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I "$PSScriptRoot/location_2161_stubs" /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "$PSScriptRoot/${testName}_test.cpp" "$output/config_registry.obj" "$output/config_registry_log.obj" /link /OPT:REF
  }else{
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I "$PSScriptRoot/location_2161_stubs" /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "$PSScriptRoot/${testName}_test.cpp" /link /OPT:REF
  }
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 if($CompileSources){
  foreach($file in @('coop/world/event_location_sync','ue_wrap/world/event_location','coop/session/teleport_client','coop/world/event_fire_sync','coop/session/subsystems')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
