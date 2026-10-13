$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2130'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . .\_toolchain\env.ps1
    function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
        & cl /nologo /std:c++20 /EHsc /Gy /I agent\tests\item_record_stubs /I src\votv-coop\include "/Fo$output\$name.obj" "/Fe$output\$name.exe" $source /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $name" }
        & "$output\$name.exe"
        $rc=$LASTEXITCODE
        if ($negative) {
            if ($rc -ne 1) { throw "Negative control must fail an assertion (1), got $rc for $name" }
            Write-Host "PASS: negative control rejected: $name"
        } elseif ($rc -ne 0) { throw "Test failed: $name" }
    }
    Compile-AndRun 'item_record' 'agent\tests\item_record_reentry_test.cpp' $false
    Compile-AndRun 'pending_birth' 'agent\tests\pending_birth_2130_test.cpp' $false
    $cases=@(
        @{Name='reentry';Before='key.empty() || g_applying.count(key)';After='key.empty()'},
        @{Name='borrowed_record';Before='const Parked snapshot = it->second;';After='const Parked& snapshot = it->second;'},
        @{Name='erase_replacement';Before='current->second.seq == snapshot.seq';After='true'},
        @{Name='world_aba';Before='++g_worldEpoch;';After='/* unchanged world epoch */'},
        @{Name='drive_reentry';Before='if (g_driving) return;';After='/* allow nested Drive */'},
        @{Name='uncharged_failure';Before='if (budgeted) --g_applyBudget;';After='/* no dispatch budget */'},
        @{Name='stale_target';Before='++g_peerEpoch[senderSlot];';After='/* unchanged slot occupant */'},
        @{Name='retry_replacement';Before='current->second.seq == seq';After='true'},
        @{Name='capture_reentry';Before='g_applying.count(key) || g_publishing.count(key)';After='false'}
    )
    foreach ($case in $cases) {
        $relative='coop/props/prop_save_data.cpp'
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$relative)))
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'item_record_reentry_test.cpp'))
        $harness=$harness.Replace('../../src/votv-coop/src/'+$relative,($mutant -replace '\\','/'))
        $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
        $copy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($copy,$harness)
        Compile-AndRun $case.Name $copy $true
    }
    $birthCases=@(
        @{Name='birth_identity';Before='e.actor.Get()';After='e.actor.Raw()'},
        @{Name='birth_reset';Before='++g_pendingEpoch;';After='/* unchanged world */'},
        @{Name='birth_reentry';Before='g_pending.empty() || g_draining';After='g_pending.empty()'},
        @{Name='birth_callback_loss';Before='for (auto& e : keep) {';After='g_pending.clear(); for (auto& e : keep) {'},
        @{Name='birth_stale_dispatch';Before='if (!stillCurrent()) continue;';After='/* skip identity revalidation */'},
        @{Name='birth_merge_cap';Before='if (g_pending.size() < kMaxPending)';After='if (true)'}
    )
    foreach ($case in $birthCases) {
        $relative='coop/props/prop_drop_intent.cpp'
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$relative)))
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'pending_birth_2130_test.cpp'))
        $harness=$harness.Replace('../../src/votv-coop/src/'+$relative,($mutant -replace '\\','/'))
        $copy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($copy,$harness)
        Compile-AndRun $case.Name $copy $true
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
