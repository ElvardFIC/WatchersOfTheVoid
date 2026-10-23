param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/container-apply-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$python=Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
& $python "$PSScriptRoot/container_apply_extract.py"
if($LASTEXITCODE){throw 'Container production extraction failed'}
$reflection=Get-Content (Join-Path $root 'src/votv-coop/src/ue_wrap/core/reflection_props.cpp') -Raw
$offset=[regex]::Match($reflection,'(?ms)^int32_t InstanceOffset::Of\(.*?^\}').Value
if(-not $offset){throw 'Production InstanceOffset missing'}
[IO.File]::WriteAllText((Join-Path $output 'instance_offset_production.inc'),("namespace ue_wrap::reflection {`n"+$offset+"`n}"),[Text.UTF8Encoding]::new($false))
& cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/container_apply_stubs" "/I$output" "/I$root/src/votv-coop/include" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/container_apply_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Container apply fixture compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Container apply regression failed'}
if($FaultControls) {
    $source=Get-Content (Join-Path $output 'container_apply_production.inc') -Raw
    $park=Get-Content (Join-Path $root 'src/votv-coop/src/coop/props/container_park.cpp') -Raw
    foreach($name in @('unchecked-record','stale-commit','lost-relay','whole-byte-boundary','old-park-replay','lost-new-park','false-empty-slice')) {
        $mutant=$source;$parkMutant=$park
        switch($name) {
            'unchecked-record' {$mutant=$source.Replace('const bool complete=SR::WriteSaveRecordChecked(', 'const bool complete=(SR::WriteSaveRecord(').Replace('recs[i],resolve);','recs[i],resolve),true);')}
            'stale-commit' {$mutant=$source.Replace('return source.Current() && actorRef.Get()', 'return true && actorRef.Get()')}
            'lost-relay' {$mutant=$source.Replace('g_seedRetry.Queue(eid,slot,peer);','(void)eid;')}
            'whole-byte-boundary' {$mutant=$source.Replace('return ci::IsWorldInventory(inv);','int32_t off=-1;uint8_t mask=0;return R::FindBoolProperty(R::ClassOf(inv),L"Player",off,mask) && !*(static_cast<uint8_t*>(inv)+off);')}
            'old-park-replay' {$mutant=$source.Replace('pk::Retire(eid,parked);','(void)parked;')}
            'lost-new-park' {$parkMutant=$park.Replace(' || it->second.revision!=snapshot.revision','')}
            'false-empty-slice' {$mutant=$source.Replace('SR::Arr objs{};if(!SR::TryReadArr(slot,0,objs))return SliceRead::Unreadable;','const SR::Arr objs=SR::ReadArr(slot,0);')}
        }
        if($mutant -eq $source -and $parkMutant -eq $park){throw "Missing control site: $name"}
        $control=Join-Path $output $name
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $inc=(Join-Path $control 'container.inc').Replace('\','/')
        $parkInc=(Join-Path $control 'park.inc').Replace('\','/')
        [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($parkInc,$parkMutant,[Text.UTF8Encoding]::new($false))
        $test=(Join-Path $PSScriptRoot 'container_apply_test.cpp').Replace('\','/')
        $fixture=Join-Path $control 'control.cpp'
        $lines=@(('#define CONTAINER_APPLY_FUNCTIONS "'+$inc+'"'),('#define CONTAINER_PARK_SOURCE "'+$parkInc+'"'),('#include "'+$test+'"'))
        [IO.File]::WriteAllText($fixture,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/container_apply_stubs" "/I$output" "/I$root/src/votv-coop/include" `
            "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Control compile failed: $name"}
        & "$control/test.exe"
        if($LASTEXITCODE -ne 1){throw "Control did not detect removed protection: $name"}
        Write-Output "PASS expected-failure control: $name"
    }
}
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/container_contents_sync.obj" "$root/src/votv-coop/src/coop/props/container_contents_sync.cpp"
if($LASTEXITCODE){throw 'Container contents production compile failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/container_park.obj" "$root/src/votv-coop/src/coop/props/container_park.cpp"
if($LASTEXITCODE){throw 'Container park production compile failed'}
Write-Output 'PASS container apply checks and production compilation'
exit 0
