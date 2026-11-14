param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run_player_reports.ps1') -Only wisp_spawn_race
if($LASTEXITCODE -ne 0){throw 'Wisp animation regression failed'}
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/wisp-animation-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" `
    "/Fo$output/wisp_montage_test.obj" "/Fe$output/wisp_montage_test.exe" `
    "$PSScriptRoot/wisp_montage_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Wisp montage regression compile failed'}
& "$output/wisp_montage_test.exe"
if($LASTEXITCODE -ne 0){throw 'Wisp montage regression failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/wisp_stubs" "/I$checkout/src/votv-coop/include" `
    "/Fo$output/wisp_release_test.obj" "/Fe$output/wisp_release_test.exe" `
    "$PSScriptRoot/wisp_release_test.cpp" /link /OPT:REF
if($LASTEXITCODE -ne 0){throw 'Wisp release regression compile failed'}
& "$output/wisp_release_test.exe"
if($LASTEXITCODE -ne 0){throw 'Wisp release regression failed'}
& (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') "$PSScriptRoot/wisp_host_extract.py" $output
if($LASTEXITCODE){throw 'Wisp host body extraction/wiring failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$checkout/src/votv-coop/include" "/I$output" `
    "/Fo$output/wisp_host.obj" "/Fe$output/wisp_host.exe" "$PSScriptRoot/wisp_host_pending_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Wisp host pending fixture compile failed'}
& "$output/wisp_host.exe"
if($LASTEXITCODE){throw 'Wisp host pending regression failed'}
if($FaultControls) {
    foreach($name in @('hold-body','mesh-ready','release-lease','cleanup-death')) {
        $macro='WISP_TEAR_SOURCE';$fixture='wisp_spawn_race_test.cpp'
        $unit='coop/creatures/wisp_tear_mirror'
        $pattern='plays==before&&T::g_pendingTears.size()==1'
        if($name -eq 'hold-body'){$macro='WISP_HOLD_SOURCE';$fixture='wisp_release_test.cpp';$unit='coop/creatures/wisp_grab_hold';$pattern='p==&test::player'}
        if($name -eq 'release-lease'){$macro='WISP_NATIVE_SOURCE';$fixture='wisp_montage_test.cpp';$unit='ue_wrap/actors/wisp';$pattern='ReleaseGrabOnLocalPlayerWhile'}
        $source=Get-Content -LiteralPath "$checkout/src/votv-coop/src/$unit.cpp" -Raw
        $mutant=switch($name) {
            'hold-body' {$source.Replace('hold.victim.Get() != puppet || ','')}
            'mesh-ready' {$source.Replace('if (!ue_wrap::wisp::ForceMeshTick(wispActor)) return false;','ue_wrap::wisp::ForceMeshTick(wispActor);')}
            'release-lease' {
                $start=$source.IndexOf('bool ReleaseGrabOnLocalPlayerWhile(')
                $source.Substring(0,$start)+$source.Substring($start).Replace('(!current || current())','true')
            }
            'cleanup-death' {$pattern='deaths==before';$source -replace '(?s)if \(!alive \|\| epoch!=g_tearEpoch.*?\) return;','if (false) return;'}
        }
        if($mutant -eq $source){throw "Wisp control did not remove its protection: $name"}
        $control=Join-Path $output "control-$name"
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $inc=Join-Path $control 'source.cpp'
        [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
        $lines=@(('#define '+$macro+' "'+$inc.Replace('\','/')+'"'),('#include "'+(Join-Path $PSScriptRoot $fixture).Replace('\','/')+'"'))
        $entry=Join-Path $control 'test.cpp'
        [IO.File]::WriteAllText($entry,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/wisp_stubs" "/I$checkout/src/votv-coop/include" `
            "/Fo$control/test.obj" "/Fe$control/test.exe" $entry /link /OPT:REF
        if($LASTEXITCODE){throw "Wisp fault-control compile failed: $name"}
        $ErrorActionPreference='Continue'
        & "$control/test.exe" *> (Join-Path $control 'failure.log')
        $code=$LASTEXITCODE
        $ErrorActionPreference='Stop'
        if($code -eq 0 -or -not (Get-Content -LiteralPath (Join-Path $control 'failure.log') -Raw).Contains($pattern)) {
            throw "Wisp fault control missed its intended assertion: $name"
        }
        Write-Output "PASS expected-failure control: $name"
    }
}
foreach($unit in @('coop/creatures/wisp_tear_mirror','coop/creatures/wisp_grab_hold','coop/creatures/wisp_attack_sync','ue_wrap/actors/wisp')){
    $name=$unit.Replace('/','_')
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" `
        "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/$unit.cpp"
    if($LASTEXITCODE -ne 0){throw "Production compile failed: $unit"}
}
