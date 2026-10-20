$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/placement-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($name in @('pending_birth_2130','local_birth_2182','merged_protocol')) {
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/item_record_stubs" "/I$checkout/src/votv-coop/include" `
        "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
    & "$output/$name.exe"
    if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
}
