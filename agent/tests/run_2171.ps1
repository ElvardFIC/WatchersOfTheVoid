$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root '_research/tests_2171'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        . (Join-Path $root '../_toolchain/env.ps1')
    }
    & cl /nologo /std:c++20 /EHsc /utf-8 `
        /I agent/tests/laptop_2171_stubs `
        /I src/votv-coop/include `
        "/Fo$output/laptop_power_2171_test.obj" `
        "/Fe$output/laptop_power_2171_test.exe" `
        agent/tests/laptop_power_2171_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Laptop power test compile failed' }
    & "$output/laptop_power_2171_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Laptop power test failed' }

    & cl /nologo /std:c++20 /EHsc /utf-8 /c `
        /I src/votv-coop/include `
        "/Fo$output/laptop_sync.obj" `
        src/votv-coop/src/coop/interactables/laptop_sync.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Laptop sync production compile failed' }
} finally {
    Pop-Location
}
