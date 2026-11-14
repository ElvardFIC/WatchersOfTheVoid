param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2131'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
        & cl /nologo /std:c++20 /EHsc /Gy "/I$PSScriptRoot" /I agent\tests\player_2131_stubs /I agent\tests\stubs /I src\votv-coop\include "/Fo$output\$name.obj" "/Fe$output\$name.exe" $source /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $name" }
        & "$output\$name.exe"
        $rc=$LASTEXITCODE
        if ($negative) {
            if ($rc -ne 1) { throw "Negative control must fail assertion (1), got $rc for $name" }
            Write-Host "PASS: negative control rejected: $name"
        } elseif ($rc -ne 0) { throw "Test failed: $name" }
    }
    foreach ($suite in @('player_state','player_fire')) {
        Compile-AndRun $suite "agent/tests/${suite}_2131_test.cpp" $false
    }
    & $Python agent/tests/blueprint_2131_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Native/integration evidence check failed' }
    $cases=@(
        @{Name='guard_off';Suite='player_state';File='coop/player/player_damage.cpp';Before='return Verdict::Cancel;';After='return Verdict::Run;'},
        @{Name='guard_local';Suite='player_state';File='coop/player/player_damage.cpp';Before='!local || c.object == local';After='!local'},
        @{Name='guard_disconnect';Suite='player_state';File='coop/player/player_damage.cpp';Before='!s || !s->connected() || !c.object';After='!s || !c.object'},
        @{Name='guard_old_pawn';Suite='player_state';File='coop/player/player_damage.cpp';Before='void* local = coop::players::Registry::Get().Local();';After='void* local = g_localPawn.load();'},
        @{Name='fire_fuel';Suite='player_fire';File='ue_wrap/engine/engine_mainplayer.cpp';Before='fuel == 0.f &&';After='true &&'},
        @{Name='fire_flag';Suite='player_fire';File='ue_wrap/engine/engine_mainplayer.cpp';Before='(bytes[byteOffset] & mask) == 0';After='true'},
        @{Name='fire_call';Suite='player_fire';File='ue_wrap/engine/engine_mainplayer.cpp';Before='!frame.valid() || !Call(mainPlayer, frame) ||';After='!frame.valid() ||'},
        @{Name='ragdoll_bit';Suite='player_fire';File='ue_wrap/engine/engine_mainplayer.cpp';Before='if (allowed) *p |= mask; else *p &= static_cast<uint8_t>(~mask);';After='*p = allowed ? 1 : 0;'}
    )
    foreach ($case in $cases) {
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2131_test.cpp')))
        $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
        $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
        $copy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($copy,$harness)
        Compile-AndRun $case.Name $copy $true
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
