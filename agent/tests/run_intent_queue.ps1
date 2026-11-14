param([string]$Case='all')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/intent-queue-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$root/src/votv-coop/include" "/Fo$output/" "/Fe$output/test.exe" "$PSScriptRoot/intent_queue_lifetime_test.cpp" "$root/src/votv-coop/src/coop/net/intent_queue_selftest.cpp"
if($LASTEXITCODE){throw 'Intent queue lifetime compile failed'}
& "$output/test.exe" $Case
if($LASTEXITCODE){throw 'Intent queue lifetime regression failed'}
