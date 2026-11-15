param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/llama-marker-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    foreach($fixture in @('native','sync')) {
        $extra=@();if($fixture -eq 'sync'){$extra=@('/I',(Join-Path $PSScriptRoot 'placed_save_stubs'))}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @extra /I src/votv-coop/include "/Fo$output/$fixture.obj" "/Fe$output/$fixture.exe" "$PSScriptRoot/llama_marker_${fixture}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE){throw "Llama marker $fixture fixture compile failed"}
        & "$output/$fixture.exe"
        if($LASTEXITCODE){throw "Llama marker $fixture fixture failed"}
    }
    foreach($unit in @('ue_wrap/actors/llama_marker','ue_wrap/actors/placed_save','ue_wrap/actors/placed_save_record','coop/world/placed_save_sync')) {
        $leaf=$unit.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$unit.cpp"
        if($LASTEXITCODE){throw "Llama marker production compile failed: $unit"}
    }
    if($FaultControls){
        $production=Join-Path $checkout 'src/votv-coop/src/ue_wrap/actors/llama_marker.cpp'
        $source=[IO.File]::ReadAllText($production)
        $fixture=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'llama_marker_native_test.cpp'))
        $controls=@(
            @{Name='retry';Before='g_cleanupNext=now+std::chrono::milliseconds(250);Cleanup();';After='g_cleanupNext=now+std::chrono::milliseconds(250);'},
            @{Name='spawn_reentry';Before='if(g_spawning||g_cleaning)return nullptr;';After='/* nested spawn allowed */'},
            @{Name='retry_pace';Before='if(now<g_cleanupNext)return;';After='/* no retry pace */'},
            @{Name='absent_world';Before='const auto world=world_identity::CurrentWorld();'+"`n"+'        if(!owned.actor.Get()';After='g_cleanup.reset();return;'+"`n"+'        const auto world=world_identity::CurrentWorld();'+"`n"+'        if(!owned.actor.Get()'}
        )
        foreach($control in $controls){
            $normalized=$source.Replace("`r`n","`n")
            if(-not $normalized.Contains($control.Before)){throw "Missing marker control: $($control.Name)"}
            $mutant=Join-Path $output ($control.Name+'_source.cpp')
            [IO.File]::WriteAllText($mutant,$normalized.Replace($control.Before,$control.After))
            $test=$fixture.Replace('../../src/votv-coop/src/ue_wrap/actors/llama_marker.cpp',$mutant.Replace('\','/'))
            $test=$test.Replace('../../src/votv-coop/',($checkout.Replace('\','/')+'/src/votv-coop/'))
            $testPath=Join-Path $output ($control.Name+'_test.cpp')
            [IO.File]::WriteAllText($testPath,$test)
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/$($control.Name).obj" "/Fe$output/$($control.Name).exe" $testPath /link /OPT:REF
            if($LASTEXITCODE){throw "Marker control compile failed: $($control.Name)"}
            & "$output/$($control.Name).exe"
            if($LASTEXITCODE -ne 1){throw "Marker control must fail an assertion: $($control.Name)"}
            Write-Output "PASS rejected marker control $($control.Name)"
        }
    }
}finally{Pop-Location}
$global:LASTEXITCODE=0
