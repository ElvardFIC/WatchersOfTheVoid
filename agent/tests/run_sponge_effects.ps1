param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/sponge-effects-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/sponge_effects_sync_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Sponge effects controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Sponge effects controller failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/sponge_effects_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Sponge effects wrapper compile failed'}
    & "$output/wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Sponge effects wrapper failed'}
    if($CompileSources) {
        foreach($file in @('ue_wrap/actors/sponge_effects','coop/props/sponge_effects_sync','coop/dispatch/event_dispatch_world','coop/session/subsystems')) {
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Sponge effects source compile failed: $file"}
        }
    }
} finally {Pop-Location}
