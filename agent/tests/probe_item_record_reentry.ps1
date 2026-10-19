# Historical entry point, now runs the full b2130 regression and negative controls.
# The original failing evidence remains in _research/item_record_audit/red_*.log.
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
Push-Location $root
try {
    & (Join-Path $PSScriptRoot 'run_2130.ps1')
} finally { Pop-Location }
