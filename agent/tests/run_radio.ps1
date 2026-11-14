param([switch]$FaultControls,[switch]$ProductionCompile,[string]$BuildDirectory='build/votv-coop-2164')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/radio-ownership-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/run_2125.ps1"
if($LASTEXITCODE){throw 'Radio controller regression failed'}
& powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/run_2127.ps1"
if($LASTEXITCODE){throw 'Radio wrapper regression failed'}
if($FaultControls){
    . (Join-Path $root '../_toolchain/env.ps1')
    $cases=@(
        @{Name='peer_capacity';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='sender>=net::kMaxPeers';After='sender>=4'},
        @{Name='ack_capacity';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='i<net::kMaxPeers;++i) next.ack[i]';After='i<4;++i) next.ack[i]'},
        @{Name='sender_generation';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='scope.session->peerGenerationForSlot(sender)==generation &&';After='true &&'},
        @{Name='inflight_receive';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='|| g_receiving[sender]';After=''},
        @{Name='old_pending_send';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='g_pending.front().revision==q.revision';After='true'},
        @{Name='old_canonical_paint';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='it->second.revision==row.revision';After='true'},
        @{Name='cancelled_ui';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='!SG::IsInvocationActive(u.invocation)';After='false'},
        @{Name='tick_reentry';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='|| g_ticking';After=''},
        @{Name='key_eid_conflict';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='keyed && numbered && keyed!=numbered';After='false'},
        @{Name='future_ack';File='coop/world/radio_sync.cpp';Macro='RADIO_SYNC_SOURCE';Fixture='radio_2125_test.cpp';Before='if (After(p.ack[me],g_offered)) return;';After=''},
        @{Name='owned_filename';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before='R::FString link{path.data(),';After='R::FString link{reinterpret_cast<R::FString*>(original.data()+layout.link)->Data,'},
        @{Name='url_retirement_free';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before='if (read && result.Data) R::EngineFree(result.Data);';After='if (false) R::EngineFree(result.Data);'},
        @{Name='query_class_serial';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before='clsRef.Get()';After='clsRef.Raw()'},
        @{Name='media_rebind';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before='return player.Is(bound);';After='return player.Get()!=nullptr;'},
        @{Name='layout_snapshot';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before='s.track=Id(static_cast<uint8_t*>(actor)+op.data,op.layout)';After='s.track=Id(static_cast<uint8_t*>(actor)+op.data,g_layout)'},
        @{Name='layout_function_serial';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before='type.Get() && fn.Get() && stride>0';After='type.Get() && fn.Raw() && stride>0'},
        @{Name='fstring_capacity';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before='s.Max<s.Num';After='false'},
        @{Name='fstring_terminator';File='ue_wrap/world/radio_player.cpp';Macro='RADIO_PLAYER_SOURCE';Fixture='radio_wrapper_2127_test.cpp';Before="s.Data[s.Num-1]!=L'\0'";After='false'}
    )
    foreach($case in $cases){
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
        if(-not $source.Contains($case.Before)){throw "Missing fault anchor: $($case.Name)"}
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $fixture=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($fixture,('#define '+$case.Macro+' "'+$mutant.Replace('\','/')+'"'+"`n"+'#include "'+(Join-Path $PSScriptRoot $case.Fixture).Replace('\','/')+'"'))
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/stubs" "/I$root/src/votv-coop/include" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Radio fault compile failed: $($case.Name)"}
        & "$output/$($case.Name).exe"
        if($LASTEXITCODE -ne 1){throw "Fault was not detected: $($case.Name)"}
        Write-Output "PASS rejected radio fault: $($case.Name)"
    }
}
if($ProductionCompile){
    . (Join-Path $root '../_toolchain/env.ps1')
    $ownPaths=@('src/votv-coop/src/coop/world/radio_sync.cpp','src/votv-coop/src/ue_wrap/world/radio_player.cpp','src/votv-coop/include/ue_wrap/world/radio_player.h')
    $before=@{};foreach($path in $ownPaths){$before[$path]=(Get-FileHash (Join-Path $root $path) -Algorithm SHA256).Hash}
    $build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $root $BuildDirectory}
    $ninja=[IO.File]::ReadAllText((Join-Path $build 'build.ninja'))
    $protocolPath=Join-Path $root 'src/votv-coop/include/coop/net/protocol.h'
    $wire=[regex]::Match([IO.File]::ReadAllText($protocolPath),'(?s)struct RadioStatePayload \{.*?\n\};').Value
    if(-not $wire){throw 'Radio wire schema missing'}
    $compiles=@()
    foreach($name in @('radio_sync','radio_player')){
        $pattern='(?m)^build CMakeFiles[^\r\n]*\\'+$name+'\.cpp\.obj:[^\r\n]*\r?\n  DEFINES = ([^\r\n]*)\r?\n  FLAGS = ([^\r\n]*)\r?\n  INCLUDES = ([^\r\n]*)'
        $entry=[regex]::Match($ninja,$pattern)
        if(-not $entry.Success){throw "Release compiler settings missing: $name"}
        # The fingerprint-table macro is used by function_row.cpp, not either TU
        # compiled here; omit its Ninja/shell quoting rather than evaluating shell text.
        $defines=@($entry.Groups[1].Value -split '\s+' | Where-Object {$_ -and $_ -notmatch 'VOTVCOOP_FINGERPRINT_TABLE'})
        $flags=@($entry.Groups[2].Value -split '\s+' | Where-Object {$_})
        $includes=@($entry.Groups[3].Value -split '\s+' | Where-Object {$_})
        if(($defines+$flags+$includes) -match '"'){throw 'Quoted compiler arguments require explicit parsing'}
        $path=if($name -eq 'radio_sync'){$ownPaths[0]}else{$ownPaths[1]}
        $object=Join-Path $output ($name+'_release.obj')
        & cl /nologo /c @defines @flags @includes "/Fo$object" "/Fd$output/$name.pdb" (Join-Path $root $path)
        if($LASTEXITCODE){throw "Production Release compilation failed: $name"}
        $compiles+=[pscustomobject]@{Source=$path;Object=$object;Defines=$defines;Flags=$flags;Includes=$includes}
    }
    foreach($path in $ownPaths){if((Get-FileHash (Join-Path $root $path) -Algorithm SHA256).Hash -ne $before[$path]){throw "Radio source changed during compile: $path"}}
    $afterWire=[regex]::Match([IO.File]::ReadAllText($protocolPath),'(?s)struct RadioStatePayload \{.*?\n\};').Value
    if($wire -ne $afterWire){throw 'Radio wire schema changed during compile'}
    [pscustomobject]@{RadioSourcesStable=$true;RadioWireSchemaStable=$true;SourceSHA256=$before;Compiles=$compiles;LinkedDll=$false} |
        ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'production-compile.json') -Encoding UTF8
    Write-Output 'PASS two production radio translation units with Release compiler settings; no DLL link'
}
$global:LASTEXITCODE=0
