$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$workspace = Split-Path -Parent $root
$dll = Join-Path $root 'build/votv-coop-2153/main.dll'
$zip = Join-Path $root 'dist/WatchersOfTheVoid-0.9.2153.zip'
$notes = Join-Path $PSScriptRoot 'GITHUB_RELEASE_2153.md'
$logs = Join-Path $root 'build/release-2153-logs'
$tests = Get-Content (Join-Path $logs 'tests.json') -Raw | ConvertFrom-Json
$failed = @($tests | Where-Object { -not $_.Skipped -and $_.ExitCode -ne 0 })
$passed = @($tests | Where-Object { -not $_.Skipped })
if ($passed.Count -ne 4 -or $failed.Count -ne 0) { throw 'Expected four passing focused regression groups and no failed checks' }
$skipped = @($tests | Where-Object Skipped)
if ($skipped.Count -ne 1 -or $skipped[0].Runner -ne 'sync_native_contract_test.py') { throw 'Native-asset audit status is missing or unexpected' }
$cache = Get-Content (Join-Path $root 'build/votv-coop-2153/CMakeCache.txt') -Raw
if ($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$') { throw 'Release configuration required' }
. (Join-Path $workspace '_toolchain/env.ps1')
& dumpbin /imports $dll | Set-Content (Join-Path $logs 'imports.txt')
if ($LASTEXITCODE -ne 0) { throw 'DLL import inspection failed' }
& dumpbin /exports $dll | Set-Content (Join-Path $logs 'exports.txt')
if ($LASTEXITCODE -ne 0) { throw 'DLL export inspection failed' }
$version = [Diagnostics.FileVersionInfo]::GetVersionInfo($dll)
if ($version.ProductVersion -ne '0.9.0n b2156' -or $version.FileVersion -ne '0.9.0n b2156') { throw 'DLL version mismatch (package 2153 requires protocol/DLL 2156)' }
$protocolSource = Get-Content (Join-Path $root 'src/votv-coop/include/coop/net/protocol.h') -Raw
if ($protocolSource -notmatch 'kProtocolVersion\s*=\s*2156\s*;') { throw 'Source protocol mismatch' }
$imports = Get-Content (Join-Path $logs 'imports.txt') -Raw
$exports = Get-Content (Join-Path $logs 'exports.txt') -Raw
if ($imports -notmatch 'KERNEL32.dll' -or $imports -match '(?im)^\s+(UE4SS|VCRUNTIME\d+|MSVCP\d+).*\.dll\s*$') { throw 'Unexpected DLL imports' }
if ($exports -notmatch '\bstart_mod\b' -or $exports -notmatch '\buninstall_mod\b') { throw 'Loader exports missing' }
$sourceRoot = Join-Path $root 'src/votv-coop'
$sourceList = Get-Content (Join-Path $sourceRoot 'sources.cmake') -Raw
$sourcePaths = @([regex]::Matches($sourceList, '(?m)^\s*(src/[^\s]+\.cpp)\s*$') | ForEach-Object { $_.Groups[1].Value })
foreach ($relative in $sourcePaths) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $relative.Replace('/','\')))) { throw "Listed build source is missing: $relative" }
}
if (-not (Test-Path -LiteralPath $zip) -or -not (Test-Path -LiteralPath $notes)) { throw 'Release ZIP or GitHub notes are missing' }
$zipHash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
$dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
$delivery = Join-Path $workspace 'dist'
New-Item -ItemType Directory -Force -Path $delivery | Out-Null
$outerZip = Join-Path $delivery 'WatchersOfTheVoid-0.9.2153.zip'
$outerNotes = Join-Path $delivery 'GITHUB_RELEASE_2153.md'
Copy-Item -LiteralPath $zip -Destination $outerZip -Force
Copy-Item -LiteralPath $notes -Destination $outerNotes -Force
if ((Get-FileHash -LiteralPath $outerZip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $zipHash) { throw 'Delivery ZIP copy mismatch' }
$sums = "$zipHash  WatchersOfTheVoid-0.9.2153.zip`n$dllHash  mod/dlls/main.dll`n"
[IO.File]::WriteAllText((Join-Path $delivery 'SHA256SUMS-2153.txt'), $sums, [Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $root 'dist/SHA256SUMS-2153.txt'), $sums, [Text.Encoding]::ASCII)
$report = [pscustomobject]@{ Release = 2153; PackageVersion = '0.9.2153'; Protocol = 2156; ProductVersion = $version.ProductVersion; Configuration = 'Release'; BuildSourceEntries = $sourcePaths.Count; PassingRegressionGroups = $passed.Count; SkippedNativeAssetAudit = $true; TwoPlayerGameplayVerified = $false; DllSHA256 = $dllHash; ZipSHA256 = $zipHash }
$report | ConvertTo-Json | Set-Content (Join-Path $logs 'verification.json') -Encoding UTF8
Write-Output "PASS release 2153: $($sourcePaths.Count) build source entries, $($passed.Count) regression groups, protocol 2156, DLL identity/imports/exports and delivery hashes."
Write-Output "SKIPPED native asset audit: extracted 0.9.0n game assets unavailable."
Write-Output "ZIP SHA256 $zipHash"
