param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/grill-hand-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/grill_hand_sync_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grill hand controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Grill hand controller failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/persistence.obj" "/Fe$output/persistence.exe" agent/tests/grill_saved_state_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grill persistence compile failed'}
    & "$output/persistence.exe"
    if($LASTEXITCODE -ne 0){throw 'Grill persistence failed'}
    if($CompileSources){
        foreach($file in @('coop/interactables/grill_hand_sync','ue_wrap/devices/grill','ue_wrap/devices/grill_saved_state','coop/world/material_save_sync','coop/items/player_inventory_sync','coop/interactables/kitchen_sync','coop/dispatch/event_dispatch_world')){
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Grill hand source compile failed: $file"}
        }
    }
} finally {Pop-Location}
