$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/session-reports-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$screen=Get-Content (Join-Path $checkout 'src/votv-coop/src/ue_wrap/desk/device_screen.cpp') -Raw
$functions=foreach($name in @('PosKey','ClassifyWidgetClaimKey','ClassifyDeviceActorClaimKey')) {
    $match=[regex]::Match($screen,"(?ms)^std::wstring $name\(.*?^\}")
    if(-not $match.Success){throw "Production classifier missing: $name"}
    $match.Value
}
[IO.File]::WriteAllText((Join-Path $output 'console_identity.inc'),($functions -join "`n"),[Text.UTF8Encoding]::new($false))
$rest=Get-Content (Join-Path $checkout 'src/votv-coop/src/coop/player/sleep_rest.cpp') -Raw
$functions=foreach($name in @('PersonalRest','OnSleepPost','Tick')) {
    $match=[regex]::Match($rest,"(?ms)^(bool|void) $name\(.*?^\}")
    if(-not $match.Success){throw "Production rest controller missing: $name"}
    $match.Value
}
[IO.File]::WriteAllText((Join-Path $output 'sleep_rest_controller.inc'),($functions -join "`n"),[Text.UTF8Encoding]::new($false))
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/Fo$output/reports.obj" "/Fe$output/reports.exe" "$PSScriptRoot/session_reports_test.cpp"
if($LASTEXITCODE){throw 'Session report regression compile failed'}
& "$output/reports.exe"
if($LASTEXITCODE){throw 'Session report regression failed'}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/session_reports_native_test.py"
if($LASTEXITCODE){throw 'Session report native contract failed'}
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run_atv_companion.ps1')
if($LASTEXITCODE){throw 'Current companion report regression failed'}
