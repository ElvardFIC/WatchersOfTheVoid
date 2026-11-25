$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root '_research/tests_2179'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) { . (Join-Path $root '../_toolchain/env.ps1') }
    & cl /nologo /std:c++20 /EHsc /utf-8 /Gy /I "$PSScriptRoot/state_2145_stubs" `
        /I "$PSScriptRoot/laptop_2171_stubs" /I src/votv-coop/include `
        "/Fo$output/channel.obj" "/Fe$output/channel.exe" "$PSScriptRoot/channel_2179_test.cpp" /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Channel test compile failed' }
    & "$output/channel.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Channel test failed' }
    & "$PSScriptRoot/run_2177.ps1"
} finally { Pop-Location }
