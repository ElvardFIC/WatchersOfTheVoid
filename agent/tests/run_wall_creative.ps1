param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/wall-creative-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    foreach($testName in @('wall_creative_sync','wall_creative_wrapper')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Creative wall test compile failed: $testName"}
        & "$output/$testName.exe"
        if($LASTEXITCODE -ne 0){throw "Creative wall test failed: $testName"}
    }
    if($CompileSources) {
        foreach($file in @('ue_wrap/actors/wall_creative','coop/world/wall_creative_sync','coop/world/wall_build_sync','coop/dispatch/event_dispatch_world')) {
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Creative wall source compile failed: $file"}
        }
    }
} finally {Pop-Location}
