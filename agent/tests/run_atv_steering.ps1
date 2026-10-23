param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/atv-steering-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source=Get-Content (Join-Path $checkout 'src/votv-coop/src/coop/interactables/atv_sync.cpp') -Raw
$snapshot=[regex]::Match($source,'(?ms)^void QueueConnectBroadcastForSlot\(.*?^\}')
if(-not $snapshot.Success){throw 'Production ATV join function missing'}
[IO.File]::WriteAllText((Join-Path $output 'connect_snapshot.inc'),$snapshot.Value,[Text.UTF8Encoding]::new($false))
$read=[regex]::Match($source,'(?ms)^bool ReadPayload\(.*?^\}')
if(-not $read.Success){throw 'Production ATV payload function missing'}
[IO.File]::WriteAllText((Join-Path $output 'read_payload.inc'),$read.Value,[Text.UTF8Encoding]::new($false))
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/I$PSScriptRoot/blood_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/steering.obj" "/Fe$output/steering.exe" "$PSScriptRoot/atv_steering_test.cpp"
if($LASTEXITCODE){throw 'ATV steering regression compile failed'}
& "$output/steering.exe"
if($LASTEXITCODE){throw 'ATV steering regression failed'}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/atv_steering_native_test.py"
if($LASTEXITCODE){throw 'Native ATV steering contract failed'}
if($FaultControls){
    foreach($name in @('missing-steering','join-clears-honk')){
        $control=Join-Path $output $name
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $readControl=$read.Value
        $joinControl=$snapshot.Value
        if($name -eq 'missing-steering'){
            $readControl=$readControl.Replace('p.steering = coop::atv_steering_sync::Capture(actor);','p.steering = 0;')
            if($readControl -eq $read.Value){throw 'Missing steering mutation site'}
        }else{
            $joinControl=$joinControl.Replace('// A join snapshot carries state, not a pending one-shot for existing peers.','e.honkPulse = false;')
            if($joinControl -eq $snapshot.Value){throw 'Missing join mutation site'}
        }
        [IO.File]::WriteAllText((Join-Path $control 'read_payload.inc'),$readControl,[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $control 'connect_snapshot.inc'),$joinControl,[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /utf-8 "/I$control" "/I$PSScriptRoot/blood_stubs" "/I$checkout/src/votv-coop/include" `
            "/Fo$control/control.obj" "/Fe$control/control.exe" "$PSScriptRoot/atv_steering_test.cpp"
        if($LASTEXITCODE){throw "Control compile failed: $name"}
        & "$control/control.exe"
        if($LASTEXITCODE -ne 1){throw "Control did not fire its assertion: $name"}
        Write-Output "PASS expected-failure control: $name"
    }
}
