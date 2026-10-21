param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/watoy-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/watoy_sync.obj" "/Fe$output/watoy_sync.exe" agent/tests/watoy_sync_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Watoy controller test compile failed'}
    & "$output/watoy_sync.exe"
    if($LASTEXITCODE -ne 0){throw 'Watoy controller test failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/watoy_wrapper.obj" "/Fe$output/watoy_wrapper.exe" agent/tests/watoy_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Watoy wrapper test compile failed'}
    & "$output/watoy_wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Watoy wrapper test failed'}
    if($CompileSources) {
        foreach($file in @('ue_wrap/actors/watoy','coop/props/watoy_sync','coop/dispatch/event_dispatch_world','coop/session/subsystems')) {
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Watoy source compile failed: $file"}
        }
    }
} finally {Pop-Location}
