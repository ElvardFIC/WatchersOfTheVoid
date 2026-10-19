$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$logs = Join-Path $root 'build/release-2153-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$runs = @(
    @{ Name = 'run_garden_sync.ps1'; Args = @() },
    @{ Name = 'run_laptop_ui.ps1'; Args = @() },
    @{ Name = 'run_world_sound.ps1'; Args = @() },
    @{ Name = 'run_entity_sync.ps1'; Args = @() },
    @{ Name = 'sync_native_contract_test.py'; Args = @((Join-Path $root '_research/current-assets'), (Join-Path $root '_research')) }
)
$results = @()
foreach ($run in $runs) {
    $log = Join-Path $logs ($run.Name + '.log')
    if ($run.Name.EndsWith('.ps1')) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "agent/tests/$($run.Name)") @($run.Args) *> $log
    } else {
        $python = Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
        & $python (Join-Path $root "agent/tests/$($run.Name)") @($run.Args) *> $log
    }
    $results += [pscustomobject]@{ Runner = $run.Name; ExitCode = $LASTEXITCODE; Log = $log }
}
$results | ConvertTo-Json | Set-Content (Join-Path $logs 'tests.json') -Encoding UTF8
if (@($results | Where-Object ExitCode -ne 0).Count) { throw 'Release checks failed; see tests.json' }
Write-Output "PASS $($results.Count) release check groups"
