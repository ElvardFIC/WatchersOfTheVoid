$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2148'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $source=Get-Content 'src/votv-coop/src/coop/interactables/atv_sync.cpp' -Raw
 $body=[regex]::Match($source,'(?ms)^bool IsHeldForMount\(.*?^\}')
 if(-not $body.Success){throw 'Production mounting query not found'}
 [IO.File]::WriteAllText((Join-Path $output 'atv_mount_body.h'),$body.Value,[Text.UTF8Encoding]::new($false))
 foreach($name in @('prop_pry','atv_mount')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/atv_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_2148_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
 }
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/prop_action.obj" "/Fe$output/prop_action.exe" agent/tests/prop_action_2143_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Prop action test compilation failed'}
 & "$output/prop_action.exe"
 if($LASTEXITCODE -ne 0){throw 'Prop action test failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/birth.obj" "/Fe$output/birth.exe" agent/tests/wrappers_2129_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Birth provenance test compilation failed'}
 & "$output/birth.exe"
 if($LASTEXITCODE -ne 0){throw 'Birth provenance test failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/container.obj" "/Fe$output/container.exe" agent/tests/container_extract_2147_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Container restoration test compilation failed'}
 & "$output/container.exe"
 if($LASTEXITCODE -ne 0){throw 'Container restoration test failed'}
 foreach($file in @('coop/props/prop_stick_sync','coop/interactables/atv_sync','coop/interactables/device_occupancy','coop/items/inventory_pickup_sync','coop/props/prop_action_sync','coop/props/prop_spawn_authoring','coop/props/prop_drop_intent','coop/props/host_spawn_watcher')) {
  $name=Split-Path $file -Leaf
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE -ne 0){throw "Source compile failed: $name"}
 }
}finally{Pop-Location}
