param([switch]$Apply)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$workspace=Split-Path -Parent $root
$archive=Join-Path $workspace 'archive/release-2150-cleanup'
$journal=Join-Path $workspace 'archive/cleanup-2150.json'
$actions=New-Object 'System.Collections.Generic.List[object]'
function Checked([string]$path) {
    $full=[IO.Path]::GetFullPath($path)
    if (-not $full.StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase)) { throw "Outside workspace: $full" }
    $probe=$full
    while ($probe -and $probe -ne $workspace) {
        if ((Test-Path -LiteralPath $probe) -and ((Get-Item -LiteralPath $probe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Reparse point: $probe" }
        $probe=Split-Path -Parent $probe
    }
    return $full
}
function Record([string]$action,[string]$path,[string]$destination='') {
    $full=Checked $path
    $item=Get-Item -LiteralPath $full
    if ($item.PSIsContainer) {
        $files=@(Get-ChildItem -LiteralPath $full -Recurse -File -Force)
        if (@(Get-ChildItem -LiteralPath $full -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw "Nested link: $full" }
        $bytes=($files | Measure-Object Length -Sum).Sum
        $hash='generated-directory'
    } else { $bytes=$item.Length; $hash=(Get-FileHash -LiteralPath $full).Hash }
    $actions.Add([pscustomobject]@{Action=$action;Path=$full;Destination=$destination;Bytes=$bytes;SHA256=$hash})
}
foreach ($dist in @((Join-Path $workspace 'dist'),(Join-Path $root 'dist'))) {
    foreach ($zip in @(Get-ChildItem -LiteralPath $dist -Filter '*.zip' -File)) {
        if ($zip.Name -ne 'WatchersOfTheVoid-0.9.2150.zip') {
            Record 'archive-file' $zip.FullName (Join-Path $workspace ('dist/archive/'+$zip.Name))
        }
    }
    $stage=Join-Path $dist 'stage'
    if (Test-Path -LiteralPath $stage) { Record 'delete-generated' $stage }
}
# Preserve previous DLL bytes before dropping obsolete build caches. Keep vcpkg dependencies.
foreach ($buildName in @('votv-coop','votv-coop-2168')) {
    $old=Join-Path $root ('build/'+$buildName)
    $dll=Join-Path $old 'main.dll'
    if (Test-Path -LiteralPath $dll) { Record 'preserve-file' $dll (Join-Path $archive ($buildName+'/main.dll')) }
    if (Test-Path -LiteralPath $old) {
        foreach ($entry in @(Get-ChildItem -LiteralPath $old -Force)) {
            if ($entry.Name -ne 'vcpkg_installed') { Record 'delete-generated' $entry.FullName }
        }
    }
}
# Compiled test products only: research scripts, dumps, logs and baselines stay intact.
foreach ($research in @((Join-Path $workspace '_research'),(Join-Path $root '_research'))) {
    $files=@(Get-ChildItem -LiteralPath $research -File -Force)
    foreach ($dir in @(Get-ChildItem -LiteralPath $research -Directory | Where-Object { $_.Name -match '^(tests_\d+|ragdoll_diagnostics)$' })) {
        $files+=Get-ChildItem -LiteralPath $dir.FullName -Recurse -File -Force
    }
    foreach ($file in $files) {
        if ($file.Extension -in @('.obj','.exe','.pdb','.ilk','.exp','.lib','.res')) { Record 'delete-generated' $file.FullName }
    }
}
if (-not $Apply) {
    $actions | Group-Object Action | ForEach-Object { [pscustomobject]@{Action=$_.Name;Entries=$_.Count;MiB=[math]::Round(($_.Group|Measure-Object Bytes -Sum).Sum/1MB,1)} }
    return
}
$dll=Join-Path $root 'build/votv-coop-2150/main.dll'
if ([Diagnostics.FileVersionInfo]::GetVersionInfo($dll).ProductVersion -ne '0.9.0n b2150') { throw 'Verified new DLL required before cleanup' }
if (-not (Test-Path -LiteralPath (Join-Path $root 'dist/WatchersOfTheVoid-0.9.2150.zip'))) { throw 'New package required before cleanup' }
if (Test-Path -LiteralPath $journal) { throw 'Cleanup journal already exists; inspect before rerunning.' }
# Persist the plan before destructive work, then mark each completed action.
$actions | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $journal -Encoding UTF8
foreach ($action in $actions) {
    $path=Checked $action.Path
    if ($action.Action -in @('archive-file','preserve-file')) {
        $dest=Checked $action.Destination
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
        if (Test-Path -LiteralPath $dest) {
            if ((Get-FileHash -LiteralPath $dest).Hash -ne $action.SHA256) { throw "Archive conflict: $dest" }
        } else { Copy-Item -LiteralPath $path -Destination $dest }
        if ((Get-FileHash -LiteralPath $dest).Hash -ne $action.SHA256) { throw 'Archive verification failed' }
        if ($action.Action -eq 'archive-file') { Remove-Item -LiteralPath $path }
    } else {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
    $action | Add-Member NoteProperty Completed $true
}
$actions | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $journal -Encoding UTF8
$removed=($actions | Where-Object Action -eq 'delete-generated' | Measure-Object Bytes -Sum).Sum
Write-Output "Cleanup complete: $($actions.Count) actions, $([math]::Round($removed/1MB,1)) MiB generated artifacts removed."
