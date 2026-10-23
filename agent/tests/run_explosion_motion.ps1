param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/explosion-motion-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Run-Harness([string]$Source,[string]$Name,[int]$Expected=0) {
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/blood_stubs" "/I$checkout/src/votv-coop/include" `
        "/Fo$output/$Name.obj" "/Fe$output/$Name.exe" $Source /link /OPT:REF
    if($LASTEXITCODE){throw "Explosion motion compile failed: $Name"}
    & "$output/$Name.exe"
    if($LASTEXITCODE -ne $Expected){throw "Explosion motion $Name returned $LASTEXITCODE, expected $Expected"}
}
Run-Harness "$PSScriptRoot/explosion_motion_test.cpp" 'controller'
Run-Harness "$PSScriptRoot/radial_impulse_test.cpp" 'radial-wrapper'
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/explosion_motion_native_test.py"
if($LASTEXITCODE){throw 'Native explosion motion contract failed'}
if($FaultControls){
    $path=Join-Path $checkout 'src/votv-coop/src/coop/props/explosion_prop_motion.cpp'
    $source=[IO.File]::ReadAllText($path)
    $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'explosion_motion_test.cpp'))
    foreach($case in @(
        @{Name='early';Before='free->blast.Set(blast);';After='free->blast.Set(blast); free->ready=true;'},
        @{Name='wrong-source';Before='if (g_targets[i].blast.Is(blast))';After='if (true)'},
        @{Name='held';Before='!local_prop_hold::IsHeld(actor)';After='true'},
        @{Name='session';Before='g_serial==net::session_serial::Current()';After='true'}
        @{Name='no-force-query';Before='if (sampled) {';After='if (false) {'}
    )) {
        if(-not $source.Contains($case.Before)){throw "Mutation site missing: $($case.Name)"}
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $test=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($test,$harness.Replace('../../src/votv-coop/src/coop/props/explosion_prop_motion.cpp',($mutant -replace '\\','/')))
        Run-Harness $test $case.Name 1
        Write-Output "PASS expected-failure control: $($case.Name)"
    }
}
$global:LASTEXITCODE=0
