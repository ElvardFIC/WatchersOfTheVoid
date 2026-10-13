$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root '_research/tests_2177'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) { . (Join-Path $root '../_toolchain/env.ps1') }
    & cl /nologo /std:c++20 /EHsc /utf-8 /Gy /I agent/tests/state_2145_stubs `
        /I agent/tests/laptop_2171_stubs /I src/votv-coop/include `
        "/Fo$output/lamp_test.obj" "/Fe$output/lamp_test.exe" agent/tests/lamp_2177_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Lamp test compile failed' }
    & "$output/lamp_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Lamp test failed' }
    foreach ($source in @('coop/interactables/lightgroup_verbs','coop/interactables/interactable_sync',
                          'coop/dispatch/event_dispatch_state','ue_wrap/devices/lightswitch')) {
        $name = Split-Path -Leaf $source
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include `
            "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
        if ($LASTEXITCODE -ne 0) { throw "$source production compile failed" }
    }
} finally { Pop-Location }
