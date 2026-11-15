param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/event-sync-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not(Get-Command cl -ErrorAction SilentlyContinue)) { . (Join-Path $root '../_toolchain/env.ps1') }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/config_registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Config registry compile failed' }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include /c "/Fo$output/config_registry_log.obj" "$PSScriptRoot/config_registry_log_stub.cpp"
    if ($LASTEXITCODE -ne 0) { throw 'Config registry log stub compile failed' }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include /c "/Fo$output/config_registry_flag.obj" "$PSScriptRoot/config_registry_flag_stub.cpp"
    if ($LASTEXITCODE -ne 0) { throw 'Config registry flag stub compile failed' }
    foreach ($testName in @('event_sync','event_rate','event_active_lifetime','event_cue')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I "$PSScriptRoot/event_stubs" /I "$PSScriptRoot/location_2161_stubs" /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "$PSScriptRoot/${testName}_test.cpp" "$output/config_registry.obj" "$output/config_registry_log.obj" "$output/config_registry_flag.obj" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Test compile failed: $testName" }
        & "$output/$testName.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $testName" }
    }
    if ($CompileSources) {
        foreach ($file in @('coop/world/event_rate','coop/world/event_fire_sync','coop/world/event_fire_policy','coop/world/event_fire_delivery','coop/world/event_active_sync','coop/world/event_cue_sync')) {
            $name=$file -replace '/','_'
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if ($LASTEXITCODE -ne 0) { throw "Source compile failed: $file" }
        }
    }
    & (Join-Path $PSScriptRoot 'run_serverbox.ps1') -CompileSources:$CompileSources
    & (Join-Path $PSScriptRoot 'run_event_slots.ps1') -CompileSources:$CompileSources
} finally { Pop-Location }
