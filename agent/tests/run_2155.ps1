param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2155'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/dingus.obj" "/Fe$output/dingus.exe" agent/tests/dingus_2155_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Dingus test compile failed'}
 & "$output/dingus.exe"
 if($LASTEXITCODE -ne 0){throw 'Dingus test failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/console.obj" "/Fe$output/console.exe" agent/tests/console_2154_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Console test compile failed'}
 & "$output/console.exe"
 if($LASTEXITCODE -ne 0){throw 'Console test failed'}
 if($CompileSources){
  foreach($file in @('coop/props/dingus_music_sync','ue_wrap/actors/dingus_music','coop/interactables/sat_console_sync','coop/dispatch/event_dispatch_world','coop/session/subsystems')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
