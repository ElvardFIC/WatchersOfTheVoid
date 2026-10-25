[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param()

$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$workspace = [IO.Path]::GetFullPath((Join-Path $checkout '..')).TrimEnd('\')
$metadata = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release.json') -Raw | ConvertFrom-Json
$archiveName = '{0}-{1}-{2}.zip' -f $metadata.namespace, $metadata.name, $metadata.version_number
$archive = Join-Path $checkout "dist/$archiveName"
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

# The checkout dist is canonical. Keep its exact current archive and checksum;
# discard superseded packages and duplicate delivery copies outside the checkout.
foreach ($directory in @((Join-Path $checkout 'dist'), (Join-Path $workspace 'dist'))) {
    foreach ($item in Get-ChildItem -LiteralPath $directory -ErrorAction SilentlyContinue) {
        if ($item.FullName -eq $archive -or
            $item.FullName -eq (Join-Path $checkout 'dist/SHA256SUMS-current.txt')) { continue }
        if ($item.Name -match '^stage-\d+$' -or
            $item.Name -match '^(?:ElvardFIC-)?WatchersOfTheVoid-\d+\.\d+\.\d+\.zip$' -or
            $item.Name -match '^SHA256SUMS(?:-[\w.-]+)?\.txt$' -or
            $item.Name -match '^GITHUB_(?:DESCRIPTION|RELEASE)_\d+\.(?:txt|md)$') {
            $targets.Add($item.FullName)
        }
    }
}
# These publication drafts are superseded by README, description, RELEASE and
# CHANGELOG_PLUS. Historical qualification reports and source handoffs stay intact.
foreach ($item in Get-ChildItem -LiteralPath $PSScriptRoot -File) {
    if ($item.Name -match '^GITHUB_(?:DESCRIPTION|RELEASE)_\d+\.(?:txt|md)$' -or
        $item.Name -match '^SHA256SUMS-\d+\.txt$') { $targets.Add($item.FullName) }
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
$inventory = [Collections.Generic.List[object]]::new()
foreach ($target in $existing) {
    $item = Get-Item -LiteralPath $target -Force
    $contents = if ($item.PSIsContainer) { @(Get-ChildItem -LiteralPath $target -Recurse -File -Force) } else { @($item) }
    $files += $contents.Count
    $bytes += ($contents | Measure-Object Length -Sum).Sum
    $inventory.Add([pscustomobject]@{
        path = $target.Substring($workspace.Length + 1).Replace('\', '/')
        files = $contents.Count
        bytes = [long](($contents | Measure-Object Length -Sum).Sum)
    })
    if ($PSCmdlet.ShouldProcess($target, 'Remove generated or obsolete artifacts')) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $archiveHash -or
    (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $dllHash) { throw 'Retained archive/DLL changed during cleanup' }
if (-not $WhatIfPreference -and $existing.Count) {
    $inventoryPath = Join-Path $PSScriptRoot 'cleanup-inventory.json'
    $report = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
    $report | Add-Member -Force -NotePropertyName latestRun -NotePropertyValue ([pscustomobject]@{
        dateUtc = [DateTime]::UtcNow.ToString('o')
        retainedPackage = $metadata.version_number
        archive = $archive.Substring($workspace.Length + 1).Replace('\', '/')
        archiveSHA256 = $archiveHash
        dllSHA256 = $dllHash
        files = $files
        bytes = $bytes
        targets = @($inventory.ToArray())
    })
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $inventoryPath -Encoding UTF8
}
[pscustomobject]@{ Preview = [bool]$WhatIfPreference; Targets = $existing.Count; Files = $files; MiB = [math]::Round($bytes / 1MB, 1) }
