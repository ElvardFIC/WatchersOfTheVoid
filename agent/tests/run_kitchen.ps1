param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/kitchen-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/kitchen_2124_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Kitchen controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Kitchen controller failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/grill_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grill wrapper compile failed'}
    & "$output/wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Grill wrapper failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/appliances.obj" "/Fe$output/appliances.exe" agent/tests/kitchen_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Appliance wrapper compile failed'}
    & "$output/appliances.exe"
    if($LASTEXITCODE -ne 0){throw 'Appliance wrapper failed'}
    if($CompileSources) {
        foreach($file in @('ue_wrap/devices/grill','ue_wrap/devices/kitchen_machine','coop/interactables/kitchen_sync','coop/dispatch/event_dispatch_world')) {
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Kitchen source compile failed: $file"}
        }
    }
} finally {Pop-Location}
