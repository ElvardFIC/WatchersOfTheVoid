param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/glowstick-grab-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    $drive=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/remote_prop_drive.cpp'))
    $helper=[regex]::Match($drive,'(?ms)^bool PrepareGrabPrelude\(.*?^\}').Value
    if(-not $helper){throw 'Production prelude admission missing'}
    $inc=Join-Path $output 'grab_prelude.inc'
    [IO.File]::WriteAllText($inc,$helper)
    function Run([string]$name,[string]$source,[bool]$negative=$false) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/stubs" /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source /link /OPT:REF
        if($LASTEXITCODE){throw "Compile failed: $name"}
        & "$output/$name.exe"
        if($negative){if($LASTEXITCODE -ne 1){throw "Expected assertion failure: $name"};Write-Output "PASS rejected control: $name"}
        elseif($LASTEXITCODE){throw "Test failed: $name"}
    }
    Run 'wrapper' (Join-Path $PSScriptRoot 'glowstick_grab_wrapper_test.cpp')
    Run 'admission' (Join-Path $PSScriptRoot 'remote_grab_prelude_test.cpp')
    & (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') agent/tests/glowstick_native_test.py
    if($LASTEXITCODE){throw 'Native glowstick contract failed'}
    if($FaultControls){
        try {
            foreach($case in @(
                @{Name='repeat';Before='if (hold.preludeAttempted) return hold.preludeSucceeded;';After=''},
                @{Name='reentry';Before='if (hold.preludeRunning) return false;';After=''},
                @{Name='world';Before='world != ue_wrap::world_identity::Generation() || actor.Get() != prop ||';After='!actor.Is(prop) ||'},
                @{Name='uncertain';Before='hold.preludeAttempted = result != Result::NotReady;';After='hold.preludeAttempted = result == Result::Applied;'}
            )) {
                if(-not $helper.Contains($case.Before)){throw "Missing control: $($case.Name)"}
                [IO.File]::WriteAllText($inc,$helper.Replace($case.Before,$case.After))
                Run $case.Name (Join-Path $PSScriptRoot 'remote_grab_prelude_test.cpp') $true
            }
        } finally {[IO.File]::WriteAllText($inc,$helper)}
        $wrapper=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/prop_events.cpp'))
        foreach($case in @(
            @{Name='glow_owner';Before='BaseEvent& event = isGlowstick ? g_glowstickRelease : g_grabPrelude;';After='BaseEvent& event = g_grabPrelude;'},
            @{Name='function_serial';Before='if (void* fn = e.fn.Get()) return fn;';After='if (void* fn = e.fn.Raw()) return fn;'}
        )) {
            if(-not $wrapper.Contains($case.Before)){throw "Missing wrapper control: $($case.Name)"}
            $mutant=Join-Path $output ($case.Name+'_source.cpp')
            [IO.File]::WriteAllText($mutant,$wrapper.Replace($case.Before,$case.After))
            $fixture=Join-Path $output ($case.Name+'_test.cpp')
            $lines=@(('#define GRAB_WRAPPER_SOURCE "'+$mutant.Replace('\','/')+'"'),
                ('#include "'+(Join-Path $PSScriptRoot 'glowstick_grab_wrapper_test.cpp').Replace('\','/')+'"'))
            [IO.File]::WriteAllText($fixture,($lines -join "`n"))
            Run $case.Name $fixture $true
        }
    }
    $global:LASTEXITCODE=0
} finally {Pop-Location}
