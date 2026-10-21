$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output = Join-Path $checkout 'build/ragdoll-skin-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$ragdoll = Get-Content -LiteralPath "$checkout/src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp" -Raw
$start = [regex]::Match($ragdoll, '(?ms)^bool RefreshLocalRagdollModel\(.*?^\}').Value
if (-not $start) { throw 'Production local ragdoll start seam missing' }
[IO.File]::WriteAllText((Join-Path $output 'local_ragdoll_start_production.inc'), $start, [Text.UTF8Encoding]::new($false))
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" "/I$output" `
    "/Fo$output/local-start.obj" "/Fe$output/local-start.exe" "$PSScriptRoot/local_ragdoll_start_test.cpp" /link /OPT:REF
if ($LASTEXITCODE -ne 0) { throw 'Local ragdoll start regression compile failed' }
& "$output/local-start.exe"
if ($LASTEXITCODE -ne 0) { throw 'Local ragdoll start regression failed' }
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/text_drone_stubs" `
    "/I$PSScriptRoot/stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/ragdoll_skin_2144_test.cpp" /link /OPT:REF
if ($LASTEXITCODE -ne 0) { throw 'Ragdoll skin regression compile failed' }
& "$output/test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Ragdoll skin regression failed' }
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
    "/Fo$output/engine_ragdoll_skin.obj" "$checkout/src/votv-coop/src/ue_wrap/engine/engine_ragdoll_skin.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Production ragdoll skin compilation failed' }
