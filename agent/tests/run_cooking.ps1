param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/cooking-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    foreach($name in @('cooking_food','food_wrapper','food_publish_2142','food_delivery','pending_birth_2130','wrappers_2129')) {
        $stub=if($name -in @('food_publish_2142','food_delivery')){'agent/tests/food_stubs'}elseif($name -eq 'wrappers_2129'){'agent/tests/stubs'}else{'agent/tests/item_record_stubs'}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I $stub /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Cooking test compile failed: $name"}
        & "$output/$name.exe"
        if($LASTEXITCODE -ne 0){throw "Cooking test failed: $name"}
    }
    if($CompileSources) {
        foreach($file in @('ue_wrap/actors/food','ue_wrap/actors/cooking_food','coop/props/food_save_sync','coop/props/prop_food_sync','coop/props/prop_drop_intent','coop/items/player_inventory_sync','harness/harness','harness/pump')) {
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Cooking source compile failed: $file"}
        }
    }
} finally {Pop-Location}
