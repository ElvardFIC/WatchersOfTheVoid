$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root 'build/profile-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) { . (Join-Path $root '../_toolchain/env.ps1') }
    foreach ($test in @('profile_receive_2169_test','canned_profile_2176_test','canned_record_2176_test')) {
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy /I agent/tests/profile_2169_stubs `
            /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" `
            "agent/tests/$test.cpp" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw "$test compile failed" }
        & "$output/$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test failed" }
    }
    foreach ($source in @('coop/items/player_inventory_sync','ue_wrap/actors/food','coop/net/blob_chunks')) {
        $name = Split-Path -Leaf $source
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include `
            "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
        if ($LASTEXITCODE -ne 0) { throw "$source production compile failed" }
    }
} finally { Pop-Location }
