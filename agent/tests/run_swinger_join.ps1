param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/swinger-join-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
. (Join-Path $root '../_toolchain/env.ps1')
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/stubs" "/I$root/src/votv-coop/include" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/swinger_join_lifetime_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Swinger fixture compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Swinger fixture failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$root/src/votv-coop/include" "/Fo$output/riders.obj" "/Fe$output/riders.exe" "$PSScriptRoot/park_riders_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Rider fixture compile failed'}
& "$output/riders.exe"
if($LASTEXITCODE){throw 'Rider fixture failed'}
