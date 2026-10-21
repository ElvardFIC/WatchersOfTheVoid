param([switch]$CompileSources)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root 'build/garden-sync-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        $environment = Join-Path $root '../_toolchain/env.ps1'
        if (-not (Test-Path -LiteralPath $environment)) { $environment = Join-Path $root '../_toolchain/env.ps1' }
        . $environment
    }
    foreach ($testName in @('garden_sync', 'garden_record', 'garden_wrapper', 'garden_actions', 'hose_route', 'hose_link', 'wall_enrollment_2167', 'care_invocation', 'garden_owner')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/placed_save_stubs /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Test compile failed: $testName" }
        & "$output/$testName.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $testName" }
    }
    if ($CompileSources) {
    foreach ($file in @('coop/world/placed_save_sync', 'coop/world/garden_actions', 'ue_wrap/actors/placed_save_record', 'ue_wrap/actors/placed_save', 'ue_wrap/facts/function_row', 'ue_wrap/core/ufunction_hook')) {
            $name = $file -replace '/', '_'
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if ($LASTEXITCODE -ne 0) { throw "Source compile failed: $file" }
        }
    }
} finally { Pop-Location }
