param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/tower-wrapper-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
. (Join-Path $root '../_toolchain/env.ps1')
foreach($kind in @('coordinate','radio')) {
    [string[]]$defines=@(if($kind -eq 'radio'){'/DTEST_RADIO'})
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @defines "/I$root/src/votv-coop/include" "/Fo$output/$kind.obj" "/Fe$output/$kind.exe" "$PSScriptRoot/tower_wrapper_test.cpp" /link /OPT:REF
    if($LASTEXITCODE){throw "Tower wrapper compile failed: $kind"}
    & "$output/$kind.exe"
    if($LASTEXITCODE){throw "Tower wrapper regression failed: $kind"}
}
if($FaultControls) {
    $cases=@(
        @{Name='class';Source='wrapper';Before='R::NameEquals(R::NameOf(cls.Raw()), kClassName)';After='true'},
        @{Name='class_reload';Source='wrapper';Before='if (g_cls.Is(cls)) return g_membersResolved && !g_latchedOff;';After='if (g_membersResolved) { g_cls.Set(cls); return true; }'},
        @{Name='field_extent';Source='helper';Before='off <= size - bytes';After='off < size'},
        @{Name='array_capacity';Source='helper';Before='a.max >= a.num';After='true'},
        @{Name='read_retirement';Source='wrapper';Before='ready && TA::Lifetime::Current() &&';After='ready &&'},
        @{Name='function_retirement';Source='helper';Before='&& function.Get();';After=';'},
        @{Name='frame_budget';Source='helper';Before='bytes > 1024';After='bytes > 65536'},
        @{Name='input_ownership';Source='wrapper';Before='const LookAt at = input;';After='const LookAt& at = input;'},
        @{Name='reentry';Source='wrapper';Before='if (g_interacting) return false;';After=''},
        @{Name='restore_ownership';Source='wrapper';Before='scope.Current() && SameLook(SaveLook(scope.actor.Raw()), staged)';After='scope.Current()'},
        @{Name='restore_unwind';Source='wrapper';Before='if (scope.Current() && SameLook(SaveLook(scope.actor.Raw()), staged)) RestoreLook(scope.actor.Raw(), saved);';After=''},
        @{Name='fuse_value';Source='wrapper';Before=' || value > 2';After=''},
        @{Name='part_binding';Source='wrapper';Before='PartComponent(tower, at) == part';After='true'}
    )
    foreach($kind in @('coordinate','radio')) {
        $file=if($kind -eq 'radio'){'radio_tower.cpp'}else{'coord_tower.cpp'}
        $source=[IO.File]::ReadAllText((Join-Path $root "src/votv-coop/src/ue_wrap/devices/$file"))
        foreach($case in $cases) {
            $stem=$kind+'_'+$case.Name
            $helperPath=Join-Path $output ($stem+'_access.h')
            $helper=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/devices/tower_access.h'))
            $changed=$source
            if($case.Source -eq 'helper') {
                if(-not $helper.Contains($case.Before)){throw "Missing helper fault anchor: $stem"}
                $helper=$helper.Replace($case.Before,$case.After)
                # The frame cap is checked before allocation and again on the constructed frame.
                if($case.Name -eq 'frame_budget'){$helper=$helper.Replace('frame.FrameSize() > 1024','frame.FrameSize() > 65536')}
            } else {
                if(-not $source.Contains($case.Before)){throw "Missing wrapper fault anchor: $stem"}
                $changed=$source.Replace($case.Before,$case.After)
            }
            [IO.File]::WriteAllText($helperPath,$helper)
            $changed=$changed.Replace('#include "tower_access.h"',('#include "'+$helperPath.Replace('\','/')+'"'))
            $sourcePath=Join-Path $output ($stem+'_source.cpp')
            [IO.File]::WriteAllText($sourcePath,$changed)
            $fixture=Join-Path $output ($stem+'_test.cpp')
            $prefix=if($kind -eq 'radio'){"#define TEST_RADIO`n"}else{''}
            [IO.File]::WriteAllText($fixture,($prefix+'#define TOWER_WRAPPER_SOURCE "'+$sourcePath.Replace('\','/')+'"'+"`n"+'#include "'+(Join-Path $PSScriptRoot 'tower_wrapper_test.cpp').Replace('\','/')+'"'))
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$root/src/votv-coop/include" "/Fo$output/$stem.obj" "/Fe$output/$stem.exe" $fixture /link /OPT:REF
            if($LASTEXITCODE){throw "Tower fault compile failed: $stem"}
            & "$output/$stem.exe"
            if($LASTEXITCODE -ne 1){throw "Tower fault was not rejected by an assertion: $stem"}
            Write-Output "PASS rejected tower wrapper fault: $stem"
        }
    }
}
$global:LASTEXITCODE=0
