param([switch]$CompileSources,[switch]$OnlyBucket)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2159'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $tests=if($OnlyBucket){@('extinguisher_2145','building_wrapper_2153')}else{@('floor_host_2132','prop_knock_2157','prop_action_2143','extinguisher_2145','building_wrapper_2153')}
 foreach($testName in $tests) {
  $includes=if($testName -eq 'floor_host_2132'){@('/I','agent/tests/floor_stubs')}elseif($testName -eq 'building_wrapper_2153'){@()}else{@('/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 if($CompileSources){
  $files=if($OnlyBucket){@('ue_wrap/actors/building_material','coop/player/hand_extinguisher_sync')}else{@('coop/props/host_spawn_watcher','coop/props/prop_knock','coop/props/prop_action_sync','ue_wrap/actors/building_material','coop/player/hand_extinguisher_sync')}
  foreach($file in $files) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
