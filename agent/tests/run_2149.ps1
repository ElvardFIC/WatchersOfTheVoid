$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2149'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Extract($path,$names,$destination) {
  $source=Get-Content $path -Raw
  $bodies=foreach($name in $names){
   $body=[regex]::Match($source,"(?ms)^(?:bool|ue_wrap::script_gate::Verdict) ${name}\(.*?^\}")
   if(-not $body.Success){throw "Production function missing: $name"}
   $body.Value
  }
  [IO.File]::WriteAllText((Join-Path $output $destination),($bodies -join "`r`n"),[Text.UTF8Encoding]::new($false))
 }
 Extract 'src/votv-coop/src/coop/interactables/atv_sync.cpp' @('IsLocalGrabber','IsHeldForMount','OnMountPre','IsOccupiedByOther') 'mount.h'
 Extract 'src/votv-coop/src/ue_wrap/devices/atv.cpp' @('FindCompanionLocation') 'placement.h'
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/atv_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/atv.obj" "/Fe$output/atv.exe" agent/tests/atv_2149_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'ATV test compile failed'}
 & "$output/atv.exe"
 if($LASTEXITCODE -ne 0){throw 'ATV test failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/spray.obj" "/Fe$output/spray.exe" agent/tests/extinguisher_2145_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Spray test compile failed'}
 & "$output/spray.exe"
 if($LASTEXITCODE -ne 0){throw 'Spray test failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/washer.obj" "/Fe$output/washer.exe" agent/tests/washer_wrapper_2149_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Washer wrapper test compile failed'}
 & "$output/washer.exe"
 if($LASTEXITCODE -ne 0){throw 'Washer wrapper test failed'}
 foreach($file in @('coop/interactables/atv_sync','ue_wrap/devices/atv','ue_wrap/core/trace','coop/player/hand_extinguisher_sync','ue_wrap/actors/pressure_washer','coop/dispatch/event_dispatch_world')) {
  $name=$file -replace '/','_'
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE -ne 0){throw "Source compilation failed: $file"}
 }
}finally{Pop-Location}
