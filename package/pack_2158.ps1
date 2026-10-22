param([switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$release=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release.json') -Raw|ConvertFrom-Json
if($release.version_number -ne '0.9.2158'){throw 'This recipe is for package 0.9.2158'}
& (Join-Path $PSScriptRoot 'package_current.ps1') -VerifyOnly:$VerifyOnly
