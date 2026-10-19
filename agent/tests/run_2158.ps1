param([switch]$CompileSources,[switch]$OnlySpawn)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2158'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/engine/engine_mainplayer.cpp'))
 $first=$source.IndexOf('bool ReadMainPlayerGrabState('); $last=$source.IndexOf('namespace {', $first)
 if($first -lt 0 -or $last -le $first){throw 'Grab-state function boundaries missing'}
 [IO.File]::WriteAllText((Join-Path $output 'grab_state.h'),$source.Substring($first,$last-$first))
 if(-not $OnlySpawn){
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/physics_gun.obj" "/Fe$output/physics_gun.exe" agent/tests/physics_gun_2158_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Physics gun test compile failed'}
 & "$output/physics_gun.exe"
 if($LASTEXITCODE -ne 0){throw 'Physics gun test failed'}
 }
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/floor_stubs /I src/votv-coop/include "/Fo$output/birth.obj" "/Fe$output/birth.exe" agent/tests/floor_host_2132_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Finished birth test compile failed'}
 & "$output/birth.exe"
 if($LASTEXITCODE -ne 0){throw 'Finished birth test failed'}
 if($CompileSources){
  $files=if($OnlySpawn){@('coop/props/host_spawn_watcher')}else{@('ue_wrap/actors/physics_gun','ue_wrap/engine/engine_mainplayer','coop/player/local_streams','coop/interactables/atv_sync','coop/props/prop_knock','coop/props/host_spawn_watcher')}
  foreach($file in $files) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
