$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/upstream-containers-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$generated=Join-Path $output 'decisions.cpp'
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/upstream_containers_extract.py" $generated
if($LASTEXITCODE){throw 'Container decision extraction failed'}
$production=Join-Path $root 'src/votv-coop/src'
$sources=@($generated,"$PSScriptRoot/upstream_containers_test.cpp")
foreach($name in @('container_act_wire','container_act_state','container_slice_wire','container_act_selftest','container_act_selftest_put','container_act_selftest_overlap')) {
    $sources+=Join-Path $production "coop/props/$name.cpp"
}
$sources+=Join-Path $production 'coop/items/save_record_wire.cpp'
$sources+=Join-Path $production 'coop/interactables/signal_wire.cpp'
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$root/src/votv-coop/include" "/Fo$output/" "/Fe$output/test.exe" @sources /link /OPT:REF
if($LASTEXITCODE){throw 'Upstream container selftest compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Upstream container selftest failed'}
