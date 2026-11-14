$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/local-release-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/local_release_extract.py" "$output/release_edge.cpp"
if($LASTEXITCODE){throw 'Release edge extraction failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/I$root/src/votv-coop/include" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/local_release_test.cpp"
if($LASTEXITCODE){throw 'Release edge compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Release edge regression failed'}
