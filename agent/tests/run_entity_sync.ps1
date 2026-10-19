param([switch]$GrayboarOnly)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$toolchain=Join-Path $checkout '../_toolchain/env.ps1'
if(Test-Path -LiteralPath $toolchain) {
    . $toolchain
} elseif(-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    throw 'MSVC is unavailable. Run this script from an x64 Developer PowerShell or initialize the MSVC environment first.'
}
$output=Join-Path $checkout 'build/entity-sync-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
if(-not $GrayboarOnly) {
$enumSource=Get-Content -LiteralPath (Join-Path $checkout 'src/votv-coop/src/coop/creatures/npc_world_enum.cpp') -Raw
if($enumSource.Contains('kExSpawnSourceClasses')) {
    throw 'Dead EX-spawn source table remains and can diverge from the executed source gate'
}
$sourceTable=($enumSource -split 'kExSpawnSources\[\]\s*=\s*\{',2)[1] -split '\};',2 | Select-Object -First 1
foreach($requiredSource in @('ticker_yellowWispSpawner_C','ticker_fossilhoundSpawner_C','boarInvasion_C','ufoDropper_pig_C','ufoDropper_tank_C','firetankDeploy_C','kavotiaPatrolController_C')) {
    if($sourceTable -notmatch [regex]::Escape('L"' + $requiredSource + '"')) {
        throw "Executed EX-spawn source gate is missing $requiredSource"
    }
}
$authoritySource=Get-Content -LiteralPath (Join-Path $checkout 'src/votv-coop/src/coop/world/spawn_authority.cpp') -Raw
if($authoritySource -notmatch 'kavotiaPatrolController\.ReceiveBeginPlay') {
    throw 'Client Kavotia patrol controller BeginPlay is not authority-gated'
}
$ariralSource=Get-Content -LiteralPath (Join-Path $checkout 'src/votv-coop/src/coop/creatures/ariral_sync.cpp') -Raw
if($ariralSource -notmatch 'L"punching"' -or $ariralSource -match 'L"StealGun"|L"TakeWeapon"') {
    throw 'Gun-stealer authority must watch the exported punching timer, not guessed functions'
}
foreach($mirrorGuard in @('NpcMirrors().Snapshot','IsMirror()','LiveActor() == actor')) {
    if(-not $ariralSource.Contains($mirrorGuard)) {
        throw "Gun-stealer authority is missing mirror identity guard: $mirrorGuard"
    }
}
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
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/grayboar_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/kavotia_sync.obj" "/Fe$output/kavotia_sync.exe" "$PSScriptRoot/kavotia_sync_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Kavotia compile failed'}
& "$output/kavotia_sync.exe"
if($LASTEXITCODE -ne 0){throw 'Kavotia regression failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$PSScriptRoot/stubs" "/I$checkout/src/votv-coop/include" "/I$checkout/src/votv-coop/src" `
    "/Fo$output/prop_drive.obj" "/Fe$output/prop_drive.exe" "$PSScriptRoot/prop_drive_2123_test.cpp"
if($LASTEXITCODE -ne 0){throw 'Prop drive regression compile failed'}
& "$output/prop_drive.exe"
if($LASTEXITCODE -ne 0){throw 'Prop drive regression failed'}
foreach($unit in @('coop/creatures/npc_world_enum','coop/creatures/npc_pose_host','coop/creatures/npc_pose_drive','coop/creatures/npc_mirror','coop/creatures/ariral_sync','coop/creatures/grayboar_sync','coop/creatures/grayboar_actions','coop/creatures/grayboar_turn','coop/creatures/kavotia_sync','coop/world/spawn_authority','coop/props/prop_drive_stream','coop/session/subsystems','coop/net/session_npc','coop/net/session_streams','coop/dispatch/event_dispatch_world')) {
    $name=$unit.Replace('/','_')
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" `
        "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/$unit.cpp"
    if($LASTEXITCODE -ne 0){throw "Production compile failed: $unit"}
}
