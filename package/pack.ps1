# Builds WatchersOfTheVoid-<version>.zip for r2modman / Thunderstore "Import local mod", in the layout of
# the upstream package (manifest at the root, mod\ for the UE4SS mod folder, pak\ for LogicMods).
$ErrorActionPreference = 'Stop'
$root    = Split-Path -Parent $PSScriptRoot
$src     = $root
$dll     = Join-Path $src 'build\votv-coop-2150\main.dll'
$dist    = Join-Path $root 'dist'
$stage   = Join-Path $dist 'stage'
$version = '0.9.2150'
$zip     = Join-Path $dist "WatchersOfTheVoid-$version.zip"

if (-not (Test-Path -LiteralPath $dll)) { throw "build main.dll first: $dll" }
$pv = [Diagnostics.FileVersionInfo]::GetVersionInfo($dll).ProductVersion
if ($pv -ne '0.9.0n b2150') { throw "main.dll identifies as '$pv', not '0.9.0n b2150'" }

$resolvedDist = [IO.Path]::GetFullPath($dist).TrimEnd('\')
$resolvedStage = [IO.Path]::GetFullPath($stage)
if ($resolvedStage -ne (Join-Path $resolvedDist 'stage')) { throw "unexpected stage path: $resolvedStage" }
if (Test-Path -LiteralPath $resolvedStage) { Remove-Item -LiteralPath $resolvedStage -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'mod\dlls'), (Join-Path $stage 'pak') | Out-Null

$manifest = [ordered]@{
    name           = 'WatchersOfTheVoid'
    version_number = $version
    website_url    = 'https://github.com/ElvardFIC/WatchersOfTheVoid'
    description    = 'Watchers of the Void by ElvardFIC: a Multivoid fork for Voices of the Void 0.9.0n, with co-op synchronization fixes and improvements. Based on Multivoid by Pelmentor.'
    dependencies   = @('Thunderstore-unreal_shimloader-1.1.7')
}
if ($manifest.description.Length -gt 250) { throw "description is $($manifest.description.Length) chars (max 250)" }
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'), ($manifest | ConvertTo-Json -Depth 3),
                        (New-Object Text.UTF8Encoding($false)))

Copy-Item (Join-Path $src 'assets\branding\icon.png') (Join-Path $stage 'icon.png')
Add-Type -AssemblyName System.Drawing
$icon = [Drawing.Image]::FromFile((Join-Path $stage 'icon.png'))
try {
    if ($icon.Width -ne 256 -or $icon.Height -ne 256 -or $icon.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid) { throw 'Thunderstore icon must be a 256x256 PNG' }
} finally { $icon.Dispose() }
Copy-Item (Join-Path $PSScriptRoot 'README.md') (Join-Path $stage 'README.md')
# This package's changelog is only the Plus notes. The upstream history stays in the
# Pelmentor tree and is not appended, so the r2modman page is not that whole log.
Copy-Item (Join-Path $PSScriptRoot 'CHANGELOG_PLUS.md') (Join-Path $stage 'CHANGELOG.md')
Copy-Item (Join-Path $PSScriptRoot 'RELEASE_2150.md') (Join-Path $stage 'RELEASE_2150.md')
Copy-Item (Join-Path $src 'LICENSE') (Join-Path $stage 'LICENSE')
Copy-Item (Join-Path $src 'THIRD-PARTY-NOTICES.md') (Join-Path $stage 'THIRD-PARTY-NOTICES.md')

Copy-Item $dll (Join-Path $stage 'mod\dlls\main.dll')
[IO.File]::WriteAllText((Join-Path $stage 'mod\enabled.txt'), 'true', [Text.Encoding]::ASCII)
Get-ChildItem (Join-Path $src 'assets\paks') -File | Where-Object { $_.Extension -in '.pak', '.png' } |
    ForEach-Object { Copy-Item $_.FullName (Join-Path $stage 'pak') }

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

# Check the written zip, not the staged tree.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $names = @($z.Entries | ForEach-Object { $_.FullName -replace '\\', '/' })
    foreach ($need in 'manifest.json', 'icon.png', 'README.md', 'CHANGELOG.md', 'LICENSE',
                      'THIRD-PARTY-NOTICES.md', 'RELEASE_2150.md', 'mod/enabled.txt', 'mod/dlls/main.dll', 'pak/scientists.pak') {
        if ($names -notcontains $need) { throw "zip lacks $need" }
    }
    $reader = New-Object IO.StreamReader(($z.GetEntry('manifest.json')).Open())
    try { $writtenManifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    if ($writtenManifest.name -ne 'WatchersOfTheVoid' -or $writtenManifest.version_number -ne $version) {
        throw 'ZIP manifest identity mismatch'
    }
    $reader = New-Object IO.StreamReader(($z.GetEntry('CHANGELOG.md')).Open())
    try { $heading = $reader.ReadLine() } finally { $reader.Dispose() }
    if ($heading -ne "## $version") { throw 'ZIP changelog version mismatch' }
    $entry = $z.Entries | Where-Object { ($_.FullName -replace '\\', '/') -eq 'mod/dlls/main.dll' }
    $sha = [Security.Cryptography.SHA256]::Create()
    $s = $entry.Open()
    try { $inZip = ([BitConverter]::ToString($sha.ComputeHash($s)) -replace '-', '').ToLowerInvariant() } finally { $s.Close() }
    $built = (Get-FileHash -Algorithm SHA256 $dll).Hash.ToLowerInvariant()
    if ($inZip -ne $built) { throw "zip main.dll sha $inZip != built $built" }
} finally { $z.Dispose() }

Write-Host "ok $zip"
Write-Host "main.dll sha256 $built"
