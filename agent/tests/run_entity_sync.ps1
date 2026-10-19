param([switch]$GrayboarOnly)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/entity-sync-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
if(-not $GrayboarOnly) {
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/npc_pose_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/npc_orientation.obj" "/Fe$output/npc_orientation.exe" "$PSScriptRoot/npc_orientation_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'NPC orientation regression compile failed'}
& "$output/npc_orientation.exe"
if($LASTEXITCODE -ne 0){throw 'NPC orientation regression failed'}
}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/grayboar_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/grayboar_sync.obj" "/Fe$output/grayboar_sync.exe" "$PSScriptRoot/grayboar_sync_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Grayboar regression compile failed'}
& "$output/grayboar_sync.exe"
if($LASTEXITCODE -ne 0){throw 'Grayboar regression failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/grayboar_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/grayboar_actions.obj" "/Fe$output/grayboar_actions.exe" "$PSScriptRoot/grayboar_actions_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Grayboar actions compile failed'}
& "$output/grayboar_actions.exe"
if($LASTEXITCODE -ne 0){throw 'Grayboar actions regression failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/grayboar_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/grayboar_turn.obj" "/Fe$output/grayboar_turn.exe" "$PSScriptRoot/grayboar_turn_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Grayboar turn compile failed'}
& "$output/grayboar_turn.exe"
if($LASTEXITCODE -ne 0){throw 'Grayboar turn regression failed'}
if($GrayboarOnly){exit 0}
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$PSScriptRoot/stubs" "/I$checkout/src/votv-coop/include" "/I$checkout/src/votv-coop/src" `
    "/Fo$output/prop_drive.obj" "/Fe$output/prop_drive.exe" "$PSScriptRoot/prop_drive_2123_test.cpp"
if($LASTEXITCODE -ne 0){throw 'Prop drive regression compile failed'}
& "$output/prop_drive.exe"
if($LASTEXITCODE -ne 0){throw 'Prop drive regression failed'}
foreach($unit in @('coop/creatures/npc_world_enum','coop/creatures/npc_pose_host','coop/creatures/npc_pose_drive','coop/creatures/npc_mirror','coop/creatures/grayboar_sync','coop/creatures/grayboar_actions','coop/creatures/grayboar_turn','coop/props/prop_drive_stream','coop/session/subsystems','coop/net/session_npc','coop/net/session_streams','coop/dispatch/event_dispatch_world')) {
    $name=$unit.Replace('/','_')
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" `
        "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/$unit.cpp"
    if($LASTEXITCODE -ne 0){throw "Production compile failed: $unit"}
}
