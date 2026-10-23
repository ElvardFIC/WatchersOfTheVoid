param([switch]$FaultControls)
$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output = Join-Path $checkout 'build/sleep-personal-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$PSScriptRoot/sleep_stubs" `
    "/I$checkout/src/votv-coop/include" "/Fo$output/test.obj" "/Fe$output/test.exe" `
    "$PSScriptRoot/sleep_personal_test.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Personal sleep regression compile failed' }
& "$output/test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Personal sleep regression failed' }
& cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/sleep_stubs" `
    "/I$checkout/src/votv-coop/include" "/Fo$output/delivery.obj" "/Fe$output/delivery.exe" `
    "$PSScriptRoot/sleep_delivery_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Sleep delivery compile failed'}
& "$output/delivery.exe"
if($LASTEXITCODE){throw 'Sleep delivery regression failed'}
if($FaultControls) {
    $source=Get-Content (Join-Path $checkout 'src/votv-coop/src/coop/player/sleep_sync.cpp') -Raw
    foreach($name in @('lost-host-level','lost-peer-level','incomplete-join','stale-occupancy','foreign-debt',
        'paced-wake','host-dream','inactive-input','stale-world','stale-client-payload','stale-client-send','disconnect-debt')) {
        $mutant=$source
        switch($name) {
            'lost-host-level' {$mutant=$source.Replace('if(sent && g_hostDebt[to]==hostRevision)','if(g_hostDebt[to]==hostRevision)')}
            'lost-peer-level' {$mutant=$source.Replace('if(sent && g_restDebt[to][slot]==revision)','if(g_restDebt[to][slot]==revision)')}
            'incomplete-join' {$mutant=$source.Replace('g_restDebt[to][slot]=++g_revision;','if(g_state[slot]!=SleepPeerState::Awake)g_restDebt[to][slot]=++g_revision;')}
            'stale-occupancy' {$mutant=$source.Replace('g_generation[slot]!=s->peerGenerationForSlot(slot)','false')}
            'foreign-debt' {$mutant=$source.Replace('g_session.load()!=&session ||','false ||')}
            'paced-wake' {$mutant=$source.Replace('(g_pending[slot].pace.Owed() && g_pending[slot].state!=SleepPeerState::Resting)','false')}
            'host-dream' {$mutant=$source.Replace('local!=SleepPeerState::Resting ||','false ||')}
            'inactive-input' {$mutant=$source.Replace('if (!s || !s->running() || !s->connected()) return;','if (!s) return;')}
            'stale-world' {$mutant=[regex]::Replace($source,'(?ms)^void RefreshContext\(.*?^\}','void RefreshContext(coop::net::Session*) {}')}
            'stale-client-payload' {$mutant=$source.Replace('g_sentState = static_cast<SleepPeerState>(p.state);','g_sentState = g_localState;')}
            'stale-client-send' {$mutant=$source.Replace('&& serial==g_clientSendSerial','')}
            'disconnect-debt' {$mutant=$source.Replace('g_pending[slot]={};g_reportsApplied[slot]=0;','g_reportsApplied[slot]=0;')}
        }
        if($mutant -eq $source){throw "Missing sleep control site: $name"}
        $control=Join-Path $output $name
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $inc=(Join-Path $control 'sleep.inc').Replace('\','/')
        [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
        $test=(Join-Path $PSScriptRoot 'sleep_delivery_test.cpp').Replace('\','/')
        $fixture=Join-Path $control 'control.cpp'
        [IO.File]::WriteAllText($fixture,('#define SLEEP_SYNC_SOURCE "'+$inc+'"'+"`n"+'#include "'+$test+'"'),[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/sleep_stubs" `
            "/I$checkout/src/votv-coop/include" "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Sleep control compile failed: $name"}
        & "$control/test.exe"
        if($LASTEXITCODE -ne 1){throw "Sleep control did not detect removed protection: $name"}
        Write-Output "PASS expected-failure control: $name"
    }
}
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
    "/Fo$output/sleep_sync.obj" "$checkout/src/votv-coop/src/coop/player/sleep_sync.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Production sleep_sync compilation failed' }
exit 0
