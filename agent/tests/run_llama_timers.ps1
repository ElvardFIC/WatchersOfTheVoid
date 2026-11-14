$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/llama-timers-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 /I agent/tests/llama_timer_stubs /I agent/tests/state_2145_stubs /I src/votv-coop/include "/Fo$output/test.obj" "/Fe$output/test.exe" agent/tests/llama_timers_test.cpp /link /OPT:REF
    if($LASTEXITCODE){throw 'Llama timer fixture compile failed'}
    & "$output/test.exe"
    if($LASTEXITCODE){throw 'Llama timer fixture failed'}
    foreach($unit in @('ue_wrap/actors/llama_timer','coop/props/llama_timers')) {
        $leaf=$unit.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$unit.cpp"
        if($LASTEXITCODE){throw "Llama timer production compile failed: $unit"}
    }
}finally{Pop-Location}
