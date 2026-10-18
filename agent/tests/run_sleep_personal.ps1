$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output = Join-Path $checkout 'build/sleep-personal-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$PSScriptRoot/sleep_stubs" `
    "/I$checkout/src/votv-coop/include" "/Fo$output/test.obj" "/Fe$output/test.exe" `
    "$PSScriptRoot/sleep_personal_test.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Personal sleep regression compile failed' }
& "$output/test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Personal sleep regression failed' }
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
    "/Fo$output/sleep_sync.obj" "$checkout/src/votv-coop/src/coop/player/sleep_sync.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Production sleep_sync compilation failed' }
