$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root '_research/tests_2174'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        . (Join-Path $root '../_toolchain/env.ps1')
    }
    & cl /nologo /std:c++20 /EHsc /utf-8 /Gy /I agent/tests/food_stubs `
        /I agent/tests/laptop_2171_stubs /I src/votv-coop/include `
        "/Fo$output/canned_refresh.obj" "/Fe$output/canned_refresh.exe" `
        agent/tests/canned_refresh_2174_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Canned-food regression compile failed' }
    & "$output/canned_refresh.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Canned-food regression failed' }
    & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include `
        "/Fo$output/prop_record_refresh.obj" src/votv-coop/src/coop/props/prop_record_refresh.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Production compile failed' }
} finally { Pop-Location }
