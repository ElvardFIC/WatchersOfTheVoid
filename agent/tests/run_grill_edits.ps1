param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/grill-edits-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/grill_edits_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grill edit controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Grill edit controller failed'}
    if($CompileSources){
        foreach($source in @('coop/interactables/grill_edits','coop/interactables/kitchen_sync','ue_wrap/devices/grill','ue_wrap/devices/grill_saved_state','coop/props/prop_save_data','coop/dispatch/event_dispatch_world')){
            $name=$source.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
            if($LASTEXITCODE -ne 0){throw "Grill edit source compile failed: $source"}
        }
    }
}finally{Pop-Location}
