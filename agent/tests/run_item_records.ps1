param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/item-record-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/item_record_stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/item_record_reentry_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Item record controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Item record controller failed'}
    if($CompileSources){
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/prop_save_data.obj" src/votv-coop/src/coop/props/prop_save_data.cpp
        if($LASTEXITCODE -ne 0){throw 'Item record source compile failed'}
    }
} finally {Pop-Location}
