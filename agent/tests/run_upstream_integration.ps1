$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/upstream-integration-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source=Get-Content (Join-Path $checkout 'src/votv-coop/src/coop/world/explosion_sync.cpp') -Raw
$parts=foreach($name in @('AllFinite','InRange','OnSpawnReceived')) {
    $pattern=if($name -eq 'InRange'){'(?m)^bool InRange[^\r\n]+'}else{"(?ms)^bool $name\(.*?^\}"}
    $match=[regex]::Match($source,$pattern)
    if(-not $match.Success){throw "Production blast validator missing: $name"}
    $match.Value
}
$bounds=[regex]::Matches($source,'(?m)^constexpr float kMax\w+\s*=.*?;').Value -join "`n"
[IO.File]::WriteAllText((Join-Path $output 'explosion_validation.inc'),($bounds+"`n"+($parts -join "`n")),[Text.UTF8Encoding]::new($false))
foreach($test in @('native_explosion_wire','drone_intent_2135','atv_condition_merge')) {
    $includes=@("/I$output")
    if($test -eq 'drone_intent_2135'){$includes+=@("/I$PSScriptRoot/text_drone_stubs","/I$PSScriptRoot/stubs")}
    $includes+="/I$checkout/src/votv-coop/include"
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/$test.obj" "/Fe$output/$test.exe" "$PSScriptRoot/${test}_test.cpp" /link /OPT:REF
    if($LASTEXITCODE){throw "Integration compile failed: $test"}
    & "$output/$test.exe"
    if($LASTEXITCODE){throw "Integration regression failed: $test"}
}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/upstream_integration_test.py"
if($LASTEXITCODE){throw 'Published protocol identity regression failed'}
