param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2160'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($testName in @('bucket_2153','building_wrapper_2153','wrappers_2129','gate_scope_2160')) {
  $includes=if($testName -in @('building_wrapper_2153','gate_scope_2160')){@()}else{@('/I','agent/tests/material_2153_stubs','/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 if($CompileSources){
  foreach($file in @('coop/props/bucket_sync','ue_wrap/actors/building_material','coop/props/prop_spawn_authoring','ue_wrap/core/script_gate')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
