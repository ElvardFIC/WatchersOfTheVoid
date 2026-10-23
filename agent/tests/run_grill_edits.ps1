param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/grill-edits-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/" "/Fe$output/controller.exe" agent/tests/grill_edits_test.cpp src/votv-coop/src/coop/net/session_serial.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Grill edit controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Grill edit controller failed'}
    if($FaultControls){
        $source=Get-Content 'src/votv-coop/src/coop/interactables/grill_edits.cpp' -Raw
        foreach($name in @('keeps-old-queues','ignores-reentrant-restart')){
            $control=Join-Path $output $name
            New-Item -ItemType Directory -Force -Path $control | Out-Null
            $needle=if($name -eq 'keeps-old-queues'){' || g_serial!=net::session_serial::Current()'}else{'g_serial==net::session_serial::Current()'}
            $replacement=if($name -eq 'keeps-old-queues'){''}else{'true'}
            $mutant=$source.Replace($needle,$replacement)
            if($mutant -eq $source){throw "Missing mutation site: $name"}
            $mutantPath=(Join-Path $control 'grill_edits.inc').Replace('\','/')
            [IO.File]::WriteAllText($mutantPath,$mutant,[Text.UTF8Encoding]::new($false))
            $testPath=(Join-Path $PSScriptRoot 'grill_edits_test.cpp').Replace('\','/')
            $fixture=Join-Path $control 'control.cpp'
            $lines=@(('#define GRILL_EDITS_SOURCE "'+$mutantPath+'"'),('#include "'+$testPath+'"'))
            [IO.File]::WriteAllText($fixture,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/stubs" /I src/votv-coop/include "/Fo$control/" "/Fe$control/control.exe" $fixture src/votv-coop/src/coop/net/session_serial.cpp /link /OPT:REF
            if($LASTEXITCODE -ne 0){throw "Control compile failed: $name"}
            & "$control/control.exe"
            if($LASTEXITCODE -ne 1){throw "Control did not fire its assertion: $name"}
            Write-Output "PASS expected-failure control: $name"
        }
    }
    if($CompileSources){
        foreach($source in @('coop/interactables/grill_edits','coop/interactables/kitchen_sync','ue_wrap/devices/grill','ue_wrap/devices/grill_saved_state','coop/props/prop_save_data','coop/dispatch/event_dispatch_world')){
            $name=$source.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
            if($LASTEXITCODE -ne 0){throw "Grill edit source compile failed: $source"}
        }
    }
}finally{Pop-Location}
