param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/container-birth-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/container_birth_stubs" "/I$root/src/votv-coop/include" `
    "/Fo$output/birth.obj" "/Fe$output/birth.exe" "$PSScriptRoot/container_birth_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Container birth regression compile failed'}
& "$output/birth.exe"
if($LASTEXITCODE){throw 'Container birth regression failed'}
if($FaultControls) {
    $source=Get-Content (Join-Path $root 'src/votv-coop/src/coop/props/prop_container_extract.cpp') -Raw
    foreach($name in @('scope-leaks','layout-never-refreshes','lost-spawn','repeated-spawn')) {
        $mutant=switch($name) {
            'scope-leaks' {$source.Replace('SG::IsBodyActive(g_takeFunction.Get())','true')}
            'layout-never-refreshes' {$source.Replace('if (!g_result.function.Is(call.function)) {','if (!g_result.function.Raw()) {')}
            'lost-spawn' {$source.Replace('if (!current() || !s->SendPropSpawn(p) || !current()) continue;','if (!current()) continue; s->SendPropSpawn(p); if (!current()) continue;')}
            'repeated-spawn' {$source.Replace('it->second.spawnQueued = true;','it->second.spawnQueued = false;')}
        }
        if($mutant -eq $source){throw "Missing control site: $name"}
        $control=Join-Path $output $name
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $inc=(Join-Path $control 'birth.inc').Replace('\','/')
        [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
        $fixture=Join-Path $control 'control.cpp'
        $test=(Join-Path $PSScriptRoot 'container_birth_test.cpp').Replace('\','/')
        [IO.File]::WriteAllText($fixture,(('#define CONTAINER_BIRTH_SOURCE "'+$inc+'"')+"`n"+('#include "'+$test+'"')),[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/container_birth_stubs" "/I$root/src/votv-coop/include" `
            "/I$root/src/votv-coop/src/coop/props" "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Control compilation failed: $name"}
        & "$control/test.exe"
        if($LASTEXITCODE -ne 1){throw "Control did not fail as expected: $name"}
        Write-Output "PASS expected-failure control: $name"
    }
}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/container_birth_native_test.py"
if($LASTEXITCODE){throw 'Container birth native contract failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/floor_stubs" "/I$root/src/votv-coop/include" `
    "/Fo$output/host.obj" "/Fe$output/host.exe" "$PSScriptRoot/floor_host_2132_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Shared finish feeder regression compile failed'}
& "$output/host.exe"
if($LASTEXITCODE){throw 'Shared finish feeder regression failed'}
foreach($source in @('prop_container_extract','prop_lifecycle','host_spawn_watcher')) {
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" `
        "/Fo$output/$source.obj" "$root/src/votv-coop/src/coop/props/$source.cpp"
    if($LASTEXITCODE){throw "Production compilation failed: $source"}
}
