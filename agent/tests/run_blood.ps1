$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/bleeding-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$PSScriptRoot/blood_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/controller.obj" "/Fe$output/controller.exe" "$PSScriptRoot/blood_sync_test.cpp"
if($LASTEXITCODE){throw 'Blood regression compile failed'}
& "$output/controller.exe"
if($LASTEXITCODE){throw 'Blood regression failed'}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/blood_native_test.py"
if($LASTEXITCODE){throw 'Native blood contract failed'}
