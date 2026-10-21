param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/balance-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    . (Join-Path $checkout '../_toolchain/env.ps1')
    & cl /nologo /std:c++20 /EHsc /utf-8 /I agent/tests/balance_stubs /I src/votv-coop/include "/Fo$output/balance.obj" "/Fe$output/balance.exe" agent/tests/balance_sync_test.cpp
    if($LASTEXITCODE -ne 0){throw 'Balance regression compile failed'}
    & "$output/balance.exe"
    if($LASTEXITCODE -ne 0){throw 'Balance regression failed'}
    if($CompileSources) {
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/production.obj" src/votv-coop/src/coop/world/balance_sync.cpp
        if($LASTEXITCODE -ne 0){throw 'Balance production compile failed'}
    }
} finally {Pop-Location}
