$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output = Join-Path $checkout 'build/insomnia-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach ($unit in @('world/spawn_authority', 'creatures/npc_world_enum')) {
    $name = Split-Path $unit -Leaf
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/coop/$unit.cpp"
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $unit" }
}
Write-Output 'PASS: Insomniac authority/enrollment translation units compiled'
