param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/emote-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    foreach($name in @('emote_sync','emote_wrapper')) {
        & cl /nologo /std:c++20 /EHsc /utf-8 /I agent/tests/emote_stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp"
        if($LASTEXITCODE){throw "$name compile failed"}
        & "$output/$name.exe"
        if($LASTEXITCODE){throw "$name regression failed"}
    }
    & "$env:USERPROFILE/.local/bin/python3.15.exe" agent/tests/emote_catalogue_test.py
    if($LASTEXITCODE){throw 'EmoteWheel catalogue validation failed'}
    & "$env:USERPROFILE/.local/bin/python3.15.exe" agent/tests/emote_native_fingerprints_test.py
    if($LASTEXITCODE){throw 'EmoteWheel native fingerprint validation failed'}
    if($FaultControls){
        $cases=@(
            @{Name='sender_owner';File='coop/player/emote_sync.cpp';Macro='EMOTE_SYNC_SOURCE';Fixture='emote_sync_test.cpp';Before='sender!=p.slot';After='false'},
            @{Name='hand_generation';File='coop/player/emote_sync.cpp';Macro='EMOTE_SYNC_SOURCE';Fixture='emote_sync_test.cpp';Before='p.handGeneration!=hand_item::MirrorGeneration(slot)';After='false'},
            @{Name='source_mesh';File='ue_wrap/actors/emote_wheel.cpp';Macro='EMOTE_WHEEL_SOURCE';Fixture='emote_wrapper_test.cpp';Before='void* inst=Instance(pawn,L"Mesh");if(!inst)inst=Instance(pawn,L"mesh_playerVisible");';After='void* inst=Instance(pawn,L"mesh_playerVisible");'}
        )
        foreach($case in $cases){
            $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
            if(-not $source.Contains($case.Before)){throw "Missing fault anchor: $($case.Name)"}
            $mutant=Join-Path $output ($case.Name+'_source.cpp')
            [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
            $fixture=Join-Path $output ($case.Name+'_test.cpp')
            [IO.File]::WriteAllText($fixture,('#define '+$case.Macro+' "'+$mutant.Replace('\','/')+'"'+"`n"+'#include "'+(Join-Path $PSScriptRoot $case.Fixture).Replace('\','/')+'"'))
            & cl /nologo /std:c++20 /EHsc /utf-8 /I agent/tests/emote_stubs /I src/votv-coop/include "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $fixture
            if($LASTEXITCODE){throw "Fault compile failed: $($case.Name)"}
            & "$output/$($case.Name).exe"
            if($LASTEXITCODE -eq 0){throw "Injected fault escaped detection: $($case.Name)"}
            Write-Output "PASS rejected emote fault: $($case.Name)"
        }
    }
} finally {Pop-Location}
$global:LASTEXITCODE=0
