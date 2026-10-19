$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2182'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /utf-8 /I src/votv-coop/include `
        "/Fo$output/birth.obj" "/Fe$output/birth.exe" agent/tests/local_birth_2182_test.cpp
    if($LASTEXITCODE -ne 0){throw 'Protection regression compile failed'}
    & "$output/birth.exe"
    if($LASTEXITCODE -ne 0){throw 'Protection regression failed'}
    & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include `
        "/Fo$output/join_membership_sweep.obj" src/votv-coop/src/coop/props/join_membership_sweep.cpp
    if($LASTEXITCODE -ne 0){throw 'Production compile failed'}
} finally {Pop-Location}
