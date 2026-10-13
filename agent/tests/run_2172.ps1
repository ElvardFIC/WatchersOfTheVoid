$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $root '_research\tests_2172'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '..\_toolchain\env.ps1')

    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 `
        /I src/votv-coop/include `
        "/Fo$output\floppy_slot_bitfield_2172_test.obj" `
        "/Fe$output\floppy_slot_bitfield_2172_test.exe" `
        agent/tests/floppy_slot_bitfield_2172_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'Floppy slot bitfield test compile failed' }
    & "$output\floppy_slot_bitfield_2172_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Floppy slot bitfield test failed' }

    & cl /nologo /std:c++20 /EHsc /utf-8 /c `
        /I src/votv-coop/include `
        "/Fo$output\floppy_slot.obj" `
        src/votv-coop/src/ue_wrap/devices/floppy_slot.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Floppy slot production compile failed' }
} finally {
    Pop-Location
}
