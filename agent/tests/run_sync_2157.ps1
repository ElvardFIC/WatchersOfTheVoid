$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/sync-2157-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/registry.obj" "/Fe$output/registry.exe" "$PSScriptRoot/script_gate_capacity_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Watch capacity fixture did not compile'}
& "$output/registry.exe"
if($LASTEXITCODE){throw 'Watch capacity regression failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/limits.obj" "/Fe$output/limits.exe" "$PSScriptRoot/player_limit_test.cpp"
if($LASTEXITCODE){throw 'Seat limit fixture did not compile'}
& "$output/limits.exe"
if($LASTEXITCODE){throw 'Seat limit regression failed'}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/container_seed_retry_test.py"
if($LASTEXITCODE){throw 'Container seed regression failed'}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/peer_masks_32_test.py"
if($LASTEXITCODE){throw 'Final-seat recipients and held-food regression failed'}

# Compile the same registry fixture with the previous table budget. It must fail
# on a refused registration, independently of the corrected positive result.
$legacy=Join-Path $output 'legacy-capacity'
New-Item -ItemType Directory -Force -Path $legacy | Out-Null
$core=Join-Path $checkout 'src/votv-coop/src/ue_wrap/core'
$detail=[IO.File]::ReadAllText((Join-Path $core 'script_gate_detail.h'))
if(-not $detail.Contains('kSlotBits = 13')){throw 'Legacy capacity control seam changed'}
[IO.File]::WriteAllText((Join-Path $legacy 'script_gate_detail.h'),$detail.Replace('kSlotBits = 13','kSlotBits = 11'))
Copy-Item -LiteralPath (Join-Path $core 'script_gate_registry.cpp') -Destination $legacy
$fixture=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'script_gate_capacity_test.cpp'))
$fixture=$fixture.Replace('../../src/votv-coop/src/ue_wrap/core/script_gate_registry.cpp',
    (Join-Path $legacy 'script_gate_registry.cpp').Replace('\','/'))
[IO.File]::WriteAllText((Join-Path $legacy 'test.cpp'),$fixture)
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$legacy/test.obj" "/Fe$legacy/test.exe" "$legacy/test.cpp"
if($LASTEXITCODE){throw 'Legacy capacity control did not compile'}
$process=Start-Process -FilePath "$legacy/test.exe" -Wait -PassThru -WindowStyle Hidden `
    -RedirectStandardError "$legacy/stderr.log" -RedirectStandardOutput "$legacy/stdout.log"
if($process.ExitCode -ne 1 -or -not ([IO.File]::ReadAllText("$legacy/stderr.log").Contains('Register(g_nameTable'))){
    throw 'Legacy capacity control did not reject the exhausted registry'
}
Write-Output 'PASS negative control: old watch capacity refuses the combined synchronization controllers'
$global:LASTEXITCODE=0
