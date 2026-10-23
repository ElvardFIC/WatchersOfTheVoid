param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/profile-join-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/profile_2169_stubs" "/I$root/src/votv-coop/include" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/profile_join_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Profile join compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Profile join regression failed'}
if($FaultControls) {
    $source=Get-Content (Join-Path $root 'src/votv-coop/src/coop/items/player_inventory_sync.cpp') -Raw
    $apply=Get-Content (Join-Path $root 'src/votv-coop/src/coop/items/player_join_apply.inc') -Raw
    foreach($name in @('old-world','old-profile','old-request','unguarded-chunks','borrowed-guid','stale-sent-latch','left-slot-send')) {
        $mutant=$source;$joinMutant=$apply
        switch($name) {
            'old-world' {$joinMutant=$apply.Replace('world==ue_wrap::world_identity::Generation() && serial==coop::net::session_serial::Current() &&','true &&')}
            'old-profile' {$joinMutant=$apply.Replace('revision==g_pendingRevision && haveProfile==g_hasPendingApply.load(std::memory_order_acquire) &&','true &&')}
            'old-request' {$joinMutant=$apply.Replace('g_joinApplyState.load(std::memory_order_acquire)==request &&','true &&')}
            'unguarded-chunks' {$mutant=$source.Replace('sequence, blob,current))','sequence, blob,[]{return true;}))')}
            'borrowed-guid' {$mutant=$source.Replace('const std::string guid = coop::player_handshake::GuidForSlot(peerSlot);','const std::string& guid = coop::player_handshake::GuidForSlot(peerSlot);')}
            'stale-sent-latch' {$mutant=$source.Replace('g_applySentGeneration[slot]!=generation || g_applySentGuid[slot]!=guid','false')}
            'left-slot-send' {$mutant=$source.Replace('++g_hostSendSeq[peerSlot]; // cancel an in-flight send even before transport generation changes','// no disconnect fence')}
        }
        if($mutant -eq $source -and $joinMutant -eq $apply){throw "Missing profile control site: $name"}
        $control=Join-Path $output $name
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $inc=(Join-Path $control 'profile.inc').Replace('\','/')
        $joinInc=(Join-Path $control 'join.inc').Replace('\','/')
        [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($joinInc,$joinMutant,[Text.UTF8Encoding]::new($false))
        $test=(Join-Path $PSScriptRoot 'profile_join_test.cpp').Replace('\','/')
        $fixture=Join-Path $control 'control.cpp'
        $lines=@(('#define PLAYER_PROFILE_SOURCE "'+$inc+'"'),('#define PROFILE_JOIN_APPLY_SOURCE "'+$joinInc+'"'),('#include "'+$test+'"'))
        [IO.File]::WriteAllText($fixture,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$PSScriptRoot/profile_2169_stubs" "/I$root/src/votv-coop/include" `
            "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Profile control compile failed: $name"}
        & "$control/test.exe"
        if($LASTEXITCODE -ne 1){throw "Profile control did not detect removed protection: $name"}
        Write-Output "PASS expected-failure control: $name"
    }
}
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/player_inventory_sync.obj" "$root/src/votv-coop/src/coop/items/player_inventory_sync.cpp"
if($LASTEXITCODE){throw 'Profile controller compilation failed'}
exit 0
