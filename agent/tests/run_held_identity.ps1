$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/held-identity-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source=Get-Content -LiteralPath (Join-Path $checkout 'src/votv-coop/src/coop/props/trash_collect_sync.cpp') -Raw
$body=[regex]::Match($source,'(?ms)^bool EnsureHeldItemBroadcast\(.*?(?=^void Install\()').Value
if(-not $body){throw 'Production held-item seam missing'}
$includes=([regex]::Matches($source,'(?m)^#include [^\r\n]+') | ForEach-Object Value) -join "`n"
[IO.File]::WriteAllText((Join-Path $output 'held_identity.inc'),$includes+"`nnamespace coop::trash_collect_sync { namespace R=ue_wrap::reflection; namespace PT=coop::prop_element_tracker;`n"+$body+"`n}")
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/held_identity_stubs" "/I$checkout/src/votv-coop/include" "/I$output" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/held_identity_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Held identity compile failed'}
& "$output/test.exe"
if($LASTEXITCODE -ne 0){throw 'Held identity regression failed'}
