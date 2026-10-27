$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/item-icons-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') "$PSScriptRoot/item_icons_extract.py" $output
if($LASTEXITCODE){throw 'Item icon extraction/wiring failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$root/src/votv-coop/include" "/I$output" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/item_icons_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Item icon fixture compile failed'}
$sandbox=Join-Path $output ([Guid]::NewGuid().ToString('N'))
& "$output/test.exe" $sandbox
if($LASTEXITCODE){throw 'Item icon regression failed'}
