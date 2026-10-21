param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/scorch-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root 'build/local-msvc.ps1')}
    foreach($test in @('scorch_spill','scorch_spill_wrapper','explosion_blast_2140')){
        $includes=if($test -eq 'scorch_spill'){@('/I','agent/tests/grime_stubs','/I','agent/tests/placed_save_stubs')}else{@()}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Scorch compile failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE -ne 0){throw "Scorch test failed: $test"}
    }
    if($CompileSources){foreach($file in @('coop/props/drink_spill_sync','ue_wrap/devices/drink_spill','ue_wrap/actors/explosion_blast')){
        $name=$file.Replace('/','_')
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
        if($LASTEXITCODE -ne 0){throw "Scorch source failed: $file"}
    }}
}finally{Pop-Location}
