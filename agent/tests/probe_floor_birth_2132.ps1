# Compatibility entry for the original red reproduction. Historical failure:
# _research/floor_probe_2132.log. Fixed cases live in the full regression suite.
param([string]$Python='python')
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'run_2132.ps1') -Python $Python
