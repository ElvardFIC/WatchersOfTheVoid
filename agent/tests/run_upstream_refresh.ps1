$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/upstream-refresh-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    & "$env:USERPROFILE/.local/bin/python3.15.exe" agent/tests/upstream_refresh_extract.py "$output/upstream_refresh_wire.inc"
    if($LASTEXITCODE){throw 'Packet builder extraction failed'}
    & cl /nologo /std:c++20 /EHsc /utf-8 /I agent/tests/upstream_refresh_stubs /I src/votv-coop/include "/I$output" "/Fo$output/test.obj" "/Fe$output/test.exe" agent/tests/upstream_refresh_test.cpp
    if($LASTEXITCODE){throw 'Upstream refresh fixture compile failed'}
    & "$output/test.exe"
    if($LASTEXITCODE){throw 'Upstream refresh regression failed'}
} finally {Pop-Location}
