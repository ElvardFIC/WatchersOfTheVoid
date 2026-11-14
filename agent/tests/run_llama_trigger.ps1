$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/llama-trigger-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    foreach($test in @('native','controller')) {
        $extra=@();$source='agent/tests/llama_trigger_native_test.cpp'
        if($test -eq 'controller'){
            $extra=@('/DTEST_STATE_MAX_PEERS=32','/I','agent/tests/llama_trigger_stubs','/I','agent/tests/state_2145_stubs')
            $source='agent/tests/llama_trigger_test.cpp'
        }
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @extra /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" $source /link /OPT:REF
        if($LASTEXITCODE){throw "Llama trigger $test compile failed"}
        & "$output/$test.exe"
        if($LASTEXITCODE){throw "Llama trigger $test fixture failed"}
    }
    foreach($unit in @('ue_wrap/actors/llama_trigger','coop/props/llama_trigger')) {
        $leaf=$unit.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$unit.cpp"
        if($LASTEXITCODE){throw "Llama trigger production compile failed: $unit"}
    }
} finally {Pop-Location}
