param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2153'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($name in @('bucket_2153','extinguisher_2145')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/material_2153_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $name"}
  & "$output/$name.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
 }
 if($FaultControls){
  $control=Join-Path $output 'missing-stock-guard'
  New-Item -ItemType Directory -Force -Path $control | Out-Null
  $source=Get-Content 'src/votv-coop/src/coop/props/bucket_sync.cpp' -Raw
  $mutant=$source.Replace(' && Exact(state,live)','')
  if($mutant -eq $source){throw 'Missing stock guard mutation site'}
  $mutantPath=(Join-Path $control 'bucket.inc').Replace('\','/')
  [IO.File]::WriteAllText($mutantPath,$mutant,[Text.UTF8Encoding]::new($false))
  $testPath=(Join-Path $PSScriptRoot 'bucket_2153_test.cpp').Replace('\','/')
  $fixture=Join-Path $control 'control.cpp'
  $lines=@(('#define BUCKET_SOURCE "'+$mutantPath+'"'),('#include "'+$testPath+'"'))
  [IO.File]::WriteAllText($fixture,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/material_2153_stubs" "/I$PSScriptRoot/state_2145_stubs" "/I$PSScriptRoot/text_drone_stubs" "/I$PSScriptRoot/stubs" /I src/votv-coop/include "/Fo$control/control.obj" "/Fe$control/control.exe" $fixture /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw 'Stock guard control compile failed'}
  & "$control/control.exe"
  if($LASTEXITCODE -ne 1){throw 'Stock guard control did not fire its assertion'}
  Write-Output 'PASS expected-failure control: missing-stock-guard'
 }
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/building_wrapper.obj" "/Fe$output/building_wrapper.exe" agent/tests/building_wrapper_2153_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Building wrapper compile failed'}
 & "$output/building_wrapper.exe"
 if($LASTEXITCODE -ne 0){throw 'Building wrapper test failed'}
 if($CompileSources){
  foreach($file in @('coop/props/bucket_sync','ue_wrap/actors/building_material','coop/player/hand_extinguisher_sync','coop/session/subsystems')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
