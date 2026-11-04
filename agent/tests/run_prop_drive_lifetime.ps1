param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/prop-drive-lifetime-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
. (Join-Path $checkout '../_toolchain/env.ps1')
$fixture=Join-Path $PSScriptRoot 'prop_drive_2123_test.cpp'
$production=Join-Path $checkout 'src/votv-coop/src/coop/props/prop_drive_stream.cpp'
$includes=@("/I$PSScriptRoot/upstream_refresh_stubs","/I$PSScriptRoot/stubs","/I$checkout/src/votv-coop/include","/I$checkout/src/votv-coop/src")
function Compile([string]$name,[string]$path) {
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/$name.obj" "/Fe$output/$name.exe" $path /link /OPT:REF
    if($LASTEXITCODE){throw "Prop drive compile failed: $name"}
}
Compile 'test' $fixture
& "$output/test.exe"
if($LASTEXITCODE){throw 'Prop drive regression failed'}
if($FaultControls) {
    $source=[IO.File]::ReadAllText($production)
    $harness=[IO.File]::ReadAllText($fixture)
    $cases=@(
        @{Name='late_bind';Before='g_endedGen.erase(ended);'+"`n"+'    OnEnd(p);';After='OnEnd(p);'},
        @{Name='pose_serial';Before='if (void* live = it->second.LiveActor())';After='if (void* live = it->second.d.LiveActor())';ExtraBefore='owned.LiveActor()';ExtraAfter='owned.d.LiveActor()'},
        @{Name='host_end_serial';Before='if (void* actor = dr.LiveActor())';After='if (void* actor = dr.d.LiveActor())';ExtraBefore='dr.LiveActor() == actor && !g_drives.contains(p.eid)';ExtraAfter='dr.d.LiveActor() == actor && !g_drives.contains(p.eid)'},
        @{Name='spin';Before='(lin2 > 0.f || ang2 > 0.f)';After='lin2 > 0.f'},
        @{Name='host_generation';Before='if (!drive_generation::AtLeast(p.gen, dr.gen)) return;';After='/* no generation guard */'},
        @{Name='end_continuation';Before='!E::SetActorLocation(actor, ue_wrap::FVector{p.x, p.y, p.z}) || !current() ||';After='!E::SetActorLocation(actor, ue_wrap::FVector{p.x, p.y, p.z}) ||'},
        @{Name='component_owner';Before='return current() && component.Get() && componentClass.Get() &&'+"`n"+'                R::ClassOf(component.Get()) == componentClass.Get() && PR::GetStaticMesh(actor) == component.Get();';After='return current();'},
        @{Name='nested_tick';Before='if (!BindSession(s) || g_ticking) return;';After='if (!BindSession(s)) return;'},
        @{Name='pose_revision';Before='found->second.revision == owned.revision';After='true'},
        @{Name='tick_revision';Before='found->second.revision == dr.revision';After='true'}
        @{Name='input_owner';Before='const auto p = input;';After='const auto& p = input;'}
    )
    foreach($case in $cases) {
        $normalized=$source.Replace("`r`n","`n")
        if(-not $normalized.Contains($case.Before)){throw "Mutation target missing: $($case.Name)"}
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        $changed=$normalized.Replace($case.Before,$case.After)
        if($case.ExtraBefore){
            if(-not $changed.Contains($case.ExtraBefore)){throw "Mutation extra target missing: $($case.Name)"}
            $changed=$changed.Replace($case.ExtraBefore,$case.ExtraAfter)
        }
        [IO.File]::WriteAllText($mutant,$changed)
        $test=$harness.Replace('../../src/votv-coop/src/coop/props/prop_drive_stream.cpp',$mutant.Replace('\','/'))
        $test=$test.Replace('../../src/votv-coop/',($checkout.Replace('\','/')+'/src/votv-coop/'))
        $testPath=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($testPath,$test)
        Compile $case.Name $testPath
        $saved=$ErrorActionPreference
        try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
        [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
        if($code -ne 1 -or $report -notmatch 'FAIL (?:line )?\d+:'){throw "Mutation must fail an assertion: $($case.Name), exit $code"}
        Write-Output "PASS rejected prop-drive control $($case.Name)"
    }
}
$global:LASTEXITCODE=0
Write-Output 'PASS prop-drive validation'
