$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
$output = Join-Path $checkout 'build/ragdoll-retry-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/ragdoll_retry_test.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Ragdoll retry regression compile failed' }
& "$output/test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Ragdoll retry regression failed' }
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
    "/Fo$output/remote_player_ragdoll.obj" "$checkout/src/votv-coop/src/coop/player/remote_player_ragdoll.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Production ragdoll compilation failed' }
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
    "/Fo$output/engine_playerragdoll.obj" "$checkout/src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp"
if ($LASTEXITCODE -ne 0) { throw 'Production ragdoll engine compilation failed' }
