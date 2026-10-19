$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/run_2179.ps1"
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) { . (Join-Path $root '../_toolchain/env.ps1') }
    & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include `
        /Fo_research/tests_2179/subsystems.obj src/votv-coop/src/coop/session/subsystems.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Session lifecycle production compile failed' }
} finally { Pop-Location }
