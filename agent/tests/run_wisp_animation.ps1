$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run_player_reports.ps1') -Only wisp_spawn_race
if($LASTEXITCODE -ne 0){throw 'Wisp animation regression failed'}
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/wisp-animation-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/wisp_montage_test.obj" "/Fe$output/wisp_montage_test.exe" `
    "$PSScriptRoot/wisp_montage_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Wisp montage regression compile failed'}
& "$output/wisp_montage_test.exe"
if($LASTEXITCODE -ne 0){throw 'Wisp montage regression failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/wisp_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/wisp_release_test.obj" "/Fe$output/wisp_release_test.exe" `
    "$PSScriptRoot/wisp_release_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Wisp release regression compile failed'}
& "$output/wisp_release_test.exe"
if($LASTEXITCODE -ne 0){throw 'Wisp release regression failed'}
foreach($unit in @('coop/creatures/wisp_tear_mirror','coop/creatures/wisp_grab_hold','coop/creatures/wisp_attack_sync','ue_wrap/actors/wisp')){
    $name=$unit.Replace('/','_')
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" `
        "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/$unit.cpp"
    if($LASTEXITCODE -ne 0){throw "Production compile failed: $unit"}
}
