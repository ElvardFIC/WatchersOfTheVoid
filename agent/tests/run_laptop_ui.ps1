param([switch]$CompileSources)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root 'build/laptop-ui-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        $environment = Join-Path $root '../_toolchain/env.ps1'
        if (-not (Test-Path -LiteralPath $environment)) { $environment = Join-Path $root '../_toolchain/env.ps1' }
        . $environment
    }
    foreach ($name in @('laptop_ui_sync','laptop_ui_wrapper')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/laptop_ui_stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Test compile failed: $name" }
        & "$output/$name.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $name" }
    }
    if ($CompileSources) {
        foreach ($file in @('coop/interactables/laptop_ui_sync', 'ue_wrap/devices/laptop_ui', 'coop/interactables/device_occupancy', 'coop/dispatch/event_dispatch_signal','ue_wrap/world/order_economy')) {
            $name = $file -replace '/', '_'
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if ($LASTEXITCODE -ne 0) { throw "Source compile failed: $file" }
        }
    }
} finally { Pop-Location }
