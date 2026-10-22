param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(-not(Get-Command cl -ErrorAction SilentlyContinue)){throw 'Run from an x64 MSVC developer shell.'}
$output=Join-Path $checkout 'build/ragdoll-stream-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/ragdoll_stream_test.cpp"
if($LASTEXITCODE){throw 'Ragdoll stream fixture compilation failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Ragdoll stream regression failed'}
if($FaultControls){
    foreach($mode in @('legacy-time','legacy-inbox')){
        $lines=& "$output/test.exe" $mode 2>&1
        if($LASTEXITCODE -ne 1 -or ($lines -join "`n") -notmatch '^REPRODUCED:'){throw "Ragdoll stream control failed: $mode"}
        $lines | Set-Content -LiteralPath (Join-Path $output "$mode.log")
        Write-Output "PASS negative control: $mode loses limbs as reported"
    }
}
$global:LASTEXITCODE=0
