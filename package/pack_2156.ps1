$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$qualified = & (Join-Path $PSScriptRoot 'assert_2156_qualification.ps1')
$dll = Join-Path $root 'build/votv-coop-2154/main.dll'
$dist = Join-Path $root 'dist'
$stage = Join-Path $dist 'stage-2156'
$version = '0.9.2156'
$dllVersion = '0.9.0n b2179'
$zip = Join-Path $dist "WatchersOfTheVoid-$version.zip"
if (-not (Test-Path -LiteralPath $dll)) { throw "Build main.dll first: $dll" }
$pv = [Diagnostics.FileVersionInfo]::GetVersionInfo($dll).ProductVersion
if ($pv -ne $dllVersion) { throw "main.dll identifies as '$pv', expected '$dllVersion'" }
$expectedStage = [IO.Path]::GetFullPath((Join-Path $root 'dist/stage-2156'))
if ([IO.Path]::GetFullPath($stage) -ne $expectedStage -or (Split-Path -Parent $expectedStage) -ne [IO.Path]::GetFullPath($dist)) { throw 'Stage path escapes the release dist directory' }
if (Test-Path -LiteralPath $stage) {
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    if ($resolvedStage -ne $expectedStage -or (Get-Item -LiteralPath $stage).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing unexpected staging directory' }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'mod/dlls'), (Join-Path $stage 'pak') | Out-Null
$manifest = [ordered]@{
    name = 'WatchersOfTheVoid'
    version_number = $version
    website_url = 'https://github.com/ElvardFIC/WatchersOfTheVoid'
    description = 'Watchers of the Void by ElvardFIC: a Multivoid fork for Voices of the Void 0.9.0n, with co-op synchronization fixes and improvements. Based on Multivoid by Pelmentor.'
    dependencies = @('Thunderstore-unreal_shimloader-1.1.7')
}
if ($manifest.description.Length -gt 250) { throw 'Thunderstore description exceeds 250 characters' }
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'), ($manifest | ConvertTo-Json -Depth 3), (New-Object Text.UTF8Encoding($false)))
Copy-Item (Join-Path $root 'assets/branding/icon.png') (Join-Path $stage 'icon.png')
Add-Type -AssemblyName System.Drawing
$icon = [Drawing.Image]::FromFile((Join-Path $stage 'icon.png'))
try { if ($icon.Width -ne 256 -or $icon.Height -ne 256 -or $icon.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid) { throw 'Thunderstore icon must be a 256x256 PNG' } } finally { $icon.Dispose() }
Copy-Item (Join-Path $PSScriptRoot 'README.md') (Join-Path $stage 'README.md')
Copy-Item (Join-Path $PSScriptRoot 'CHANGELOG_PLUS.md') (Join-Path $stage 'CHANGELOG.md')
Copy-Item (Join-Path $PSScriptRoot 'RELEASE_2156.md') (Join-Path $stage 'RELEASE_2156.md')
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE')
Copy-Item (Join-Path $root 'THIRD-PARTY-NOTICES.md') (Join-Path $stage 'THIRD-PARTY-NOTICES.md')
Copy-Item $dll (Join-Path $stage 'mod/dlls/main.dll')
[IO.File]::WriteAllText((Join-Path $stage 'mod/enabled.txt'), 'true', [Text.Encoding]::ASCII)
Get-ChildItem (Join-Path $root 'assets/paks') -File | Where-Object { $_.Extension -in '.pak', '.png' } | ForEach-Object { Copy-Item $_.FullName (Join-Path $stage 'pak') }
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $names = @($archive.Entries | ForEach-Object { $_.FullName -replace '\\', '/' })
    foreach ($needed in @('manifest.json','icon.png','README.md','CHANGELOG.md','LICENSE','THIRD-PARTY-NOTICES.md','RELEASE_2156.md','mod/enabled.txt','mod/dlls/main.dll','pak/scientists.pak')) { if ($names -notcontains $needed) { throw "Archive lacks $needed" } }
    $reader = New-Object IO.StreamReader(($archive.GetEntry('manifest.json')).Open())
    try { $writtenManifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    if ($writtenManifest.name -ne 'WatchersOfTheVoid' -or $writtenManifest.version_number -ne $version) { throw 'Archive manifest identity mismatch' }
    $reader = New-Object IO.StreamReader(($archive.GetEntry('CHANGELOG.md')).Open())
    try { $heading = $reader.ReadLine() } finally { $reader.Dispose() }
    if ($heading -ne "## $version") { throw 'Archive changelog version mismatch' }
    $entry = $archive.Entries | Where-Object { ($_.FullName -replace '\\', '/') -eq 'mod/dlls/main.dll' } | Select-Object -First 1
    if (-not $entry) { throw 'Archive lacks mod/dlls/main.dll' }
    $sha = [Security.Cryptography.SHA256]::Create(); $stream = $entry.Open()
    try { $embeddedHash = ([BitConverter]::ToString($sha.ComputeHash($stream)) -replace '-', '').ToLowerInvariant() } finally { $stream.Close() }
    $dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($dllHash -ne $qualified.DllSHA256 -or $embeddedHash -ne $dllHash) { throw 'Embedded DLL hash differs from build output' }
} finally { $archive.Dispose() }
Write-Output "PASS $zip"
Write-Output "main.dll sha256 $dllHash"
