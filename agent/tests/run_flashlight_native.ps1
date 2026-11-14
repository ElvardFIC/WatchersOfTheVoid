$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/flashlight-native-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    $python=Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
    & $python agent/tests/flashlight_dispatch_extract.py "$output/flashlight_dispatch_production.inc"
    if($LASTEXITCODE){throw 'Flashlight dispatcher extraction failed'}
    foreach($test in @('flashlight_native_frame','flashlight_reader','flashlight_dispatch')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/I$output" `
            "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE){throw "Flashlight fixture compilation failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE){throw "Flashlight regression failed: $test"}
    }
} finally {Pop-Location}
