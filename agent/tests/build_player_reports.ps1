$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$log=Join-Path $checkout 'build/player-reports-check/full-build.log'
New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
$ErrorActionPreference='Continue' # Native compiler/CMake warnings do not fail the build.
& cmake --build (Join-Path $checkout 'build/votv-coop-2150') --target votv-coop --parallel 4 *> $log
if($LASTEXITCODE -ne 0){Get-Content $log -Tail 55;throw "Full build failed: $log"}
Write-Output "PASS complete current-source DLL build: $log"
