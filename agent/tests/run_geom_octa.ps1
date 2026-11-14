param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/geom-octa-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& python3.15.exe (Join-Path $PSScriptRoot 'geom_octa_native_test.py')
if($LASTEXITCODE -ne 0){throw 'Native Ramiel contract failed'}
& python3.15.exe (Join-Path $PSScriptRoot 'actor_transform_extract.py') (Join-Path $output 'actor_transform.inc')
if($LASTEXITCODE -ne 0){throw 'Actor transform extraction failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/Fo$output/actor_transform.obj" "/Fe$output/actor_transform.exe" "$PSScriptRoot/actor_transform_test.cpp"
if($LASTEXITCODE -ne 0){throw 'Actor transform compile failed'}
& "$output/actor_transform.exe"
if($LASTEXITCODE -ne 0){throw 'Actor transform fixture failed'}
& python3.15.exe (Join-Path $PSScriptRoot 'session_world_ready_extract.py') (Join-Path $output 'session_world_ready.inc')
if($LASTEXITCODE -ne 0){throw 'World-ready extraction failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/Fo$output/session_world_ready.obj" "/Fe$output/session_world_ready.exe" "$PSScriptRoot/session_world_ready_test.cpp"
if($LASTEXITCODE -ne 0){throw 'World-ready compile failed'}
& "$output/session_world_ready.exe"
if($LASTEXITCODE -ne 0){throw 'World-ready fixture failed'}
foreach($test in @('geom_octa_lifetime','geom_octa_wrapper')) {
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/geom_stubs" "/I$checkout/src/votv-coop/include" `
        "/Fo$output/$test.obj" "/Fe$output/$test.exe" "$PSScriptRoot/${test}_test.cpp" /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw "Compile failed: $test"}
    & "$output/$test.exe"
    if($LASTEXITCODE -ne 0){throw "Test failed: $test"}
}
foreach($unit in @('coop/creatures/geom_octa_sync','coop/creatures/geom_octa_relay','ue_wrap/actors/geom_octa')) {
    $name=$unit.Replace('/','_')
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" `
        "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/$unit.cpp"
    if($LASTEXITCODE -ne 0){throw "Production compile failed: $unit"}
}
if($FaultControls){
    $transformPath=Join-Path $output 'actor_transform.inc'
    $transform=[IO.File]::ReadAllText($transformPath)
    $readyPath=Join-Path $output 'session_world_ready.inc'
    $ready=[IO.File]::ReadAllText($readyPath)
    $sync=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/coop/creatures/geom_octa_sync.cpp'))
    try {
        foreach($case in @(
            @{Name='birth_required';Body=$sync;Before='if(!BirthKnown(*decision,slot,peer.generation,worldRevision))';After='if(false)'},
            @{Name='birth_receipt';Body=$sync;Before='if(!born)';After='if(false)'},
            @{Name='birth_recipient';Body=$sync;Before='birth.generation[slot]==generation';After='(birth.generation[31]==generation||birth.generation[1]==generation)'},
            @{Name='birth_generation';Body=$sync;Before='peer.generation!=session->peerGenerationForSlot(slot)';After='false'},
            @{Name='birth_world_reset';Body=$sync;Before='birth.worldRevision[slot]==worldRevision';After='true'},
            @{Name='session_ready_revision';Body=$ready;Before='if (!ready) slotWorldReadyRevision_[peerSlot].fetch_add(1, std::memory_order_acq_rel);';After='(void)ready;'},
            @{Name='transform_caller';Body=$transform;Before='(!callerCurrent||callerCurrent())';After='true'},
            @{Name='transform_world';Body=$transform;Before='generation==world_identity::Generation()';After='true'},
            @{Name='transform_atomic';Body=$transform;Before='if(!Call(actor,frame)';After="if(i==1)location=ownedLocation;`n        if(!Call(actor,frame)"}
        )){
            if(-not $case.Body.Contains($case.Before)){throw "Missing Geom fault anchor: $($case.Name)"}
            $mutated=$case.Body.Replace($case.Before,$case.After)
            if($case.Name.StartsWith('birth_')){
                $syncPath=Join-Path $output 'geom_octa_sync_fault.inc'
                [IO.File]::WriteAllText($syncPath,$mutated)
                $driver=Join-Path $output 'geom_octa_fault_test.cpp'
                $fixture=(Join-Path $PSScriptRoot 'geom_octa_lifetime_test.cpp').Replace('\','/')
                [IO.File]::WriteAllText($driver,"#define GEOM_SYNC_SOURCE `"geom_octa_sync_fault.inc`"`n#include `"$fixture`"`n")
            } elseif($case.Name.StartsWith('session_')) {
                [IO.File]::WriteAllText($readyPath,$mutated)
                $driver=Join-Path $PSScriptRoot 'session_world_ready_test.cpp'
            } else {
                [IO.File]::WriteAllText($transformPath,$mutated)
                $driver=Join-Path $PSScriptRoot 'actor_transform_test.cpp'
            }
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/geom_stubs" "/I$checkout/src/votv-coop/include" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $driver /link /OPT:REF
            if($LASTEXITCODE -ne 0){throw "Geom fault compile failed: $($case.Name)"}
            $saved=$ErrorActionPreference
            try {$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}
            finally {$ErrorActionPreference=$saved}
            [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
            if($code -eq 0 -or $report -notmatch '(?:FAIL \d+:|Assertion failed:)'){throw "Geom mutation did not reach an assertion: $($case.Name)"}
            Write-Output "PASS Geom birth fault $($case.Name)"
        }
    } finally {[IO.File]::WriteAllText($transformPath,$transform);[IO.File]::WriteAllText($readyPath,$ready)}
}
$global:LASTEXITCODE=0
