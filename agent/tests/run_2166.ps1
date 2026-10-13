param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2166'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($testName in @('wall_placement_2166','wall_record_2166','wall_authority_2166','wall_wrapper_2166')) {
  $stub=if($testName -eq 'wall_authority_2166'){'agent/tests/authority_2165_stubs'}else{'agent/tests/placed_save_stubs'}
  $extra=if($testName -eq 'wall_authority_2166'){@('/I','agent/tests/text_drone_stubs')}else{@()}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I $stub @extra /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 if($CompileSources){
  foreach($file in @('ue_wrap/actors/placed_save','ue_wrap/actors/placed_save_record','ue_wrap/actors/wall_build','coop/world/wall_build_sync','coop/world/placed_save_sync')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
