param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/toy-music-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/dingus_music_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Toy music wrapper compile failed'}
    & "$output/wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Toy music wrapper failed'}
    foreach($test in @('toy_music_phase','toy_music_clock_wrapper')){
        $includes=@('/I','src/votv-coop/include')
        if($test -eq 'toy_music_phase'){$includes=@('/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')+$includes}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Toy music compile failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE -ne 0){throw "Toy music failed: $test"}
    }
    if($CompileSources){
        foreach($file in @('ue_wrap/actors/dingus_music','ue_wrap/core/component_calls','coop/player/hand_toy_audio','coop/props/dingus_music_sync')){
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Toy music source compile failed: $file"}
        }
    }
} finally {Pop-Location}
