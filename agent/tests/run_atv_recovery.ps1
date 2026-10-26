$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/atv-recovery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
    function Extract($path,$names,$destination) {
        $source=[IO.File]::ReadAllText((Join-Path $checkout $path))
        $bodies=foreach($name in $names){
            $body=[regex]::Match($source,"(?ms)^(?:void\*|void|bool) ${name}\(.*?^\}")
            if(-not $body.Success){throw "Production function missing: $name"}
            $body.Value
        }
        [IO.File]::WriteAllText((Join-Path $output $destination),($bodies -join "`r`n"),[Text.UTF8Encoding]::new($false))
    }
    Extract 'src/votv-coop/src/coop/interactables/atv_sync.cpp' @('EnsureHonkWatch','Install') 'atv_install.inc'
    Extract 'src/votv-coop/src/ue_wrap/devices/atv.cpp' @('TeleportRig','SpawnMirror') 'atv_spawn.inc'
    foreach($name in @('atv_recovery','atv_spawn_preparation')){
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy /I agent/tests/state_2145_stubs /I src/votv-coop/include "/I$output" `
            "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE){throw "ATV compile failed: $name"}
        if($name -eq 'atv_recovery'){
            foreach($mode in @('metadata','capacity','frame')){
                & "$output/$name.exe" $mode
                if($LASTEXITCODE){throw "ATV recovery failed: $mode"}
            }
        }else{
            & "$output/$name.exe"
            if($LASTEXITCODE){throw "ATV regression failed: $name"}
        }
    }
    foreach($source in @('coop/interactables/atv_hit_guard','coop/interactables/atv_sync','ue_wrap/devices/atv')){
        $leaf=Split-Path -Leaf $source
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$source.cpp"
        if($LASTEXITCODE){throw "ATV production compile failed: $source"}
    }
} finally {Pop-Location}
