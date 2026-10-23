param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/ariral-ship-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($name in @('ariral_ship','ariral_ship_wrapper')) {
    & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/ariral_ship_stubs" "/I$root/src/votv-coop/include" `
        "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" /link /OPT:REF
    if($LASTEXITCODE){throw "Compile failed: $name"}
    & "$output/$name.exe"
    if($LASTEXITCODE){throw "Regression failed: $name"}
}
if($FaultControls) {
    $source=Get-Content (Join-Path $root 'src/votv-coop/src/coop/world/ariral_ship_sync.cpp') -Raw
    foreach($name in @('local-delays','stale-phase','lost-send','old-cue')) {
        $mutant=switch($name) {
            'local-delays' {$source.Replace('case 15: case 31: case 383: case 604:', 'case 16: case 32: case 384: case 605:')}
            'stale-phase' {$source.Replace('!Newer(p.revision,row.state.revision)','false')}
            'lost-send' {$source.Replace('if(accepted && it!=g_rows.end()', 'if(true && it!=g_rows.end()')}
            'old-cue' {$source.Replace('if(birth || (p.flags&Quiet))','if(false)')}
        }
        if($mutant -eq $source){throw "Missing control site: $name"}
        $control=Join-Path $output $name
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $inc=(Join-Path $control 'ship.inc').Replace('\','/')
        [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
        $test=(Join-Path $PSScriptRoot 'ariral_ship_test.cpp').Replace('\','/')
        $fixture=Join-Path $control 'control.cpp'
        [IO.File]::WriteAllText($fixture,(('#define ARIRAL_SHIP_SOURCE "'+$inc+'"')+"`n"+('#include "'+$test+'"')),[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/ariral_ship_stubs" "/I$root/src/votv-coop/include" `
            "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Control compile failed: $name"}
        & "$control/test.exe"
        if($LASTEXITCODE -ne 1){throw "Control did not reject removed protection: $name"}
        Write-Output "PASS expected-failure control: $name"
    }
}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/ariral_ship_native_test.py"
if($LASTEXITCODE){throw 'Ariral ship cooked contract failed'}
foreach($source in @('coop/world/ariral_ship_sync','ue_wrap/world/ariral_ship')) {
    $name=$source.Replace('/','_')
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$source.cpp"
    if($LASTEXITCODE){throw "Production compile failed: $source"}
}
