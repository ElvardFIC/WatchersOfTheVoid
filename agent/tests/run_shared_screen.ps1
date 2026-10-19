$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output = Join-Path $checkout 'build/shared-screen-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/shared_screen_wire_test.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Shared screen regression compile failed' }
& "$output/test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Shared screen regression failed' }
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/sleep.obj" "/Fe$output/sleep.exe" "$PSScriptRoot/sleep_display_test.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Sleep display regression compile failed' }
& "$output/sleep.exe"
if ($LASTEXITCODE -ne 0) { throw 'Sleep display regression failed' }
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$PSScriptRoot/shared_screen_stubs" `
    "/I$checkout/src/votv-coop/include" "/Fo$output/sync.obj" "/Fe$output/sync.exe" `
    "$PSScriptRoot/shared_screen_sync_test.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Shared screen lifecycle regression compile failed' }
& "$output/sync.exe"
if ($LASTEXITCODE -ne 0) { throw 'Shared screen lifecycle regression failed' }
