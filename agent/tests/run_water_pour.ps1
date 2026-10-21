param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/water-pour-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/material_2153_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/water_pour_sync_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Water pour controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Water pour controller failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/material_2153_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/washer.obj" "/Fe$output/washer.exe" agent/tests/washer_shot_sync_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Washer shot controller compile failed'}
    & "$output/washer.exe"
    if($LASTEXITCODE -ne 0){throw 'Washer shot controller failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/material_2153_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/bucket_sound.obj" "/Fe$output/bucket_sound.exe" agent/tests/bucket_sound_sync_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Bucket sound controller compile failed'}
    & "$output/bucket_sound.exe"
    if($LASTEXITCODE -ne 0){throw 'Bucket sound controller failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/water_pour_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Water pour wrapper compile failed'}
    & "$output/wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Water pour wrapper failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/floor_stubs /I src/votv-coop/include "/Fo$output/birth.obj" "/Fe$output/birth.exe" agent/tests/floor_host_2132_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Finished water birth compile failed'}
    & "$output/birth.exe"
    if($LASTEXITCODE -ne 0){throw 'Finished water birth failed'}
    if($CompileSources){
        foreach($source in @('coop/props/water_pour_sync','ue_wrap/actors/water_pour','coop/props/prop_drop_intent','coop/props/host_spawn_watcher','coop/dispatch/event_dispatch_world','coop/session/subsystems')){
            $name=$source.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
            if($LASTEXITCODE -ne 0){throw "Water pour source compile failed: $source"}
        }
    }
}finally {Pop-Location}
