$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent $root
$logs=Join-Path $root 'build/release-2152-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$results=@()
foreach($name in @('run_player_reports.ps1','run_wisp_animation.ps1','run_ragdoll_retry.ps1','run_ragdoll_skin.ps1','run_sleep_personal.ps1','run_entity_sync.ps1')) {
    $log=Join-Path $logs ($name + '.log')
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "agent/tests/$name") *> $log
    $results += [pscustomobject]@{Runner=$name;ExitCode=$LASTEXITCODE;Log=$log}
}
$python=Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
foreach($name in @('player_reports_native_test.py','insomnia_native_test.py','entity_sync_native_test.py')) {
    $log=Join-Path $logs ($name + '.log')
    & $python (Join-Path $root "agent/tests/$name") *> $log
    $results += [pscustomobject]@{Runner=$name;ExitCode=$LASTEXITCODE;Log=$log}
}
$results | ConvertTo-Json | Set-Content (Join-Path $logs 'tests.json') -Encoding UTF8
if(@($results | Where-Object ExitCode -ne 0).Count){throw 'Focused release checks failed; see tests.json'}
Write-Output 'PASS 9 runners, including entity orientation and native spawn/class evidence'
