param([switch]$CompileSources, [switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(-not(Get-Command cl -ErrorAction SilentlyContinue)){throw 'Run from an x64 MSVC developer shell.'}
$output=Join-Path $checkout 'build/ragdoll-native-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/native.obj" "/Fe$output/native.exe" "$PSScriptRoot/ragdoll_native_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Native ragdoll regression compile failed'}
& "$output/native.exe" attach
if($LASTEXITCODE -ne 0){throw 'Native ragdoll attachment regression failed'}
& "$output/native.exe"
if($LASTEXITCODE -ne 0){throw 'Native ragdoll model entry regression failed'}
& "$output/native.exe" camera
if($LASTEXITCODE -ne 0){throw 'Native ragdoll camera regression failed'}
& "$output/native.exe" momentum
if($LASTEXITCODE -ne 0){throw 'Native ragdoll momentum regression failed'}
& "$output/native.exe" transition
if($LASTEXITCODE -ne 0){throw 'Native ragdoll construction/recovery transition failed'}
if($FaultControls){
    # Compile the actual substrate with its camera correction disabled. The same
    # regression must reject it; a positive-only camera test could be blind.
    $production=Join-Path $checkout 'src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp'
    $source=[IO.File]::ReadAllText($production)
    $needle='bool Restore() const {'
    if(-not $source.Contains($needle)){throw 'Camera negative-control injection point missing'}
    $mutant=Join-Path $output 'camera-disabled.cpp'
    [IO.File]::WriteAllText($mutant,$source.Replace($needle,"$needle`n        return false; // regression control"))
    $fixture=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ragdoll_native_test.cpp'))
    $fixture=$fixture.Replace('../../src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp',$mutant.Replace('\','/'))
    $fixture=$fixture.Replace('../../src/votv-coop/src/ue_wrap/core/call.cpp',"$checkout/src/votv-coop/src/ue_wrap/core/call.cpp".Replace('\','/'))
    $testSource=Join-Path $output 'camera-disabled-test.cpp'
    [IO.File]::WriteAllText($testSource,$fixture)
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" `
        "/Fo$output/camera-disabled.obj" "/Fe$output/camera-disabled.exe" $testSource /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Camera negative control did not compile'}
    $controlOutput= & "$output/camera-disabled.exe" camera 2>&1
    $controlExit=$LASTEXITCODE
    $controlOutput | Set-Content -LiteralPath (Join-Path $output 'camera-disabled.log')
    if($controlExit -eq 0 -or ($controlOutput -join "`n") -notmatch 'cameraWrites==1'){
        throw 'Camera regression did not reject the removed correction for the expected reason'
    }
    Write-Output 'PASS negative control: removing the camera correction is rejected'
    $needle='script_gate::Verdict HoldRagdollVelocity(void* component, void*, const uint8_t* params) {'
    if(-not $source.Contains($needle)){throw 'Momentum negative-control injection point missing'}
    $mutant=Join-Path $output 'momentum-disabled.cpp'
    [IO.File]::WriteAllText($mutant,$source.Replace($needle,"$needle`n    return script_gate::Verdict::Run; // regression control"))
    $fixture=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ragdoll_native_test.cpp'))
    $fixture=$fixture.Replace('../../src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp',$mutant.Replace('\','/'))
    $fixture=$fixture.Replace('../../src/votv-coop/src/ue_wrap/core/call.cpp',"$checkout/src/votv-coop/src/ue_wrap/core/call.cpp".Replace('\','/'))
    $testSource=Join-Path $output 'momentum-disabled-test.cpp'
    [IO.File]::WriteAllText($testSource,$fixture)
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" `
        "/Fo$output/momentum-disabled.obj" "/Fe$output/momentum-disabled.exe" $testSource /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Momentum negative control did not compile'}
    $controlOutput= & "$output/momentum-disabled.exe" momentum 2>&1
    $controlExit=$LASTEXITCODE
    $controlOutput | Set-Content -LiteralPath (Join-Path $output 'momentum-disabled.log')
    if($controlExit -eq 0 -or -not ($controlOutput -join "`n").Contains('velocities[1].X-600')){throw 'Momentum regression did not reject disabled command holding'}
    Write-Output 'PASS negative control: dropping native warmup commands is rejected'
    foreach($control in @(
        @{Name='construction';Needle='void OnLocalRagdollConstructed(void*, void* source, void* result) {';Failure='configurations==1 && bodySim'},
        @{Name='recovery';Needle='void OnNativeRagdollArmAttached(void* component, void* source, void*) {';Failure='actorYaw==yaw'}
    )){
        if(-not $source.Contains($control.Needle)){throw 'Transition negative-control seam missing'}
        $mutant=Join-Path $output ($control.Name+'-disabled.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($control.Needle,$control.Needle+"`n    return; // regression control"))
        $fixture=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ragdoll_native_test.cpp'))
        $fixture=$fixture.Replace('../../src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp',$mutant.Replace('\','/'))
        $fixture=$fixture.Replace('../../src/votv-coop/src/ue_wrap/core/call.cpp',"$checkout/src/votv-coop/src/ue_wrap/core/call.cpp".Replace('\','/'))
        $testSource=Join-Path $output ($control.Name+'-disabled-test.cpp')
        [IO.File]::WriteAllText($testSource,$fixture)
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" `
            "/Fo$output/$($control.Name)-disabled.obj" "/Fe$output/$($control.Name)-disabled.exe" $testSource /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw 'Transition negative control did not compile'}
        $controlOutput=& "$output/$($control.Name)-disabled.exe" transition 2>&1
        $controlExit=$LASTEXITCODE
        $controlOutput | Set-Content -LiteralPath (Join-Path $output ($control.Name+'-disabled.log'))
        if($controlExit -eq 0 -or -not ($controlOutput -join "`n").Contains($control.Failure)){throw 'Transition regression did not reject missing native lifecycle correction'}
        Write-Output "PASS negative control: removed $($control.Name) correction is rejected"
    }
}
if($CompileSources){foreach($unit in @('ue_wrap/engine/engine_playerragdoll','coop/player/local_streams','coop/player/local_body')){
    $name=$unit.Replace('/','_')
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/$unit.cpp"
    if($LASTEXITCODE -ne 0){throw "Ragdoll production compile failed: $unit"}
}}
