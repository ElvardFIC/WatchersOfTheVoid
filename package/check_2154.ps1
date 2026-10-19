$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$logs = Join-Path $root 'build/release-2154-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$runs = @(
    @{ Name = 'run_garden_sync.ps1'; Args = @() },
    @{ Name = 'run_laptop_ui.ps1'; Args = @() },
    @{ Name = 'run_world_sound.ps1'; Args = @() },
    @{ Name = 'run_entity_sync.ps1'; Args = @() }
)
$results = @()
$assetRoot = Join-Path $root '_research/current-assets'
if (Test-Path -LiteralPath $assetRoot) {
    $runs += @{ Name = 'sync_native_contract_test.py'; Args = @($assetRoot, (Join-Path $root '_research')) }
} else {
    $results += [pscustomobject]@{ Runner = 'sync_native_contract_test.py'; ExitCode = $null; Skipped = $true; Reason = 'Extracted Voices of the Void 0.9.0n assets are unavailable' }
    Write-Warning 'Skipping native asset contract checks: _research/current-assets is unavailable.'
}
foreach ($run in $runs) {
    $log = Join-Path $logs ($run.Name + '.log')
    if ($run.Name.EndsWith('.ps1')) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "agent/tests/$($run.Name)") @($run.Args) *> $log
    } else {
        $python = Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
        & $python (Join-Path $root "agent/tests/$($run.Name)") @($run.Args) *> $log
    }
    $results += [pscustomobject]@{ Runner = $run.Name; ExitCode = $LASTEXITCODE; Skipped = $false; Log = $log }
}
$results | ConvertTo-Json | Set-Content (Join-Path $logs 'tests.json') -Encoding UTF8
$failed = @($results | Where-Object { -not $_.Skipped -and $_.ExitCode -ne 0 })
if ($failed.Count) { throw 'Release checks failed; see tests.json' }
$passed = @($results | Where-Object { -not $_.Skipped }).Count
$skipped = @($results | Where-Object Skipped).Count
Write-Output "PASS $passed release check groups; SKIPPED $skipped (native assets unavailable)"
