param([switch]$VerifyOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
. (Join-Path $root '.github/ci/ledger_lib.ps1')
$release = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release.json') -Raw | ConvertFrom-Json
if ($release.name -ne 'WatchersOfTheVoid' -or $release.namespace -ne 'ElvardFIC' -or
    $release.version_number -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid fork package identity' }
$identity = "$($release.namespace)-$($release.name)"
$archiveName = "$identity-$($release.version_number).zip"
$zipPath = Join-Path $root "dist/$archiveName"
$stage = Join-Path $root 'build/package-current/stage'
$logs = Join-Path $root 'build/package-current'
$dll = Join-Path $root $release.dll
$qualification = Get-Content -LiteralPath (Join-Path $root $release.qualification) -Raw | ConvertFrom-Json
$snapshot = @(Get-Content -LiteralPath (Join-Path $root $release.source_snapshot) -Raw | ConvertFrom-Json)
$expectedVersion = "$($release.game_target) b$($release.protocol)"
$dllInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($dll)
$dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
if ($dllInfo.ProductVersion -ne $expectedVersion -or $dllInfo.ProductName -ne 'Watchers of the Void' -or
    $dllInfo.CompanyName -ne 'ElvardFIC' -or $qualification.build -ne $release.protocol -or
    $qualification.game -ne $release.game_target -or $qualification.dllSha256 -ne $dllHash -or
    -not $qualification.sourceUnchanged -or -not $qualification.peerDllsIdentical -or
    $qualification.live.scientistClientToHost -notlike 'PASS*' -or
    $qualification.live.kerfurHostToClient -notlike 'PASS*') { throw 'DLL differs from the qualified fork build' }
$protocol = Get-ProtoFromWorktree -RepoRoot $root
$game = Get-GameTargetFromCMake -CMakePath (Join-Path $root 'src/votv-coop/CMakeLists.txt')
if ($protocol -ne $release.protocol -or $game -ne $release.game_target) { throw 'Source identity differs from release.json' }
$recorded = @{}
foreach ($entry in $snapshot) {
    $path = Join-Path $root $entry.path
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        throw "Production changed after qualification: $($entry.path)"
    }
    $recorded[$entry.path.Replace('\','/')] = $true
}
foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $root 'src/votv-coop/src'),(Join-Path $root 'src/votv-coop/include') -Recurse -File)) {
    $relative = $file.FullName.Substring($root.Length + 1).Replace('\','/')
    if (-not $recorded.ContainsKey($relative)) { throw "Unqualified production input: $relative" }
}
$description = 'Watchers of the Void by ElvardFIC and AquaFishhh. A Multivoid fork for VotV 0.9.0n with co-op synchronization improvements and selected-character ragdolls. Original Multivoid by Pelmentor.'
$manifest = [ordered]@{
    name = $release.name; version_number = $release.version_number
    website_url = 'https://github.com/ElvardFIC/WatchersOfTheVoid'
    description = $description
    dependencies = @('Thunderstore-unreal_shimloader-1.1.7')
}
if ($description.Length -gt 250) { throw 'Description exceeds the package limit' }
$files = @{
    'README.md' = 'package/README.md'; 'CHANGELOG.md' = 'package/CHANGELOG_PLUS.md'
    'RELEASE.md' = 'package/RELEASE.md'; 'LICENSE' = 'LICENSE'
    'THIRD-PARTY-NOTICES.md' = 'THIRD-PARTY-NOTICES.md'; 'icon.png' = 'assets/branding/icon.png'
    'mod/dlls/main.dll' = $release.dll
}
foreach ($asset in (Get-ChildItem -LiteralPath (Join-Path $root 'assets/paks') -File | Where-Object Extension -in '.pak','.png')) {
    $files['pak/' + $asset.Name] = 'assets/paks/' + $asset.Name
}
if (-not $files.ContainsKey('pak/scientists.pak')) { throw 'Starter models missing' }
if (-not $VerifyOnly) {
    # Recreate only this package's staging directory, inside this checkout.
    $expected = [IO.Path]::GetFullPath((Join-Path $root 'build/package-current/stage'))
    if ($stage -ne $expected) { throw 'Unexpected stage path' }
    if (Test-Path -LiteralPath $stage) {
        $resolved = (Resolve-Path -LiteralPath $stage).Path
        if ($resolved -ne $expected -or (Get-Item -LiteralPath $stage).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Refusing an unexpected staging directory'
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $stage,(Join-Path $root 'dist') | Out-Null
    foreach ($name in $files.Keys) {
        $destination = Join-Path $stage $name
        New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
        Copy-Item -LiteralPath (Join-Path $root $files[$name]) -Destination $destination
    }
    [IO.File]::WriteAllText((Join-Path $stage 'manifest.json'), ($manifest | ConvertTo-Json -Depth 4), [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $stage 'mod/enabled.txt'), 'true', [Text.Encoding]::ASCII)
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -Force -CompressionLevel Optimal
}
$violations = @(Test-PackageZip -ZipPath $zipPath -ExpectedPayloadSha256 $dllHash)
if ($violations.Count) { throw ($violations -join "`n") }
$expectedEntries = @($files.Keys) + @('manifest.json','mod/enabled.txt')
$zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
function ReadEntry($Entry) {
    $reader = [IO.StreamReader]::new($Entry.Open())
    try { $reader.ReadToEnd() } finally { $reader.Dispose() }
}
function EntryHash($Entry) {
    $sha = [Security.Cryptography.SHA256]::Create(); $stream = $Entry.Open()
    try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose(); $sha.Dispose() }
}
try {
    $entries = @{}
    foreach ($entry in $zip.Entries) {
        $name = $entry.FullName.Replace('\','/')
        if ($name.EndsWith('/')) { continue }
        if ($entries.ContainsKey($name) -or $expectedEntries -cnotcontains $name) { throw "Unexpected archive entry: $name" }
        $entries[$name] = $entry
    }
    if ($entries.Count -ne $expectedEntries.Count) { throw 'Archive is incomplete' }
    foreach ($name in $files.Keys) {
        if ((EntryHash $entries[$name]) -ne (Get-FileHash -LiteralPath (Join-Path $root $files[$name]) -Algorithm SHA256).Hash.ToLowerInvariant()) {
            throw "Archive differs from source: $name"
        }
    }
    $written = ReadEntry $entries['manifest.json'] | ConvertFrom-Json
    if ($written.name -ne $release.name -or $written.version_number -ne $release.version_number -or
        $written.website_url -ne $manifest.website_url -or $written.description -ne $description -or
        $written.dependencies.Count -ne 1 -or $written.dependencies[0] -ne $manifest.dependencies[0]) { throw 'Manifest identity mismatch' }
    if ((ReadEntry $entries['mod/enabled.txt']) -cne 'true') { throw 'Loader activation marker is missing' }
    if ((ReadEntry $entries['CHANGELOG.md']).Split("`n")[0].Trim() -ne "## $($release.version_number)") { throw 'Changelog version mismatch' }
    if ((ReadEntry $entries['README.md']) -notlike "*$($release.version_number)*" -or
        (ReadEntry $entries['README.md']) -notlike '*AquaFishhh*') { throw 'Package page is stale' }
} finally { $zip.Dispose() }
$icon = Get-PngDimensions -Path (Join-Path $root 'assets/branding/icon.png')
if ($icon.Width -ne 256 -or $icon.Height -ne 256) { throw 'Package icon must be 256x256 PNG' }
# The actual broken delivery is a regression control for the routing predicate.
$oldZip = Join-Path $root 'dist/WatchersOfTheVoid-ragdoll-b2179.zip'
if (-not (Test-Path -LiteralPath $oldZip)) {
    $oldZip = Join-Path $root 'build/package-current/rejected-dll-only.zip'
}
$oldRejected = $null
if (Test-Path -LiteralPath $oldZip) {
    $oldViolations = @(Test-PackageZip -ZipPath $oldZip)
    if (-not $oldViolations.Count) { throw 'Package check accepted the broken DLL-only delivery' }
    $oldRejected = $true
}
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $root 'dist/SHA256SUMS-current.txt'), "$zipHash  $archiveName`n$dllHash  mod/dlls/main.dll`n", [Text.Encoding]::ASCII)
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$startupVerified = $false
$startupLog = Join-Path $logs 'startup.log'
$loaderLog = Join-Path $logs 'UE4SS-startup.log'
$installedMod = Join-Path $logs "test-profile/shimloader/mod/$identity"
if ((Test-Path -LiteralPath $startupLog) -and (Test-Path -LiteralPath $loaderLog) -and
    (Test-Path -LiteralPath (Join-Path $installedMod 'manifest.json'))) {
    $startup = Get-Content -LiteralPath $startupLog -Raw
    $loader = Get-Content -LiteralPath $loaderLog -Raw
    $installedManifest = Get-Content -LiteralPath (Join-Path $installedMod 'manifest.json') -Raw | ConvertFrom-Json
    $startupVerified = $startup.Contains("boot: Watchers of the Void by ElvardFIC $expectedVersion") -and
        $startup.Contains('HEALTH: PASS') -and $startup.Contains('4 of 4 list skins provided by installed paks') -and
        $loader.Contains("Mod '$identity' has enabled.txt, starting mod.") -and
        $installedManifest.version_number -eq $release.version_number -and
        (Get-FileHash -LiteralPath (Join-Path $installedMod 'dlls/main.dll') -Algorithm SHA256).Hash.ToLowerInvariant() -eq $dllHash
}
[ordered]@{
    package = $identity; packageVersion = $release.version_number; protocol = $protocol
    game = $game; productVersion = $dllInfo.ProductVersion; productName = $dllInfo.ProductName
    authorNamespace = $release.namespace; icon = '256x256 PNG, original fork branding'
    dllSha256 = $dllHash; zipSha256 = $zipHash; sourceEntries = $snapshot.Count
    archiveMembers = $entries.Count; archive = $archiveName; previousBrokenArchiveRejected = $oldRejected
    archiveStartupVerified = $startupVerified; startupEvidence = 'build/package-current/startup.log'
    qualification = $release.qualification; full65GroupSuiteRerun = $false
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $logs 'verification.json') -Encoding utf8
Write-Output "PASS $archiveName -- $($entries.Count) members; loader layout, identity, icon, models and qualified DLL verified"
Write-Output "DLL $expectedVersion SHA256 $dllHash"
