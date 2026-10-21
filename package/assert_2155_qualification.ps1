$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Get-Content -LiteralPath (Join-Path $root 'build/continuation-check/verification.json') -Raw | ConvertFrom-Json
if($report.Protocol -ne 2178 -or $report.ProductVersion -ne '0.9.0n b2178' -or
   $report.Configuration -ne 'Release' -or -not $report.BuildUpToDate -or
   $report.PassingGroups -ne 65 -or $report.Results.Count -ne 65 -or
   @($report.Results | Where-Object ExitCode -ne 0).Count){throw 'Package 2155 requires complete b2178 Release qualification'}
$dll=Join-Path $root 'build/votv-coop-2154/main.dll'
$version=[Diagnostics.FileVersionInfo]::GetVersionInfo($dll)
if($version.ProductVersion -ne $report.ProductVersion -or $version.FileVersion -ne $report.ProductVersion -or
   (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant() -ne $report.DllSHA256){throw 'Qualified DLL no longer matches the build output'}
$production=Join-Path $root 'src/votv-coop'
$files=@(Get-ChildItem -LiteralPath (Join-Path $production 'include'),(Join-Path $production 'src') -Recurse -File |
    Where-Object { $_.Extension -in @('.h','.hpp','.cpp','.inc','.rc') })
$files+=Get-Item -LiteralPath (Join-Path $production 'CMakeLists.txt'),(Join-Path $production 'sources.cmake')
$rows=foreach($file in ($files | Sort-Object FullName)) {
    $file.FullName.Substring($production.Length)+':'+(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
}
$sha=[Security.Cryptography.SHA256]::Create()
try { $stamp=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows -join "`n"))))).Replace('-','').ToLowerInvariant() }
finally {$sha.Dispose()}
if($stamp -ne $report.SourceSnapshotSHA256){throw 'Production inputs changed after qualification; rebuild and rerun package/build_2155.ps1'}
return $report
