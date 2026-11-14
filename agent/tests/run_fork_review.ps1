$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/fork-review-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
# The power fixture executes production registration and readiness checks;
# its doubles reject every owner except the native generator_C.
& cl /nologo /std:c++20 /EHsc /Gy /Gw /utf-8 "/I$PSScriptRoot/stubs" `
    "/I$checkout/src/votv-coop/include" "/I$checkout/src/votv-coop/src" `
    "/Fo$output/power.obj" "/Fe$output/power.exe" "$PSScriptRoot/power_door_2126_test.cpp" /link /OPT:REF /OPT:ICF
if($LASTEXITCODE){throw 'Fork generator fixture compile failed'}
& "$output/power.exe"
if($LASTEXITCODE){throw 'Fork generator fixture failed'}
foreach($runner in @('run_2157','run_laptop_ui','run_shared_screen')) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/$runner.ps1"
    if($LASTEXITCODE){throw "Fork adaptation regression failed: $runner"}
}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/entity_sync_native_test.py"
if($LASTEXITCODE){throw 'Fork native class/spawn evidence failed'}
