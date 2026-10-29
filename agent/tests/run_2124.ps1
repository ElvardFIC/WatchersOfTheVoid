param([string]$Python = (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $root '_research\tests_2124'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    foreach ($name in @('wire_2124', 'service_panel_2124', 'kitchen_2124', 'gravity_2124')) {
        & cl /nologo /std:c++20 /EHsc /Gy /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\$name.obj" "/Fe$output\$name.exe" "agent\tests\${name}_test.cpp" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Test compilation failed: $name" }
        & "$output\$name.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $name" }
    }
    & $Python agent\tests\blueprint_2124_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Cooked Blueprint validation failed' }
    # Break a real production guard in temporary source copies. Each harness
    # must fail, proving the assertions detect the targeted regression.
    $sourceRoot = ($root -replace '\\', '/') + '/src/votv-coop/src/'
    $cases = @(
        @{ Name='panel_duplicate'; Suite='service_panel'; File='coop/world/service_panel_sync.cpp';
           Before='if (!After(p.seq,g_ack[slot]))'; After='if (false)' },
        @{ Name='panel_prediction'; Suite='service_panel'; File='coop/world/service_panel_sync.cpp';
           Before='Overlay(state,p.packet);'; After='/* overlay removed */;' },
        @{ Name='panel_rollback'; Suite='service_panel'; File='coop/world/service_panel_sync.cpp';
           Before='if (!ok) P::Apply(a,original);'; After='/* rollback removed */' },
        @{ Name='kitchen_echo'; Suite='kitchen'; File='coop/interactables/kitchen_sync.cpp';
           Before='if (c.tag==0x4B540007 && K::DoorUser(parent)!=players::Registry::Get().Local()) return sg::Verdict::Cancel;'; After='/* close echo guard removed */' },
        @{ Name='gravity_missed_shot'; Suite='gravity'; File='coop/items/gravity_motion.cpp';
           Before='(g_unstreamed && active_drive::NowMs()<g_until+2000)'; After='false' }
    )
    foreach ($case in $cases) {
        $sourcePath = $sourceRoot + $case.File
        $source = [IO.File]::ReadAllText($sourcePath)
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant = Join-Path $output ($case.Name + '_source.cpp')
        [IO.File]::WriteAllText($mutant, $source.Replace($case.Before, $case.After))
        $harness = [IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite + '_2124_test.cpp')))
        $harness = $harness.Replace('../../src/votv-coop/src/' + $case.File, ($mutant -replace '\\', '/'))
        $testCopy = Join-Path $output ($case.Name + '_test.cpp')
        [IO.File]::WriteAllText($testCopy, $harness)
        & cl /nologo /std:c++20 /EHsc /Gy "/I$PSScriptRoot" /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\$($case.Name).obj" "/Fe$output\$($case.Name).exe" $testCopy /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Mutation compilation failed: $($case.Name)" }
        & "$output\$($case.Name).exe"
        if ($LASTEXITCODE -eq 0) { throw "Mutation was not detected: $($case.Name)" }
        Write-Host "PASS: negative control rejected: $($case.Name)"
    }
    $global:LASTEXITCODE = 0
} finally { Pop-Location }
