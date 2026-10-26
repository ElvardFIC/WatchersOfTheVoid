$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/rufus-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    & "$env:USERPROFILE/.local/bin/python3.15.exe" agent/tests/prepare_rufus_fixture.py "$output/rufus_extracted.h"
    if($LASTEXITCODE){throw 'Rufus production fixture extraction failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/rufus_stubs /I src/votv-coop/include "/I$output" "/Fo$output/rufus.obj" "/Fe$output/rufus.exe" agent/tests/rufus_test.cpp /link /OPT:REF
    if($LASTEXITCODE){throw 'Rufus fixture compile failed'}
    & "$output/rufus.exe"
    if($LASTEXITCODE){throw 'Rufus fixture failed'}
    foreach($unit in @('ue_wrap/actors/rufus','coop/creatures/npc_world_enum','coop/creatures/npc_mirror_brain','coop/creatures/npc_mirror','coop/creatures/npc_adoption','coop/creatures/npc_pose_host','coop/creatures/npc_pose_drive')) {
        $leaf=$unit.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$unit.cpp"
        if($LASTEXITCODE){throw "Rufus production compile failed: $unit"}
    }
}finally{Pop-Location}
