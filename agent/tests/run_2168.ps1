param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2168'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($testName in @('wall_damage_2168','wall_wrapper_2166','wall_enrollment_2167','wall_authority_2166','building_wrapper_2153')) {
  $includes=switch($testName){
   'wall_damage_2168' {@('/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
   'wall_authority_2166' {@('/I','agent/tests/authority_2165_stubs','/I','agent/tests/text_drone_stubs')}
   default {@('/I','agent/tests/placed_save_stubs')}
  }
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 if($CompileSources){
  foreach($file in @('coop/props/prop_action_sync','ue_wrap/actors/wall_build','ue_wrap/actors/building_material','coop/items/player_inventory_sync')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
