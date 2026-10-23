[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param()

$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$workspace = [IO.Path]::GetFullPath((Join-Path $checkout '..')).TrimEnd('\')
$metadata = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release.json') -Raw | ConvertFrom-Json
$archiveName = '{0}-{1}-{2}.zip' -f $metadata.namespace, $metadata.name, $metadata.version_number
$archive = Join-Path $workspace "dist/$archiveName"
$dll = Join-Path $checkout $metadata.dll
$dependencies = Join-Path $checkout 'build/votv-coop/vcpkg_installed'
foreach ($required in @($archive, $dll, $dependencies)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required retained path missing: $required" }
}
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
$dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash

$targets = [Collections.Generic.List[string]]::new()
foreach ($relative in @(
    'build/votv-coop-2152', 'build/votv-coop-2153',
    '_research', 'build/package-current/stage',
    'build/package-current/rejected-dll-only.zip', 'garden_sync_test.obj',
    'agent/tests/__pycache__', 'build/requested-events-native/__pycache__',
    'build/votv-coop-2154/generated/l10n_packs.rc.tmp'
)) { $targets.Add((Join-Path $checkout $relative)) }
$targets.Add((Join-Path $workspace '_research/__pycache__'))

# Keep the packager's current output; remove only older releases and their staging.
$version = [regex]::Escape($metadata.version_number)
foreach ($item in Get-ChildItem -LiteralPath (Join-Path $checkout 'dist') -ErrorAction SilentlyContinue) {
    if ($item.Name -match '^stage-\d+$' -or
        ($item.Name -match '^WatchersOfTheVoid-.*\.zip$' -and $item.Name -notmatch $version) -or
        $item.Name -match '^SHA256SUMS-215[3-6]\.txt$') { $targets.Add($item.FullName) }
}
foreach ($item in Get-ChildItem -LiteralPath (Join-Path $workspace 'dist') -File) {
    if ($item.Name -match '^(WatchersOfTheVoid-0\.9\.215[2-6]\.zip|SHA256SUMS(?:-215[2-6])?\.txt|GITHUB_(?:DESCRIPTION|RELEASE)_215[3-6]\.(?:txt|md))$') {
        $targets.Add($item.FullName)
    }
}

# Preserve the active CMake tree and shared vcpkg installation in full.
# Elsewhere keep logs, JSON qualifications, source snapshots and native crash dumps.
$extensions = @('.exe', '.dll', '.obj', '.pdb', '.ilk', '.ipdb', '.iobj', '.exp', '.lib', '.tmp', '.bak', '.pyc')
foreach ($directory in Get-ChildItem -LiteralPath (Join-Path $checkout 'build') -Directory) {
    if ($directory.Name -in @('votv-coop', 'votv-coop-2152', 'votv-coop-2153', 'votv-coop-2154', 'package-current')) { continue }
    foreach ($file in Get-ChildItem -LiteralPath $directory.FullName -Recurse -File) {
        if ($file.Extension -in $extensions) { $targets.Add($file.FullName) }
    }
}

$existing = @($targets | Sort-Object -Unique | Where-Object { Test-Path -LiteralPath $_ })
# A selected directory owns its descendants; do not count or remove them twice.
$existing = @($existing | Where-Object {
    $candidate = $_
    -not ($existing | Where-Object {
        $_ -ne $candidate -and $candidate.StartsWith($_.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)
    })
})
# Validate the complete selection before performing any deletion.
foreach ($target in $existing) {
    $resolved = (Resolve-Path -LiteralPath $target).ProviderPath
    if (-not $resolved.StartsWith($workspace + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Cleanup target outside workspace: $resolved"
    }
    if ($resolved -eq $archive -or $resolved -eq $dll -or
        $archive.StartsWith($resolved + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $dll.StartsWith($resolved + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $dependencies.StartsWith($resolved + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Cleanup target contains retained files: $resolved"
    }
    $entries = @(Get-Item -LiteralPath $resolved -Force)
    if ($entries[0].PSIsContainer) { $entries += @(Get-ChildItem -LiteralPath $resolved -Recurse -Force) }
    if ($entries | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
        throw "Cleanup target contains a reparse point: $resolved"
    }
}
$files = 0; $bytes = 0L
foreach ($target in $existing) {
    $item = Get-Item -LiteralPath $target -Force
    $contents = if ($item.PSIsContainer) { @(Get-ChildItem -LiteralPath $target -Recurse -File -Force) } else { @($item) }
    $files += $contents.Count
    $bytes += ($contents | Measure-Object Length -Sum).Sum
    if ($PSCmdlet.ShouldProcess($target, 'Remove generated or obsolete artifacts')) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $archiveHash -or
    (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $dllHash) { throw 'Retained archive/DLL changed during cleanup' }
[pscustomobject]@{ Preview = [bool]$WhatIfPreference; Targets = $existing.Count; Files = $files; MiB = [math]::Round($bytes / 1MB, 1) }
