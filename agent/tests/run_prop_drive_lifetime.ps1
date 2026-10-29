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
        @{Name='pose_serial';Before='if (void* live = it->second.LiveActor())';After='if (void* live = it->second.d.LiveActor())'},
        @{Name='host_end_serial';Before='if (void* actor = dr.LiveActor())';After='if (void* actor = dr.d.LiveActor())'},
        @{Name='spin';Before='(lin2 > 0.f || ang2 > 0.f)';After='lin2 > 0.f'}
    )
    foreach($case in $cases) {
        $normalized=$source.Replace("`r`n","`n")
        if(-not $normalized.Contains($case.Before)){throw "Mutation target missing: $($case.Name)"}
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$normalized.Replace($case.Before,$case.After))
        $test=$harness.Replace('../../src/votv-coop/src/coop/props/prop_drive_stream.cpp',$mutant.Replace('\','/'))
        $test=$test.Replace('../../src/votv-coop/',($checkout.Replace('\','/')+'/src/votv-coop/'))
        $testPath=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($testPath,$test)
        Compile $case.Name $testPath
        & "$output/$($case.Name).exe"
        if($LASTEXITCODE -ne 1){throw "Mutation must fail an assertion: $($case.Name), exit $LASTEXITCODE"}
        Write-Output "PASS rejected prop-drive control $($case.Name)"
    }
}
$global:LASTEXITCODE=0
