$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent $root
$qualified=& (Join-Path $PSScriptRoot 'assert_2155_qualification.ps1')
$zip=Join-Path $root 'dist/WatchersOfTheVoid-0.9.2155.zip'
$notes=Join-Path $PSScriptRoot 'GITHUB_RELEASE_2155.md'
$logs=Join-Path $root 'build/release-2155-logs'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
function EntryHash($Entry) {
    $sha=[Security.Cryptography.SHA256]::Create()
    $stream=$Entry.Open()
    try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally {$stream.Dispose();$sha.Dispose()}
}
try {
    $entries=@{}
    foreach($entry in $archive.Entries) {
        $name=$entry.FullName.Replace('\','/')
        if($entries.ContainsKey($name)){throw "Duplicate archive member: $name"}
        if($name.StartsWith('/') -or $name -match '(^|/)\.\.(/|$)'){throw "Invalid archive member: $name"}
        $entries[$name]=$entry
    }
    $expected=@{
        'mod/dlls/main.dll'='build/votv-coop-2154/main.dll'
        'README.md'='package/README.md';'CHANGELOG.md'='package/CHANGELOG_PLUS.md'
        'RELEASE_2155.md'='package/RELEASE_2155.md';'LICENSE'='LICENSE'
        'THIRD-PARTY-NOTICES.md'='THIRD-PARTY-NOTICES.md';'icon.png'='assets/branding/icon.png'
    }
    foreach($pak in (Get-ChildItem -LiteralPath (Join-Path $root 'assets/paks') -File | Where-Object Extension -in '.pak','.png')) {
        $expected['pak/'+$pak.Name]='assets/paks/'+$pak.Name
    }
    foreach($name in $expected.Keys) {
        if(-not $entries.ContainsKey($name)){throw "Missing archive member: $name"}
        if((EntryHash $entries[$name]) -ne (Get-FileHash -LiteralPath (Join-Path $root $expected[$name]) -Algorithm SHA256).Hash.ToLowerInvariant()) {
            throw "Archive differs from current qualified delivery input: $name"
        }
    }
    if(-not $entries.ContainsKey('manifest.json') -or -not $entries.ContainsKey('mod/enabled.txt') -or
       -not $entries.ContainsKey('pak/scientists.pak') -or $entries.Count -ne $expected.Count+2){throw 'Unexpected archive layout'}
    $reader=[IO.StreamReader]::new($entries['manifest.json'].Open())
    try {$manifest=$reader.ReadToEnd() | ConvertFrom-Json} finally {$reader.Dispose()}
    if($manifest.name -ne 'WatchersOfTheVoid' -or $manifest.version_number -ne '0.9.2155' -or
       $manifest.website_url -ne 'https://github.com/ElvardFIC/WatchersOfTheVoid' -or
       $manifest.description.Length -gt 250 -or $manifest.dependencies.Count -ne 1 -or
       $manifest.dependencies[0] -ne 'Thunderstore-unreal_shimloader-1.1.7'){throw 'Manifest mismatch'}
    $reader=[IO.StreamReader]::new($entries['mod/enabled.txt'].Open())
    try {if($reader.ReadToEnd() -ne 'true'){throw 'Mod must be enabled'}} finally {$reader.Dispose()}
    $reader=[IO.StreamReader]::new($entries['CHANGELOG.md'].Open())
    try {if($reader.ReadLine() -ne '## 0.9.2155'){throw 'Changelog version mismatch'}} finally {$reader.Dispose()}
} finally {$archive.Dispose()}
if(-not(Test-Path -LiteralPath $notes)){throw 'GitHub release notes missing'}
$zipHash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
$delivery=Join-Path $workspace 'dist'
New-Item -ItemType Directory -Force -Path $delivery | Out-Null
$outerZip=Join-Path $delivery 'WatchersOfTheVoid-0.9.2155.zip'
Copy-Item -LiteralPath $zip -Destination $outerZip -Force
Copy-Item -LiteralPath $notes -Destination (Join-Path $delivery 'GITHUB_RELEASE_2155.md') -Force
if((Get-FileHash -LiteralPath $outerZip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $zipHash){throw 'Delivery copy mismatch'}
$sums="$zipHash  WatchersOfTheVoid-0.9.2155.zip`n$($qualified.DllSHA256)  mod/dlls/main.dll`n"
[IO.File]::WriteAllText((Join-Path $delivery 'SHA256SUMS-2155.txt'),$sums,[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $root 'dist/SHA256SUMS-2155.txt'),$sums,[Text.Encoding]::ASCII)
[pscustomobject]@{
    Release=2155;PackageVersion='0.9.2155';Protocol=2178;ProductVersion=$qualified.ProductVersion
    Qualification='build/continuation-check/verification.json'
    PassingGroups=65;BuildSourceEntries=$qualified.BuildSourceEntries
    SourceSnapshotSHA256=$qualified.SourceSnapshotSHA256;DllSHA256=$qualified.DllSHA256;ZipSHA256=$zipHash
    ArchiveMembers=$entries.Count;Packaged=$true;TwoPlayerGameplayVerified=$false;FreshNativeAuditDeferred=$true
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logs 'release-verification.json') -Encoding UTF8
Write-Output "PASS package 2155: 65 groups, qualified b2178 DLL, source snapshot, archive and delivery hashes"
Write-Output "ZIP $outerZip"
Write-Output "SHA256 $zipHash"
