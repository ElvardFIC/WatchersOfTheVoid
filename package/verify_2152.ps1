$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent $root
$dll=Join-Path $root 'build/votv-coop-2152/main.dll'
$zip=Join-Path $root 'dist/WatchersOfTheVoid-0.9.2152.zip'
$logs=Join-Path $root 'build/release-2152-logs'
$tests=Get-Content (Join-Path $logs 'tests.json') -Raw | ConvertFrom-Json
if ($tests.Count -ne 7 -or @($tests | Where-Object ExitCode -ne 0).Count) { throw '7 passing focused regression runners required' }
$cache=Get-Content (Join-Path $root 'build/votv-coop-2152/CMakeCache.txt') -Raw
if($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$'){throw 'Release configuration required'}
. (Join-Path $workspace '_toolchain/env.ps1')
& dumpbin /imports $dll | Set-Content (Join-Path $logs 'imports.txt')
if($LASTEXITCODE -ne 0){throw 'DLL import inspection failed'}
& dumpbin /exports $dll | Set-Content (Join-Path $logs 'exports.txt')
if($LASTEXITCODE -ne 0){throw 'DLL export inspection failed'}
$version=[Diagnostics.FileVersionInfo]::GetVersionInfo($dll)
if ($version.ProductVersion -ne '0.9.0n b2152' -or $version.FileVersion -ne '0.9.0n b2152') { throw 'DLL version mismatch' }
$imports=Get-Content (Join-Path $logs 'imports.txt') -Raw
$exports=Get-Content (Join-Path $logs 'exports.txt') -Raw
if ($imports -notmatch 'KERNEL32.dll' -or $imports -match '(?im)^\s+(UE4SS|VCRUNTIME\d+|MSVCP\d+).*\.dll\s*$') { throw 'Unexpected DLL imports' }
if ($exports -notmatch '\bstart_mod\b' -or $exports -notmatch '\buninstall_mod\b') { throw 'Loader exports missing' }
$sourceRoot=Join-Path $root 'src/votv-coop'
$sourceList=Get-Content (Join-Path $sourceRoot 'sources.cmake') -Raw
$cpps=@(Get-ChildItem (Join-Path $sourceRoot 'src') -Filter '*.cpp' -Recurse -File)
foreach ($cpp in $cpps) {
    $relative=$cpp.FullName.Substring($sourceRoot.Length+1).Replace('\','/')
    if (-not $sourceList.Contains($relative)) { throw "Production source omitted from build: $relative" }
}
$dllHash=(Get-FileHash -LiteralPath $dll).Hash.ToLowerInvariant()
$zipHash=(Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant()
New-Item -ItemType Directory -Force -Path (Join-Path $workspace 'dist') | Out-Null
$outer=Join-Path $workspace 'dist/WatchersOfTheVoid-0.9.2152.zip'
Copy-Item -LiteralPath $zip -Destination $outer -Force
if ((Get-FileHash -LiteralPath $outer).Hash.ToLowerInvariant() -ne $zipHash) { throw 'Delivery copy mismatch' }
New-Item -ItemType Directory -Force -Path (Join-Path $workspace 'dist') | Out-Null
$sums="$zipHash  WatchersOfTheVoid-0.9.2152.zip`n$dllHash  mod/dlls/main.dll`n"
foreach ($dir in @((Join-Path $root 'dist'),(Join-Path $workspace 'dist'))) {
    [IO.File]::WriteAllText((Join-Path $dir 'SHA256SUMS-2152.txt'),$sums,[Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $dir 'SHA256SUMS.txt'),$sums,[Text.Encoding]::ASCII)
}
$inventory=@(Get-ChildItem (Join-Path $sourceRoot 'src'),(Join-Path $sourceRoot 'include') -Recurse -File |
    ForEach-Object { [pscustomobject]@{Path=$_.FullName.Substring($root.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$inventory | ConvertTo-Json | Set-Content (Join-Path $logs 'source-inventory.json') -Encoding UTF8
[pscustomobject]@{Release=2152;ProductVersion=$version.ProductVersion;Configuration='Release';ProductionCppFiles=$cpps.Count;PassingRunners=$tests.Count;DllSHA256=$dllHash;ZipSHA256=$zipHash;MultiplayerVerified=$false} |
    ConvertTo-Json | Set-Content (Join-Path $logs 'verification.json') -Encoding UTF8
Write-Output "PASS release2152: $($cpps.Count) production sources included, 7 focused runners, DLL identity/imports/exports, delivery hashes."
Write-Output "ZIP SHA256 $zipHash"
