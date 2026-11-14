param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/watoy-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/watoy_sync.obj" "/Fe$output/watoy_sync.exe" agent/tests/watoy_sync_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Watoy controller test compile failed'}
    & "$output/watoy_sync.exe"
    if($LASTEXITCODE -ne 0){throw 'Watoy controller test failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/watoy_budget.obj" "/Fe$output/watoy_budget.exe" agent/tests/watoy_budget_test.cpp /link /OPT:REF
    if($LASTEXITCODE){throw 'Watoy budget compile failed'}
    & "$output/watoy_budget.exe"
    if($LASTEXITCODE){throw 'Watoy budget failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/toy_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/watoy_lifetime.obj" "/Fe$output/watoy_lifetime.exe" agent/tests/watoy_lifetime_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Watoy lifetime test compile failed'}
    & "$output/watoy_lifetime.exe"
    if($LASTEXITCODE -ne 0){throw 'Watoy lifetime test failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/watoy_wrapper.obj" "/Fe$output/watoy_wrapper.exe" agent/tests/watoy_wrapper_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Watoy wrapper test compile failed'}
    & "$output/watoy_wrapper.exe"
    if($LASTEXITCODE -ne 0){throw 'Watoy wrapper test failed'}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/watoy_wrapper_lifetime.obj" "/Fe$output/watoy_wrapper_lifetime.exe" agent/tests/watoy_wrapper_lifetime_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Watoy wrapper lifetime compile failed'}
    & "$output/watoy_wrapper_lifetime.exe"
    if($LASTEXITCODE -ne 0){throw 'Watoy wrapper lifetime failed'}
    if($FaultControls){
        $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/watoy_sync.cpp'))
        $controls=@(
            @{Name='uncertain';Changes=@(@('if(!activated && !dispatched)return false;','if(!activated)return false;'))},
            @{Name='function';Changes=@(@('R::FindDispatchFunctionCached(row.cls.Get(),L"activate")!=row.function.Get()','false'),@('!R::FunctionParams(row.function.Get()).empty()','false'))},
            @{Name='scope';Changes=@(@('g_serial==net::session_serial::Current()','true'))},
            @{Name='post';Changes=@(@(' && row.stack==c.stack',''))},
            @{Name='reservation';Changes=@(@('g_activation.push_back(row);','/* late reservation */'),@('W::State before{};const bool read=W::Read(c.object,before);','W::State before{};const bool read=W::Read(c.object,before);g_activation.push_back(row);'))},
            @{Name='source';Changes=@(@('source.connection==g_session->peerGenerationForSlot(slot)','true'))},
            @{Name='recipient';Changes=@(@('targets.connections[slot]==g_session->peerGenerationForSlot(slot)','true'))},
            @{Name='flush';Changes=@(@('current->state.sequence!=row.state.sequence','false'),@('current->state.sequence==row.state.sequence','true'))},
            @{Name='drain';Changes=@(@('if(!Current(epoch))return;','/* retired completion admitted */'))},
            @{Name='prune';Changes=@(@('if(stale && it!=g_worldRows.end() && it->second.state.sequence==row.state.sequence)','if(stale && it!=g_worldRows.end())'),@('found->second.state.sequence!=rows.at(key).state.sequence','false'))},
            @{Name='admission';Changes=@(@('if(sender!=0 || p.op==ActivateWorld)return;','if(sender!=0 || p.op==ActivateWorld)return; if(cue && g_out.size()>=64)g_lastCue=p.sequence;'))},
            @{Name='payload';Changes=@(@('const Packet p=packet;','const Packet& p=packet;'))}
        )
        foreach($control in $controls){
            $mutant=$source;$changes=$control.Changes
            if($changes[0] -is [string]){$changes=@(,$changes)}
            foreach($change in $changes){
                if(-not $mutant.Contains($change[0])){throw "Missing watoy control anchor: $($control.Name)"}
                $mutant=$mutant.Replace($change[0],$change[1])
            }
            $name='fault_'+$control.Name;$path=Join-Path $output ($name+'.cpp')
            [IO.File]::WriteAllText($path,$mutant)
            $header=Join-Path $output ($name+'.h')
            [IO.File]::WriteAllText($header,('#define WATOY_SYNC_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I agent/tests/toy_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/watoy_lifetime_test.cpp /link /OPT:REF
            if($LASTEXITCODE){throw "Watoy control compile failed: $name"}
            $saved=$ErrorActionPreference
            try{$ErrorActionPreference='Continue';$report=& "$output/$name.exe" $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
            [IO.File]::WriteAllText((Join-Path $output ($name+'.log')),$report)
            if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Watoy control did not reach an assertion: $name"}
            Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
        }
    }
    if($FaultControls){
        $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/watoy.cpp'))
        $controls=@(
            @{Name='native-schema';Changes=@(@('if(actual.size()!=parameters.size())return false;','/* extra parameters admitted */'))},
            @{Name='native-function';Changes=@(@('R::FindDispatchFunctionCached(owner.cls.Get(),name)==function.Get()','true'))},
            @{Name='field-class';Changes=@(@('cls.Get() && R::ClassOf(object.Get())==cls.Get()','true'))},
            @{Name='unknown-zero';Changes=@(@('if(!Field(actor,field,raw))return false;','/* unknown becomes inactive */'))},
            @{Name='read-bindings';Changes=@(@('Audio(actor)==audio &&','true &&'),@('actual==zero','true'))},
            @{Name='activate-current';Changes=@(@('(!current || current())','true'))},
            @{Name='audio-current';Changes=@(@('(current && !current())','false'),@('(!current || current())','true'))},
            @{Name='spawn-services';Changes=@(@('libraryRef.Current() && systemRef.Current() && mathRef.Current() &&','true &&'))},
            @{Name='spawn-binding';Changes=@(@('prop::GetStaticMesh(actor)==mesh.object.Get()','true'))},
            @{Name='spawn-output';Changes=@(@('!std::isfinite(world.RotX)','false'))},
            @{Name='spawn-candidate';Changes=@(@('if(candidate.object.Get())engine::DestroyComponent(candidate.object.Get(),source.object.Get());','/* stale candidate leaked */'))},
            @{Name='view-reentry';Changes=@(@('view.revision==revision','true'),@('lease->revision==revision','true'),@('view.presentation==lease','true'))},
            @{Name='view-copies';Changes=@(@('if(!lease || lease->retired || lease->revision!=admitted)return;','if(!lease)return;'))}
        )
        foreach($control in $controls){
            $mutant=$source;$changes=$control.Changes
            if($changes[0] -is [string]){$changes=@(,$changes)}
            foreach($change in $changes){
                if(-not $mutant.Contains($change[0])){throw "Missing watoy wrapper control anchor: $($control.Name)"}
                $mutant=$mutant.Replace($change[0],$change[1])
            }
            $name='wrapper_fault_'+$control.Name;$path=Join-Path $output ($name+'.cpp')
            [IO.File]::WriteAllText($path,$mutant)
            $header=Join-Path $output ($name+'.h')
            [IO.File]::WriteAllText($header,('#define WATOY_WRAPPER_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/watoy_wrapper_lifetime_test.cpp /link /OPT:REF
            if($LASTEXITCODE){throw "Watoy wrapper control compile failed: $name"}
            $saved=$ErrorActionPreference
            try{$ErrorActionPreference='Continue';$report=& "$output/$name.exe" $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
            [IO.File]::WriteAllText((Join-Path $output ($name+'.log')),$report)
            if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Watoy wrapper control did not reach an assertion: $name"}
            Write-Host ('PASS wrapper fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
        }
    }
    if($CompileSources) {
        foreach($file in @('ue_wrap/actors/watoy','coop/props/watoy_sync','coop/dispatch/event_dispatch_world','coop/session/subsystems')) {
            $name=$file.Replace('/','_')
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if($LASTEXITCODE -ne 0){throw "Watoy source compile failed: $file"}
        }
    }
} finally {Pop-Location}
