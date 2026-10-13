param([switch]$RetryFailed)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$logs = Join-Path $root 'build/release-2150-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$results = @()
# Integration release: run the focused regressions for all accumulated batches once.
$runners = Get-ChildItem (Join-Path $root 'agent/tests') -Filter 'run_*.ps1' |
    Where-Object { $_.BaseName -match '^run_(\d+)$' -and [int]$Matches[1] -ge 2143 } |
    Sort-Object Name
$runners += Get-Item (Join-Path $root 'agent/tests/run_stow_2157.ps1')
if ($RetryFailed) {
    $previous = @(Get-Content (Join-Path $logs 'tests.json') -Raw | ConvertFrom-Json)
    $failed = @($previous | ForEach-Object { $_ } | Where-Object ExitCode -ne 0 | ForEach-Object Runner)
    $results = @($previous | ForEach-Object { $_ } | Where-Object ExitCode -eq 0)
    $runners = @($runners | Where-Object { $_.Name -in $failed })
}
foreach ($runner in $runners) {
    $log = Join-Path $logs ($runner.BaseName + '.log')
    $ErrorActionPreference = 'Continue'
    & powershell -NoProfile -ExecutionPolicy Bypass -File $runner.FullName *> $log
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $results += [pscustomobject]@{ Runner=$runner.Name; ExitCode=$code; Log=$log }
    Write-Output "$($runner.Name): exit $code"
}
$results | ConvertTo-Json | Set-Content (Join-Path $logs 'tests.json') -Encoding UTF8
if (@($results | Where-Object ExitCode -ne 0).Count) { throw 'Release regression failures; see tests.json' }
