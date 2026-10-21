$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/trowel-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    $codec=Get-Content -LiteralPath 'src/votv-coop/src/ue_wrap/actors/save_record.cpp' -Raw
    $offset=[regex]::Match($codec,'(?m)^constexpr int32_t kSave_ints\s*=\s*0x[0-9a-fA-F]+;').Value
    $read=[regex]::Match($codec,'(?ms)^Arr ReadArr\(.*?^\}').Value
    $write=[regex]::Match($codec,'(?ms)^bool WriteSingleIntGroup\(.*?^\}').Value
    if(-not $offset -or -not $read -or -not $write){throw 'Production integer codec seam missing'}
    $seam='#include "ue_wrap/actors/save_record.h"'+"`n"+'#include "ue_wrap/core/field_io.h"'+"`n#include <cstring>`nnamespace ue_wrap::save_record {`n"+$offset+"`n"+$read+"`n"+$write+"`n}`n"
    [IO.File]::WriteAllText((Join-Path $output 'saved_int_production.inc'),$seam,[Text.UTF8Encoding]::new($false))
    foreach($name in @('trowel_persistence','save_int_group')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/I$output" "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
        & "$output/$name.exe"
        if($LASTEXITCODE -ne 0){throw "Regression failed: $name"}
    }
} finally {Pop-Location}
