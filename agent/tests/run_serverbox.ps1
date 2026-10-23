param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not(Get-Command cl -ErrorAction SilentlyContinue)) { . (Join-Path $checkout '../_toolchain/env.ps1') }
$output=Join-Path $checkout 'build/requested-events-audit'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/serverbox_stubs" "/I$checkout/src/votv-coop/include" "/Fo$output/serverbox_sync_test.obj" "/Fe$output/serverbox_sync_test.exe" "$PSScriptRoot/serverbox_sync_test.cpp" /link /OPT:REF
if ($LASTEXITCODE -ne 0) {throw 'Serverbox regression compile failed'}
& "$output/serverbox_sync_test.exe"
if ($LASTEXITCODE -ne 0) {throw 'Serverbox regression failed'}
if ($CompileSources) {
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" "/Fo$output/serverbox_sync.obj" "$checkout/src/votv-coop/src/coop/interactables/serverbox_sync.cpp"
    if ($LASTEXITCODE -ne 0) {throw 'Serverbox production compile failed'}
}
