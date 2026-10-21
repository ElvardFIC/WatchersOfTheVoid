param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/window-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    foreach($test in @('window_delivery','window_wrapper')){
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/grime_stubs /I agent/tests/placed_save_stubs /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Window test compile failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE -ne 0){throw "Window test failed: $test"}
    }
    if($CompileSources){foreach($file in @('coop/interactables/window_sync','ue_wrap/devices/base_window','coop/session/subsystems')){
        $name=$file.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
        if($LASTEXITCODE -ne 0){throw "Window source compile failed: $file"}
    }}
}finally{Pop-Location}
