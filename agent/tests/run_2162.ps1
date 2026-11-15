param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2162'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($testName in @('safe_2162','prop_action_2143','item_record_reentry','kitchen_2124')) {
  $includes=if($testName -eq 'safe_2162'){@()}elseif($testName -eq 'prop_action_2143'){@('/I',(Join-Path $PSScriptRoot 'text_drone_stubs'),'/I',(Join-Path $PSScriptRoot 'stubs'))}elseif($testName -eq 'item_record_reentry'){@('/I',(Join-Path $PSScriptRoot 'item_record_stubs'))}else{@('/I',(Join-Path $PSScriptRoot 'stubs'))}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "$PSScriptRoot/${testName}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 if($CompileSources){
  foreach($file in @('coop/props/prop_action_sync','coop/props/prop_save_data','ue_wrap/actors/safe','coop/interactables/kitchen_sync','ue_wrap/devices/kitchen_machine','coop/dispatch/event_dispatch_world')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
