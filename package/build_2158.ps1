$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$release=Get-Content (Join-Path $PSScriptRoot 'release.json') -Raw | ConvertFrom-Json
if($release.version_number -ne '0.9.2158' -or $release.protocol -ne 2186){throw 'Expected package 2158 / protocol 2186'}
. (Join-Path $root '../_toolchain/env.ps1')
$evidence=Join-Path $root 'build/release-2158'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$files=@(Get-ChildItem -LiteralPath (Join-Path $root 'src/votv-coop/src'),(Join-Path $root 'src/votv-coop/include') -Recurse -File)
$files+=Get-Item -LiteralPath (Join-Path $root 'src/votv-coop/CMakeLists.txt'),(Join-Path $root 'src/votv-coop/sources.cmake'),(Join-Path $PSScriptRoot 'release.json')
$snapshot=foreach($file in ($files | Sort-Object FullName)) {
    [pscustomobject]@{path=$file.FullName.Substring($root.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$snapshot | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $evidence 'source-before.json') -Encoding UTF8
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'check_current.ps1') -EvidenceDirectory 'build/release-2158'
if($LASTEXITCODE){throw 'Offline release qualification failed'}
& "$env:USERPROFILE/.local/bin/python3.15.exe" (Join-Path $PSScriptRoot 'verify_package_label.py')
if($LASTEXITCODE){throw 'Package display formatter check failed'}
& (Join-Path $PSScriptRoot 'pack_2158.ps1')
& "$env:USERPROFILE/.local/bin/python3.15.exe" (Join-Path $PSScriptRoot 'verify_test_package.py')
if($LASTEXITCODE){throw 'Test package rejection controls failed'}
