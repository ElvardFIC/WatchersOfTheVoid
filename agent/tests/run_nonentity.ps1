param([switch]$CompileSources)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root 'build/nonentity-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) { . (Join-Path $root '../_toolchain/env.ps1') }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/handoff_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/coin_collect.obj" "/Fe$output/coin_collect.exe" agent/tests/coin_collect_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Coin test compile failed' }
    & "$output/coin_collect.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Coin test failed' }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/item_record_stubs /I src/votv-coop/include "/Fo$output/pending_birth.obj" "/Fe$output/pending_birth.exe" agent/tests/placement_reply_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Placement test compile failed' }
    & "$output/pending_birth.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Placement test failed' }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wall_drying.obj" "/Fe$output/wall_drying.exe" agent/tests/wall_drying_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Wall drying test compile failed' }
    & "$output/wall_drying.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Wall drying test failed' }
    foreach ($testName in @('wall_drying_wire','wall_record_2166')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/placed_save_stubs /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Wall test compile failed: $testName" }
        & "$output/$testName.exe"
        if ($LASTEXITCODE -ne 0) { throw "Wall test failed: $testName" }
    }
    foreach ($testName in @('building_wrapper_2153','material_profile_2169')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Material test compile failed: $testName" }
        & "$output/$testName.exe"
        if ($LASTEXITCODE -ne 0) { throw "Material test failed: $testName" }
    }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/material_2153_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/bucket.obj" "/Fe$output/bucket.exe" agent/tests/bucket_2153_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Bucket test compile failed' }
    & "$output/bucket.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Bucket test failed' }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/sky.obj" "/Fe$output/sky.exe" agent/tests/sky_2164_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Sky test compile failed' }
    & "$output/sky.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Sky test failed' }
    if ($CompileSources) {
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/coingun_collect.obj" src/votv-coop/src/coop/items/coingun_collect.cpp
        if ($LASTEXITCODE -ne 0) { throw 'Coin source compile failed' }
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/prop_drop_intent.obj" src/votv-coop/src/coop/props/prop_drop_intent.cpp
        if ($LASTEXITCODE -ne 0) { throw 'Placement source compile failed' }
        foreach ($file in @('ue_wrap/actors/wall_drying','coop/world/material_save_sync','ue_wrap/actors/material_saved_state','ue_wrap/actors/save_record','ue_wrap/actors/building_material','coop/props/bucket_sync','coop/world/sky_sync')) {
            $objectName = $file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$objectName.obj" "src/votv-coop/src/$file.cpp"
            if ($LASTEXITCODE -ne 0) { throw "Source compile failed: $file" }
        }
    }
} finally { Pop-Location }
