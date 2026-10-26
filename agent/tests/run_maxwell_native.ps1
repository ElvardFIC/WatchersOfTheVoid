$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/maxwell-native-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/native.obj" "/Fe$output/native.exe" agent/tests/maxwell_native_test.cpp /link /OPT:REF
    if($LASTEXITCODE){throw 'Maxwell native fixture compile failed'}
    & "$output/native.exe"
    if($LASTEXITCODE){throw 'Maxwell native fixture failed'}
    & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/dingus_music.obj" src/votv-coop/src/ue_wrap/actors/dingus_music.cpp
    if($LASTEXITCODE){throw 'Maxwell production compile failed'}
}finally{Pop-Location}
