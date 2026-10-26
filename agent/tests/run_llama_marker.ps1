$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/llama-marker-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    foreach($fixture in @('native','sync')) {
        $extra=@();if($fixture -eq 'sync'){$extra=@('/I','agent/tests/placed_save_stubs')}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @extra /I src/votv-coop/include "/Fo$output/$fixture.obj" "/Fe$output/$fixture.exe" "agent/tests/llama_marker_${fixture}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE){throw "Llama marker $fixture fixture compile failed"}
        & "$output/$fixture.exe"
        if($LASTEXITCODE){throw "Llama marker $fixture fixture failed"}
    }
    foreach($unit in @('ue_wrap/actors/llama_marker','ue_wrap/actors/placed_save','ue_wrap/actors/placed_save_record','coop/world/placed_save_sync')) {
        $leaf=$unit.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$unit.cpp"
        if($LASTEXITCODE){throw "Llama marker production compile failed: $unit"}
    }
}finally{Pop-Location}
