param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/child-release-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
. (Join-Path $root '../_toolchain/env.ps1')
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/child_release_stubs" "/I$root/src/votv-coop/include" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/child_release_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Child release fixture compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Child release fixture failed'}
if($FaultControls) {
    $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_child_release.cpp'))
    $cases=@(
        @{Name='parent_disappearance';Before='if (!w->parent.Current()) { w->ready = true; w->restorePose = false; }';After='if (!w->parent.Current()) { Erase(w); continue; }'},
        @{Name='clock_underflow';Before='nowMs >= w->sinceMs && ';After=''},
        @{Name='recursive_pump';Before='if (g_pumping || g_draining) return;';After='if (g_draining) return;'},
        @{Name='hand_after_location';Before='if (!PoseCurrent(w) || !located) return;';After='if (!located) return;'},
        @{Name='location_refusal';Before='if (!PoseCurrent(w) || !located) return;';After='if (!PoseCurrent(w)) return;'},
        @{Name='rotation_refusal';Before='if (!PoseCurrent(w) || !rotated) return;';After='if (!PoseCurrent(w)) return;'},
        @{Name='mesh_binding';Before=' || !bound';After=''},
        @{Name='teardown_capture';Before='if (g_draining || !door || !parent || door == parent) return;';After='if (!door || !parent || door == parent) return;'},
        @{Name='early_pose';Before='bool hasPose = false, capturing = true';After='bool hasPose = true, capturing = true'},
        @{Name='release_receipt';Before='Erase(w); // consume before native unpark; an uncertain dispatched release is never retried';After='/* receipt removed */'},
        @{Name='parent_identity';Before='&& w->parent.object.Is(parent)';After=''}
    )
    foreach($case in $cases) {
        if(-not $source.Contains($case.Before)){throw "Missing fault anchor: $($case.Name)"}
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $fixture=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($fixture,('#define CHILD_RELEASE_SOURCE "'+$mutant.Replace('\','/')+'"'+"`n"+'#include "'+(Join-Path $PSScriptRoot 'child_release_test.cpp').Replace('\','/')+'"'))
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/child_release_stubs" "/I$root/src/votv-coop/include" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Fault compile failed: $($case.Name)"}
        & "$output/$($case.Name).exe"
        if($LASTEXITCODE -ne 1){throw "Fault was not rejected by assertion: $($case.Name)"}
        Write-Output "PASS rejected fault: $($case.Name)"
    }
}
$global:LASTEXITCODE=0
