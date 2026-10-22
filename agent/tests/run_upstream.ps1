$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/upstream-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($name in @('upstream_vitals','fossilhound_yaw')) {
    & cl /nologo /std:c++20 /EHsc /utf-8 "/I$checkout/src/votv-coop/include" `
        "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp"
    if($LASTEXITCODE){throw "$name compile failed"}
    & "$output/$name.exe"
    if($LASTEXITCODE){throw "$name regression failed"}
}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/upstream_prop_classes_test.py"
if($LASTEXITCODE){throw 'Prop class regression failed'}
