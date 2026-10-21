param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/hand-tools-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    foreach($testName in @('extinguisher_2145','audio_playing','washer_wrapper_2149')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Hand tool test compile failed: $testName"}
        & "$output/$testName.exe"
        if($LASTEXITCODE -ne 0){throw "Hand tool test failed: $testName"}
    }
    if($CompileSources) {
        foreach($file in @('ue_wrap/actors/fire_extinguisher','ue_wrap/core/component_calls','ue_wrap/actors/pressure_washer','coop/player/hand_extinguisher_sync','coop/dispatch/event_dispatch_world')) {
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Hand tool source compile failed: $file"}
        }
    }
} finally {Pop-Location}
