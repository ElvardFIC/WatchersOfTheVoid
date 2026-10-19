$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2147'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/container.obj" "/Fe$output/container.exe" agent/tests/container_extract_2147_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Container test compilation failed'}
 & "$output/container.exe"
 if($LASTEXITCODE -ne 0){throw 'Container test failed'}
 # Compile only the changed translation unit to catch integration errors, without linking a mod.
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/inventory_pickup_sync.obj" src/votv-coop/src/coop/items/inventory_pickup_sync.cpp
 if($LASTEXITCODE -ne 0){throw 'Inventory pickup source compilation failed'}
}finally{Pop-Location}
