param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/physics-native-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
. (Join-Path $root '../_toolchain/env.ps1')
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$root/src/votv-coop/include" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/physics_native_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Native physics fixture compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Native physics fixture failed'}
if($FaultControls) {
    $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/engine/engine_physics.cpp'))
    $cases=@(
        @{Name='frame_budget';Before='next.frame > kFrameBytes';After='false'},
        @{Name='parameter_span';Before='off <= frame - size';After='off < frame'},
        @{Name='parameter_size';Before='f.size != bytes';After='false'},
        @{Name='overlap';Before='a.off < b.off + b.size && b.off < a.off + a.size';After='false'},
        @{Name='query_mask';Before='(f[l.fields[1].off] & l.fields[1].mask) != 0';After='f[l.fields[1].off] != 0'},
        @{Name='native_retirement';Before='if (!ok || !current() || !layout.Current()) return false;';After='if (!ok) return false;'},
        @{Name='native_refusal';Before='if (!ok || !current() || !layout.Current()) return false;';After='if (!current() || !layout.Current()) return false;'},
        @{Name='finite_velocity';Before='if (!Finite(value)) return false;';After=''},
        @{Name='deduplicate_fields';Before='if (std::find(next.offsets.begin(), next.offsets.end(), off) != next.offsets.end()) continue;';After=''},
        @{Name='successful_stop';Before='if (SimulateWhile(comp, false, current)) ++stopped;';After='SimulateWhile(comp, false, current); ++stopped;'},
        @{Name='recursive_stop';Before='if (g_stopping) return 0;';After=''}
    )
    foreach($case in $cases) {
        if(-not $source.Contains($case.Before)){throw "Missing fault anchor: $($case.Name)"}
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $fixture=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($fixture,('#define PHYSICS_SOURCE "'+$mutant.Replace('\','/')+'"'+"`n"+'#include "'+(Join-Path $PSScriptRoot 'physics_native_test.cpp').Replace('\','/')+'"'))
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8  "/I$root/src/votv-coop/include" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Fault compile failed: $($case.Name)"}
        & "$output/$($case.Name).exe"
        if($LASTEXITCODE -ne 1){throw "Fault was not rejected by assertion: $($case.Name)"}
        Write-Output "PASS rejected fault: $($case.Name)"
    }
}
$global:LASTEXITCODE=0
