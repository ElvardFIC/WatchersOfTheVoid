param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/grime-join-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/grime_stubs /I agent/tests/placed_save_stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/grime_join_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grime join compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Grime join failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/grime_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grime wrapper compile failed'}
    & "$output/wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Grime wrapper failed'}
    & cl /nologo /std:c++20 /EHa /Gy /utf-8 /I src/votv-coop/include "/Fo$output/invocation.obj" "/Fe$output/invocation.exe" agent/tests/gate_invocation_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Gate invocation compile failed'}
    & "$output/invocation.exe"
    if($LASTEXITCODE -ne 0){throw 'Gate invocation failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/grime_stubs /I agent/tests/placed_save_stubs /I src/votv-coop/include "/Fo$output/window.obj" "/Fe$output/window.exe" agent/tests/window_observation_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Window observation compile failed'}
    & "$output/window.exe"
    if($LASTEXITCODE -ne 0){throw 'Window observation failed'}
    if($CompileSources){foreach($file in @('coop/interactables/grime_sync','coop/session/subsystems','ue_wrap/devices/grime','ue_wrap/core/script_gate','coop/interactables/window_sync')){
        $name=$file.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
        if($LASTEXITCODE -ne 0){throw "Grime join source failed: $file"}
    }}
} finally {Pop-Location}
