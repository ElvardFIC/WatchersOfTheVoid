param([int]$Parallel=4)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build/votv-coop-2154'
$logs=Join-Path $root 'build/release-2155-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
. (Join-Path $root '../_toolchain/env.ps1')
if(-not(Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt'))){throw 'Configure the existing Release build first; see BUILDING.md'}
$cache=Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
if($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$'){throw 'Release configuration required'}
# Reuse the canonical build directory; package identity is independent of its historical name.
$ErrorActionPreference='Continue'
& cmake --build $build --target votv-coop --parallel $Parallel *> (Join-Path $logs 'initial-build.log')
$buildCode=$LASTEXITCODE
$ErrorActionPreference='Stop'
if($buildCode -ne 0){throw 'Release build failed; see build/release-2155-logs/initial-build.log'}
$version=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $build 'main.dll')).ProductVersion
if($version -ne '0.9.0n b2178'){throw "Expected DLL b2178 for package 2155, got $version"}
Write-Output "PASS Release $version; running full qualification"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'check_current.ps1') -EvidenceDirectory 'build/continuation-check'
if($LASTEXITCODE -ne 0){throw 'Release qualification failed; see build/continuation-check'}
$report=Get-Content -LiteralPath (Join-Path $root 'build/continuation-check/verification.json') -Raw | ConvertFrom-Json
if($report.Protocol -ne 2178 -or $report.PassingGroups -ne 65 -or $report.Results.Count -ne 65){throw 'Expected 65 passing current groups and protocol 2178'}
Write-Output 'PASS package 2155 build qualification; multiplayer gameplay remains unverified'
