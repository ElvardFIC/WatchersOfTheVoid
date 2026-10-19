$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root '_research/tests_2173'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        . (Join-Path $root '../_toolchain/env.ps1')
    }
    foreach ($test in @('laptop_power_2171_test', 'laptop_delivery_2173_test')) {
        & cl /nologo /std:c++20 /EHsc /utf-8 /I agent/tests/laptop_2171_stubs `
            /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" `
            "agent/tests/$test.cpp" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "$test compile failed" }
        & "$output/$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test failed" }
    }
    foreach ($source in @('coop/interactables/laptop_sync', 'coop/session/subsystems')) {
        $name = Split-Path -Leaf $source
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include `
            "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
        if ($LASTEXITCODE -ne 0) { throw "$source production compile failed" }
    }
} finally { Pop-Location }
