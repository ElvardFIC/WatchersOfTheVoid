param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2132'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    function Compile-AndRun([string]$name,[string]$source,[string]$stub,[bool]$negative) {
        & cl /nologo /std:c++20 /EHsc /Gy "/I$PSScriptRoot" /I "agent/tests/$stub" /I src\votv-coop\include "/I$root/src/votv-coop/src/coop/props" "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $name" }
        & "$output/$name.exe"
        $rc=$LASTEXITCODE
        if ($negative) {
            if ($rc -ne 1) { throw "Negative control must fail assertion (1), got $rc for $name" }
            Write-Host "PASS: negative control rejected: $name"
        } elseif ($rc -ne 0) { throw "Test failed: $name" }
    }
    Compile-AndRun 'key' 'agent/tests/floor_key_2132_test.cpp' 'floor_stubs' $false
    Compile-AndRun 'client' 'agent/tests/floor_client_2132_test.cpp' 'item_record_stubs' $false
    Compile-AndRun 'host' 'agent/tests/floor_host_2132_test.cpp' 'floor_stubs' $false
    & $Python agent/tests/blueprint_floor_2132_probe.py
    if ($LASTEXITCODE -ne 0) { throw 'Native evidence failed' }
    $cases=@(
        @{Name='key_no_lineage';Suite='key';File='ue_wrap/actors/floor_bed.cpp';Before='!IsFloorBed(actor) ||';After=''},
        @{Name='key_overwrite';Suite='key';File='ue_wrap/actors/floor_bed.cpp';Before='if (HasKey(actor)) return true;';After=''},
        @{Name='key_stale';Suite='key';File='ue_wrap/actors/floor_bed.cpp';Before='|| identity.Get() != actor';After=''},
        @{Name='key_no_readback';Suite='key';File='ue_wrap/actors/floor_bed.cpp';Before='return HasKey(actor);';After='return true;'},
        @{Name='key_reentry';Suite='key';File='ue_wrap/actors/floor_bed.cpp';Before='g_initializing.count(actor)';After='false'},
        @{Name='client_missing_mint';Suite='client';File='coop/props/prop_drop_intent.cpp';Before='ue_wrap::floor_bed::EnsureKey(actor)';After='false'},
        @{Name='client_load_mint';Suite='client';File='coop/props/prop_drop_intent.cpp';Before='e.playerSpawn && (key.empty()';After='(key.empty()'},
        @{Name='client_late_mirror';Suite='client';File='coop/props/prop_drop_intent.cpp';Before='coop::prop_echo_suppress::IsMirrorSpawn(actor) || coop::hand_item::IsHandAxisActor(actor)';After='false'},
        @{Name='host_missing_mint';Suite='host';File='coop/props/host_spawn_watcher.cpp';Before='ue_wrap::floor_bed::EnsureKey(actor)';After='false'},
        @{Name='host_load_mint';Suite='host';File='coop/props/host_spawn_watcher.cpp';Before='coop::prop_spawn_authoring::BirthIsPlayerAuthored();';After='true;'},
        @{Name='host_stale';Suite='host';File='coop/props/host_spawn_watcher.cpp';Before='e.actor.Get()';After='e.actor.Raw()'},
        @{Name='host_reset';Suite='host';File='coop/props/host_spawn_watcher.cpp';Before='++g_pendingEpoch;';After=''},
        @{Name='host_reentry';Suite='host';File='coop/props/host_spawn_watcher.cpp';Before='g_pendingFinished.empty() || g_draining';After='g_pendingFinished.empty()'},
        @{Name='host_callback_loss';Suite='host';File='coop/props/host_spawn_watcher.cpp';Before='if (epoch != g_pendingEpoch) return;\n    for (auto& e : keep)';After='g_pendingFinished.clear();\n    if (epoch != g_pendingEpoch) return;\n    for (auto& e : keep)'},
        @{Name='host_retry_cap';Suite='host';File='coop/props/host_spawn_watcher.cpp';Before='g_pendingFinished.size() < kMaxPendingFinished';After='true'}
    )
    foreach ($case in $cases) {
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File))).Replace("`r`n","`n")
        $before=$case.Before.Replace('\n',"`n");$after=$case.After.Replace('\n',"`n")
        if (-not $source.Contains($before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($before,$after))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ('floor_'+$case.Suite+'_2132_test.cpp')))
        if ($case.Suite -eq 'client') {
            $baseline=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'pending_birth_2130_test.cpp'))
            $harness=$harness.Replace('#include "pending_birth_2130_test.cpp"',$baseline)
        }
        $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
        $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
        $copy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($copy,$harness)
        $stub=if ($case.Suite -eq 'client') {'item_record_stubs'} else {'floor_stubs'}
        Compile-AndRun $case.Name $copy $stub $true
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
