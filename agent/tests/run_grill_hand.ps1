param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/grill-hand-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/" "/Fe$output/controller.exe" agent/tests/grill_hand_sync_test.cpp src/votv-coop/src/coop/net/session_serial.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grill hand controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Grill hand controller failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/persistence.obj" "/Fe$output/persistence.exe" agent/tests/grill_saved_state_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grill persistence compile failed'}
    & "$output/persistence.exe"
    if($LASTEXITCODE -ne 0){throw 'Grill persistence failed'}
    if($FaultControls){
        $source=Get-Content 'src/votv-coop/src/coop/interactables/grill_hand_sync.cpp' -Raw
        foreach($name in @('admit-replacement','stale-fanout','lost-recipient-debt','stale-local-read','session-reset','session-callback','old-cleanup')){
            $control=Join-Path $output $name
            New-Item -ItemType Directory -Force -Path $control | Out-Null
            $mutant=switch($name){
                'admit-replacement' {$source.Replace('g_rows[slot].revision==snapshot.revision && ','')}
                'stale-fanout' {$source.Replace('if(g_rows[owner].revision!=row.revision)break;','').Replace('if(latest.revision!=row.revision)break;','')}
                'lost-recipient-debt' {$source.Replace(' && g_rows[slot].revision==recipientRevision','')}
                'stale-local-read' {$source.Replace(' || g_rows[slot].revision!=revision','')}
                'session-reset' {$source.Replace(' && g_serial==serial','')}
                'session-callback' {$source.Replace(' && g_serial==net::session_serial::Current()','')}
                'old-cleanup' {$source.Replace('serial!=net::session_serial::Current() ||','')}
            }
            if($mutant -eq $source){throw "Missing mutation site: $name"}
            $mutantPath=(Join-Path $control 'grill_hand.inc').Replace('\','/')
            [IO.File]::WriteAllText($mutantPath,$mutant,[Text.UTF8Encoding]::new($false))
            $testPath=(Join-Path $PSScriptRoot 'grill_hand_sync_test.cpp').Replace('\','/')
            $fixture=Join-Path $control 'control.cpp'
            $lines=@(('#define GRILL_HAND_SOURCE "'+$mutantPath+'"'),('#include "'+$testPath+'"'))
            [IO.File]::WriteAllText($fixture,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/state_2145_stubs" "/I$PSScriptRoot/text_drone_stubs" "/I$PSScriptRoot/stubs" /I src/votv-coop/include "/Fo$control/" "/Fe$control/control.exe" $fixture src/votv-coop/src/coop/net/session_serial.cpp /link /OPT:REF
            if($LASTEXITCODE -ne 0){throw "Control compile failed: $name"}
            & "$control/control.exe"
            if($LASTEXITCODE -ne 1){throw "Control did not fire its assertion: $name"}
            Write-Output "PASS expected-failure control: $name"
        }
    }
    if($CompileSources){
        foreach($file in @('coop/interactables/grill_hand_sync','ue_wrap/devices/grill','ue_wrap/devices/grill_saved_state','coop/world/material_save_sync','coop/items/player_inventory_sync','coop/interactables/kitchen_sync','coop/dispatch/event_dispatch_world')){
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Grill hand source compile failed: $file"}
        }
    }
} finally {Pop-Location}
