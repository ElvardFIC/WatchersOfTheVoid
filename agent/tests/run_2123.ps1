param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $root '_research\tests_2123'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . .\_toolchain\env.ps1
    function Check-Cpp([string]$Source, [string]$Name, [bool]$Stubs = $false, [bool]$Negative = $false) {
        $argsCl = @('/nologo', '/std:c++20', '/EHsc')
        if ($Stubs) { $argsCl += @('/I', 'agent\tests\stubs') }
        $argsCl += @('/I', 'src\votv-coop\include', '/I', 'src\votv-coop\src',
            "/Fo$output\$Name.obj", "/Fe$output\$Name.exe", $Source)
        & cl @argsCl
        if ($LASTEXITCODE -ne 0) { throw "Compile failed: $Name" }
        & "$output\$Name.exe"
        if ($Negative) {
            if ($LASTEXITCODE -eq 0) { throw "Negative control was not detected: $Name" }
            Write-Host "PASS: negative control rejected: $Name"
        } elseif ($LASTEXITCODE -ne 0) { throw "Test failed: $Name" }
    }
    Check-Cpp 'agent\tests\sync_2122_test.cpp' 'sync_2122'
    Check-Cpp 'agent\tests\sync_2123_test.cpp' 'sync_2123'
    Check-Cpp 'agent\tests\prop_drive_2123_test.cpp' 'prop_drive_2123' $true
    & $Python agent\tests\cord_blueprint_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Cooked Blueprint validation failed' }

    # Real production-source mutations in temporary copies: prove the receiver/host
    # harness detects the regressions instead of merely mirroring expected output.
    $sourceRoot = ($root -replace '\\', '/') + '/src/votv-coop/src/'
    $receiverPath = $sourceRoot + 'coop/props/prop_drive_stream.cpp'
    $hostPath = $sourceRoot + 'coop/props/prop_drive_host.cpp'
    $receiver = [IO.File]::ReadAllText($receiverPath)
    $hostSource = [IO.File]::ReadAllText($hostPath)
    $harness = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'prop_drive_2123_test.cpp'))
    $harness = $harness.Replace('../../src/votv-coop/src/', $sourceRoot)
    $cases = @(
        @{ Name='old_end'; File=$receiverPath; Text=$receiver;
           Before='if (ended != g_endedGen.end() && !GenAfter(p.gen, ended->second)) return;'; After='/* missing end fence */' },
        @{ Name='local_hold'; File=$receiverPath; Text=$receiver;
           Before='return local_prop_hold::IsHeld(actor) || remote_prop::IsActorUnderAnyDrive(actor);'; After='return remote_prop::IsActorUnderAnyDrive(actor);' },
        @{ Name='pending_end'; File=$receiverPath; Text=$receiver;
           Before='g_pendingEnd.erase(p.eid);'; After='/* stale pending end retained */' },
        @{ Name='slot_serial'; File=$receiverPath; Text=$receiver;
           Before='void* LiveActor() const { return actorRef.Get(); }'; After='void* LiveActor() const { return d.LiveActor(); }' },
        @{ Name='slow_heartbeat'; File=$hostPath; Text=$hostSource;
           Before='if (moved) continue;'; After='continue;' }
    )
    foreach ($case in $cases) {
        if (-not $case.Text.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant = Join-Path $output ($case.Name + '_source.cpp')
        [IO.File]::WriteAllText($mutant, $case.Text.Replace($case.Before, $case.After))
        $testCopy = Join-Path $output ($case.Name + '_test.cpp')
        [IO.File]::WriteAllText($testCopy, $harness.Replace($case.File, ($mutant -replace '\\', '/')))
        Check-Cpp $testCopy $case.Name $true $true
    }
    Write-Host 'PASS: b2122/b2123 regressions, cooked Blueprint and all five negative controls'
    $global:LASTEXITCODE = 0
} finally { Pop-Location }
