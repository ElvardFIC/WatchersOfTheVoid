$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2181'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/item_record_stubs /I src/votv-coop/include `
        "/Fo$output/birth.obj" "/Fe$output/birth.exe" agent/tests/pending_birth_2130_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Birth regression compile failed'}
    & "$output/birth.exe"
    if($LASTEXITCODE -ne 0){throw 'Birth regression failed'}
    & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include `
        "/Fo$output/prop_drop_intent.obj" src/votv-coop/src/coop/props/prop_drop_intent.cpp
    if($LASTEXITCODE -ne 0){throw 'Production compile failed'}
} finally {Pop-Location}
