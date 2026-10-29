param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
# Apply/Mirror/Release was replaced by native lift replay. Keep the historical
# entry point, but run its maintained successor, including authority, lifetime
# and fault controls. This alias is not an independent validation group.
& (Join-Path $PSScriptRoot 'run_2136.ps1') -Python $Python
