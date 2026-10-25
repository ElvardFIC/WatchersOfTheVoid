param(
    [string]$BuildDirectory='build/votv-coop-2154',
    [string]$EvidenceDirectory='build/todo-audit',
    [string]$AssetDirectory='../_research/current-assets',
    [string[]]$CheckGroups=@()
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
    if($CheckGroups.Count -and $Name -notin $CheckGroups){return}
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
    [IO.File]::WriteAllText((Join-Path $evidence 'source-start.sha256'), $sourceAtStart)
    foreach($name in @(
        'run_player_reports','run_host_players','run_session_reports','run_sat_console_delivery','run_sat_console_callbacks','run_container_recovery','run_container_birth','run_container_apply','run_ariral_ship','run_inventory_apply','run_profile_join','run_2174','run_wisp_animation','run_ragdoll_retry','run_ragdoll_skin',
        'run_sleep_personal','run_dream_identity','run_entity_sync','run_garden_sync','run_laptop_ui','run_world_sound','run_flashlight_click','run_events',
        'run_placement','run_shared_screen','run_balance','run_item_batch','run_held_identity','run_trowel',
        'run_2130','run_2139','run_2140','run_2141','run_2153','run_2154','run_2155','run_2156',
        'run_stow_2157','run_2160','run_2161','run_2162','run_2165','run_2168','run_2169','run_blood','run_atv_steering',
        'run_2170','run_2172','run_2173','run_2180','run_sync_2157','run_upstream','run_upstream_integration','run_explosion_motion',
        'run_nonentity','run_cooking','run_food_callbacks','run_prop_action_delivery','run_prop_action_native','run_drink_spills','run_gasoline','run_grill_edits',
        'run_grill_hand','run_grime_join','run_hand_tools','run_item_records','run_save_record','run_kitchen',
        'run_scorch','run_sponge_effects','run_window_stroke','run_toy_music','run_wall_creative','run_water_pour',
        'run_watoy','run_windows','run_2166','run_wall_delivery','run_2176','run_2157','run_2145','run_2147'
    )) {
        Invoke-Check $name 'powershell' @('-NoProfile','-ExecutionPolicy','Bypass','-File',"agent/tests/$name.ps1")
    }
    foreach($entry in @(@('coin_collect','-CollectOnly'),@('inventory_capture','-InventoryOnly'))) {
        Invoke-Check $entry[0] 'powershell' @('-NoProfile','-ExecutionPolicy','Bypass','-File','agent/tests/run_2151.ps1',$entry[1])
    }
    foreach($name in @('player_reports_native_test','insomnia_native_test','entity_sync_native_test','coin_collect_native_test','native_physics_fingerprints_test','event_rate_native_test','dream_identity_native_test','server_breaker_native_test','sync_native_contract_test')) {
        $arguments=@("agent/tests/$name.py")
        $runner=$name
        if($name -in @('dream_identity_native_test','server_breaker_native_test','sync_native_contract_test')) {
            $arguments+=$AssetDirectory
        }
        if($name -eq 'sync_native_contract_test') {
            $arguments+='../_research'
            $runner='sync_native_contract'
        }
        Invoke-Check $runner $python $arguments
    }
    Invoke-Check 'public_prose_include' $python @('.github/ci/public_prose_include_test.py')
    # Disk regression compiles the production conversion code with MSVC.
    . (Join-Path $checkout '../_toolchain/env.ps1')
    Invoke-Check 'disk_utf8' $python @('agent/tests/disk_utf8_test.py')
    Invoke-Check 'reliablekind_gate' $python @('.github/ci/reliablekind_gate.py','--drill')
    foreach($requested in $CheckGroups){if($requested -notin $results.Runner){throw "Unknown check group: $requested"}}
    if((Get-SourceStamp) -ne $sourceAtStart){throw 'Production sources changed during the regression run; rerun against a stable source tree'}
    $build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $checkout $BuildDirectory}
    # CMake's always-run glob check makes Ninja dry runs pessimistically schedule
    # reconfiguration. Complete the real incremental build, then check the snapshot.
    # Windows PowerShell promotes native stderr under Stop, including CMake's
    # dependency deprecation notices. Preserve the output and judge the exit code.
    $ErrorActionPreference='Continue'
    & cmake --build $build --target votv-coop --parallel 2 *> (Join-Path $evidence 'build.log')
    $buildCode=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if($buildCode -ne 0){throw 'Current Release build failed; see build.log'}
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
    $identity | Add-Member -NotePropertyName FullRegression -NotePropertyValue ($CheckGroups.Count -eq 0)
    $identity | Add-Member -NotePropertyName RequestedGroups -NotePropertyValue @($CheckGroups)
    $identity | Add-Member -NotePropertyName SourceSHA256 -NotePropertyValue $sourceHashes
    $identity | Add-Member -NotePropertyName SourceSnapshotSHA256 -NotePropertyValue $sourceAtStart
    $identity | Add-Member -NotePropertyName BuildUpToDate -NotePropertyValue $true
    $identity | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'verification.json') -Encoding UTF8
    if(@($results | Where-Object ExitCode -ne 0).Count){throw 'Current checks failed; see results.json and runner logs'}
    Write-Output "PASS $($results.Count) current validation groups; gameplay remains unverified"
} finally {Pop-Location}
