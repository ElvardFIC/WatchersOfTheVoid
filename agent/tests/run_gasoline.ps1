param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/gasoline-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    foreach($test in @('gasoline_spill','gasoline_hand','prop_fire_2137','gasoline_fire','prop_fire_wrapper_2137','gasoline_fire_wrapper')){
        $includes=if($test -in @('gasoline_spill','gasoline_hand')){@('/I','agent/tests/grime_stubs','/I','agent/tests/placed_save_stubs')}else{@('/I','agent/tests/fire_stubs','/I','agent/tests/stubs')}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Gasoline compile failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE -ne 0){throw "Gasoline test failed: $test"}
    }
    if($CompileSources){foreach($file in @('coop/props/prop_fire_sync','ue_wrap/actors/prop_fire')){
        $name=$file.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
        if($LASTEXITCODE -ne 0){throw "Gasoline source failed: $file"}
    }}
}finally{Pop-Location}
