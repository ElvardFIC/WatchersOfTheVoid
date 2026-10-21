param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/drink-spill-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    foreach($test in @('drink_spill','drink_pour','drink_spill_wrapper','hand_generation')){
        $includes=@('/I','src/votv-coop/include')
        if($test -in @('drink_spill','drink_pour')){$includes=@('/I','agent/tests/grime_stubs','/I','agent/tests/placed_save_stubs')+$includes}
        if($test -eq 'hand_generation'){$includes=@('/I','agent/tests/hand_stubs','/I','agent/tests/stubs')+$includes}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Drink-spill compile failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE -ne 0){throw "Drink-spill test failed: $test"}
    }
    if($CompileSources){foreach($file in @('coop/props/drink_spill_sync','ue_wrap/devices/drink_spill','coop/player/hand_item','coop/interactables/grime_sync','coop/session/subsystems','coop/dispatch/event_dispatch_world')){
        $name=$file.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
        if($LASTEXITCODE -ne 0){throw "Drink-spill source failed: $file"}
    }}
}finally{Pop-Location}
