param(
    [string]$BuildDirectory='build/votv-coop-2154',
    [string]$EvidenceDirectory=''
)
$ErrorActionPreference='Stop'
$checkout=Split-Path -Parent $PSScriptRoot
$source=Join-Path $checkout 'src/votv-coop'
$protocolText=Get-Content -LiteralPath (Join-Path $source 'include/coop/net/protocol.h') -Raw
$protocolMatch=[regex]::Match($protocolText,'kProtocolVersion\s*=\s*(\d+)\s*;')
if(-not $protocolMatch.Success){throw 'Cannot resolve current source protocol'}
$protocol=[int]$protocolMatch.Groups[1].Value
$cmake=Get-Content -LiteralPath (Join-Path $source 'CMakeLists.txt') -Raw
$gameMatch=[regex]::Match($cmake,'set\(VOTVCOOP_GAME_TARGET\s+"([^"]+)"\)')
if(-not $gameMatch.Success){throw 'Cannot resolve game target'}
$expectedVersion=$gameMatch.Groups[1].Value+' b'+$protocol
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $checkout $BuildDirectory}
$dll=Join-Path $build 'main.dll'
$cache=Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
if($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$'){throw 'Release build required'}
$version=[Diagnostics.FileVersionInfo]::GetVersionInfo($dll)
if($version.ProductVersion -ne $expectedVersion -or $version.FileVersion -ne $expectedVersion){
    throw "DLL/source identity mismatch: expected $expectedVersion, got $($version.ProductVersion) / $($version.FileVersion)"
}
if(-not $EvidenceDirectory){$EvidenceDirectory="build/entity-sync-check/validation-$protocol"}
$evidence=if([IO.Path]::IsPathRooted($EvidenceDirectory)){$EvidenceDirectory}else{Join-Path $checkout $EvidenceDirectory}
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
if(-not(Get-Command dumpbin -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
$imports=& dumpbin /imports $dll
if($LASTEXITCODE -ne 0){throw 'DLL import inspection failed'}
$exports=& dumpbin /exports $dll
if($LASTEXITCODE -ne 0){throw 'DLL export inspection failed'}
$imports | Set-Content (Join-Path $evidence 'imports.txt')
$exports | Set-Content (Join-Path $evidence 'exports.txt')
if(($imports -join "`n") -notmatch 'KERNEL32.dll' -or ($imports -join "`n") -match '(?im)^\s+(UE4SS|VCRUNTIME\d+|MSVCP\d+).*\.dll\s*$'){throw 'Unexpected DLL imports'}
if(($exports -join "`n") -notmatch '\bstart_mod\b' -or ($exports -join "`n") -notmatch '\buninstall_mod\b'){throw 'Loader exports missing'}
$entries=@([regex]::Matches((Get-Content (Join-Path $source 'sources.cmake') -Raw),'(?m)^\s*(src/[^\s]+\.cpp)\s*$'))
if(-not $entries.Count){throw 'Build source list is empty'}
foreach($entry in $entries){if(-not(Test-Path -LiteralPath (Join-Path $source $entry.Groups[1].Value))){throw "Missing build source: $($entry.Value)"}}
$report=[pscustomobject]@{
    Protocol=$protocol;ProductVersion=$version.ProductVersion;Configuration='Release';Dll=$dll
    DllSHA256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
    BuildSourceEntries=$entries.Count;TwoPlayerGameplayVerified=$false;Packaged=$false
}
$report | ConvertTo-Json | Set-Content (Join-Path $evidence 'identity.json') -Encoding UTF8
Write-Output "PASS current Release $expectedVersion; $($entries.Count) source entries; SHA256 $($report.DllSHA256)"
