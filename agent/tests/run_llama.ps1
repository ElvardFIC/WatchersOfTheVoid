$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/llama-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    foreach($test in @('sync','native')){
        $extra=@();if($test -eq 'sync'){$extra=@('/DTEST_STATE_MAX_PEERS=32','/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @extra /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/llama_${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE){throw "Llama $test fixture compile failed"}
        & "$output/$test.exe"
        if($LASTEXITCODE){throw "Llama $test fixture failed"}
    }
    foreach($unit in @('ue_wrap/actors/llama','coop/props/llama_sync','coop/dispatch/event_dispatch_world','coop/session/subsystems')){
        $leaf=$unit.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$unit.cpp"
        if($LASTEXITCODE){throw "Llama production compile failed: $unit"}
    }
}finally{Pop-Location}
