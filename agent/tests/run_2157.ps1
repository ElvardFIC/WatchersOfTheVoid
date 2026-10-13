param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2157'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($testName in @('hand_toy_2157','dingus_2155','prop_knock_2157')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 if($CompileSources){
  foreach($file in @('coop/player/hand_toy_audio','coop/props/dingus_music_sync','ue_wrap/actors/dingus_music','coop/props/prop_knock','coop/props/prop_drive_host','coop/dispatch/event_dispatch_world','coop/session/subsystems')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
