param(
    [string]$BuildDirectory='build/votv-coop-2154',
    [string]$EvidenceDirectory='build/todo-audit'
)
$ErrorActionPreference='Stop'
$checkout=Split-Path -Parent $PSScriptRoot
$evidence=if([IO.Path]::IsPathRooted($EvidenceDirectory)){$EvidenceDirectory}else{Join-Path $checkout $EvidenceDirectory}
$logs=Join-Path $evidence 'logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$python=Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
$results=[Collections.Generic.List[object]]::new()
function Get-SourceStamp {
    $production=Join-Path $checkout 'src/votv-coop'
    $files=@(Get-ChildItem -LiteralPath (Join-Path $production 'include'),(Join-Path $production 'src') -Recurse -File |
        Where-Object { $_.Extension -in @('.h','.hpp','.cpp','.inc','.rc') })
    $files+=Get-Item -LiteralPath (Join-Path $production 'CMakeLists.txt'),(Join-Path $production 'sources.cmake')
    $rows=foreach($file in ($files | Sort-Object FullName)) {
        $file.FullName.Substring($production.Length)+':'+(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    $sha=[Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows -join "`n"))))).Replace('-','').ToLowerInvariant() }
    finally {$sha.Dispose()}
}
function Invoke-Check([string]$Name,[string]$Program,[string[]]$Arguments) {
    $log=Join-Path $logs ($Name+'.log')
    # Expected-failure controls write assertions to stderr and still pass their
    # runner. Record the runner's exit code instead of treating stderr as failure.
    $ErrorActionPreference='Continue'
    & $Program @Arguments 2>&1 | Out-File -LiteralPath $log -Encoding UTF8
    $code=$LASTEXITCODE
    $results.Add([pscustomobject]@{Runner=$Name;ExitCode=$code;Log=$log})
    $results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'results.json') -Encoding UTF8
    Write-Output "$Name $code"
}
Push-Location $checkout
try {
    $sourceAtStart=Get-SourceStamp
    foreach($name in @(
        'run_player_reports','run_wisp_animation','run_ragdoll_retry','run_ragdoll_skin',
        'run_sleep_personal','run_entity_sync','run_garden_sync','run_laptop_ui','run_world_sound','run_events',
        'run_placement','run_shared_screen','run_item_batch','run_held_identity',
        'run_2130','run_2139','run_2140','run_2141','run_2153','run_2154','run_2155','run_2156',
        'run_stow_2157','run_2160','run_2161','run_2162','run_2165','run_2168','run_2169',
        'run_2170','run_2172','run_2173','run_2180'
    )) {
        Invoke-Check $name 'powershell' @('-NoProfile','-ExecutionPolicy','Bypass','-File',"agent/tests/$name.ps1")
    }
    Invoke-Check 'coin_collect' 'powershell' @('-NoProfile','-ExecutionPolicy','Bypass','-File','agent/tests/run_2151.ps1','-CollectOnly')
    foreach($name in @('player_reports_native_test','insomnia_native_test','entity_sync_native_test','coin_collect_native_test')) {
        Invoke-Check $name $python @("agent/tests/$name.py")
    }
    # Disk regression compiles the production conversion code with MSVC.
    . (Join-Path $checkout '../_toolchain/env.ps1')
    Invoke-Check 'disk_utf8' $python @('agent/tests/disk_utf8_test.py')
    Invoke-Check 'reliablekind_gate' $python @('.github/ci/reliablekind_gate.py','--drill')
    if((Get-SourceStamp) -ne $sourceAtStart){throw 'Production sources changed during the regression run; rerun against a stable source tree'}
    $build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $checkout $BuildDirectory}
    # CMake's always-run glob check makes Ninja dry runs pessimistically schedule
    # reconfiguration. Complete the real incremental build, then check the snapshot.
    & cmake --build $build --target votv-coop --parallel 2 *> (Join-Path $evidence 'build.log')
    if($LASTEXITCODE -ne 0){throw 'Current Release build failed; see build.log'}
    if((Get-SourceStamp) -ne $sourceAtStart){throw 'Production sources changed during the Release build; rerun against a stable source tree'}
    & (Join-Path $PSScriptRoot 'verify_current_build.ps1') -BuildDirectory $BuildDirectory -EvidenceDirectory $evidence
    $identity=Get-Content -LiteralPath (Join-Path $evidence 'identity.json') -Raw | ConvertFrom-Json
    $sourceHashes=[ordered]@{}
    foreach($path in @('include/coop/net/protocol.h','src/coop/props/prop_drop_intent.cpp',
        'src/coop/items/coingun_collect.cpp','src/coop/props/trash_collect_sync.cpp',
        'src/coop/interactables/laptop_sync.cpp','src/coop/net/client_authorship.h','src/coop/net/session_lanes.h',
        'include/coop/config/config_registry.h','include/coop/config/config_registry_rows.inc',
        'src/coop/world/event_rate.cpp','src/coop/world/event_fire_sync.cpp','src/coop/world/event_fire_policy.cpp',
        'src/coop/world/event_fire_delivery.cpp','src/ui/server_settings_pane.cpp','src/coop/session/subsystems.cpp','sources.cmake')) {
        $sourceHashes[$path]=(Get-FileHash -LiteralPath (Join-Path $checkout "src/votv-coop/$path") -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $identity | Add-Member -NotePropertyName Results -NotePropertyValue @($results.ToArray())
    $identity | Add-Member -NotePropertyName PassingGroups -NotePropertyValue @($results | Where-Object ExitCode -eq 0).Count
    $identity | Add-Member -NotePropertyName SourceSHA256 -NotePropertyValue $sourceHashes
    $identity | Add-Member -NotePropertyName SourceSnapshotSHA256 -NotePropertyValue $sourceAtStart
    $identity | Add-Member -NotePropertyName BuildUpToDate -NotePropertyValue $true
    $identity | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'verification.json') -Encoding UTF8
    if(@($results | Where-Object ExitCode -ne 0).Count){throw 'Current checks failed; see results.json and runner logs'}
    Write-Output "PASS $($results.Count) current validation groups; gameplay remains unverified"
} finally {Pop-Location}
