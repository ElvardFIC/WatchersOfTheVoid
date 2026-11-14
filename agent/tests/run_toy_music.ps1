param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/toy-music-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/dingus_music_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Toy music wrapper compile failed'}
    & "$output/wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Toy music wrapper failed'}
    foreach($test in @('toy_music_phase','toy_music_lifetime','toy_music_clock_wrapper')){
        $includes=@('/I','src/votv-coop/include')
        if($test -ne 'toy_music_clock_wrapper'){$includes=@('/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')+$includes}
        if($test -eq 'toy_music_lifetime'){$includes=@('/I','agent/tests/toy_stubs')+$includes}
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Toy music compile failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE -ne 0){throw "Toy music failed: $test"}
    }
    if($FaultControls){
        $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/dingus_music_sync.cpp'))
        $controls=@(
            @{Name='session-serial';Changes=@(@('g_serial==net::session_serial::Current()','true'),@('g_serial!=net::session_serial::Current()','false'))},
            @{Name='source-connection';Changes=@(@('g_session->peerGenerationForSlot(sender)!=connection','false'),@('g_session->peerGenerationForSlot(sender)==connection','true'))},
            @{Name='recipient-connection';Changes=@(@('targets.connections[slot]==g_session->peerGenerationForSlot(slot)','true'))},
            @{Name='exact-post';Changes=@(@(' && row.stack==call.stack',''))},
            @{Name='reserved-pre';Changes=@(@('g_captures.push_back(row);','/* late reservation */'),@('row.key=Key(call.object);','row.key=Key(call.object);g_captures.push_back(row);'))},
            @{Name='overflow-dedup';Changes=@(@('if(sender!=0)return;','if(sender!=0)return; if(p.op==4 && g_pulses.size()>=32)g_lastPulse=p.sequence;'))},
            @{Name='packet-copy';Changes=@(@('const Packet p=packet;','const Packet& p=packet;'))},
            @{Name='send-budget';Changes=@(@('g_sendBudget=64;','g_sendBudget=4096;'))},
            @{Name='callback-expiry';Changes=@(@(' && Fresh(pulse.expires);',';'))},
            @{Name='key-domain';Changes=@(@('return c>=33 && c<=126;','return true;'))}
        )
        foreach($control in $controls){
            $mutant=$source
            $changes=$control.Changes
            if($changes[0] -is [string]){$changes=@(,$changes)}
            foreach($change in $changes){
                if(-not $mutant.Contains($change[0])){throw "Missing toy control anchor: $($control.Name)"}
                $mutant=$mutant.Replace($change[0],$change[1])
            }
            $name='fault_'+$control.Name
            $path=Join-Path $output ($name+'.cpp');[IO.File]::WriteAllText($path,$mutant)
            $header=Join-Path $output ($name+'.h')
            [IO.File]::WriteAllText($header,('#define TOY_MUSIC_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I agent/tests/toy_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/toy_music_lifetime_test.cpp /link /OPT:REF
            if($LASTEXITCODE){throw "Toy control compile failed: $name"}
            $saved=$ErrorActionPreference
            try{$ErrorActionPreference='Continue';$report=& "$output/$name.exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
            [IO.File]::WriteAllText((Join-Path $output ($name+'.log')),$report)
            if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Toy control did not reach an assertion: $name"}
            Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
        }
    }
    if($CompileSources){
        foreach($file in @('ue_wrap/actors/dingus_music','ue_wrap/core/component_calls','coop/player/hand_toy_audio','coop/props/dingus_music_sync')){
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Toy music source compile failed: $file"}
        }
    }
} finally {Pop-Location}
