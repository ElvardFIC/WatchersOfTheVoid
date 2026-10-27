$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/game-modes-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    $python=Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
    & $python agent/tests/game_mode_host_extract.py $output
    if($LASTEXITCODE){throw 'Game-mode extraction/wiring failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/I$output" `
        "/Fo$output/modes.obj" "/Fe$output/modes.exe" agent/tests/game_mode_host_test.cpp /link /OPT:REF
    if($LASTEXITCODE){throw 'Game-mode fixture compile failed'}
    & "$output/modes.exe"
    if($LASTEXITCODE){throw 'Game-mode regression failed'}
    & $python agent/tests/game_mode_native_test.py
    if($LASTEXITCODE){throw 'Cooked game-mode contract failed'}
} finally {Pop-Location}
