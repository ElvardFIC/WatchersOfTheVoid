param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/coffee-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/coffee_stubs" "/I$root/src/votv-coop/include" "/Fo$output/controller.obj" "/Fe$output/controller.exe" "$PSScriptRoot/coffee_controller_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Coffee controller compile failed'}
& "$output/controller.exe"
if($LASTEXITCODE){throw 'Coffee controller regression failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$root/src/votv-coop/include" "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" "$PSScriptRoot/coffee_wrapper_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Coffee wrapper compile failed'}
& "$output/wrapper.exe"
if($LASTEXITCODE){throw 'Coffee wrapper regression failed'}
if($FaultControls){
    $cases=@(
        @{Name='invocation';Source='src/votv-coop/src/coop/interactables/coffee_state.cpp';Macro='COFFEE_STATE_SOURCE';Fixture='coffee_controller_test.cpp';Before='slot.invocation == call.invocation && slot.tag == tag &&';After='slot.invocation != 0 && slot.tag == tag &&'},
        @{Name='nested_note';Source='src/votv-coop/src/coop/interactables/coffee_state.cpp';Macro='COFFEE_STATE_SOURCE';Fixture='coffee_controller_test.cpp';Before='if (slot.invocation && (!Current(slot) ||';After='if (slot.invocation && (true || !Current(slot) ||'},
        @{Name='unguarded_publish';Source='src/votv-coop/src/coop/interactables/coffee_state.cpp';Macro='COFFEE_STATE_SOURCE';Fixture='coffee_controller_test.cpp';Before='prop_save_data::PublishWhile(d.scope.pointer, d.actor.Get(), d.key, current)';After='prop_save_data::Publish(d.scope.pointer, d.actor.Get(), d.key)'},
        @{Name='lost_retry';Source='src/votv-coop/src/coop/interactables/coffee_state.cpp';Macro='COFFEE_STATE_SOURCE';Fixture='coffee_controller_test.cpp';Before='} else ++g_publishFailed;';After='} else { ++g_publishFailed; g_debt[i] = {}; }'},
        @{Name='stale_actor';Source='src/votv-coop/src/coop/interactables/coffee_state.cpp';Macro='COFFEE_STATE_SOURCE';Fixture='coffee_controller_test.cpp';Before='Host(n.scope) && n.actor.Get() && n.function.Get()';After='Host(n.scope) && n.actor.Raw() && n.function.Get()'},
        @{Name='sender_gate';Source='src/votv-coop/src/coop/interactables/coffee_state.cpp';Macro='COFFEE_STATE_SOURCE';Fixture='coffee_controller_test.cpp';Before='sender.Current(g_session.load()) && ref.Get()';After='Host(sender.session) && ref.Get()'},
        @{Name='body_lookup';Source='src/votv-coop/src/coop/interactables/coffee_intent.cpp';Macro='COFFEE_INTENT_SOURCE';Fixture='coffee_controller_test.cpp';Before='return current() && bodyRef.Get() && bodyRef.Is(BodyFor(slot)) && indexed == device;';After='return indexed == device;'},
        @{Name='player_write';Source='src/votv-coop/src/ue_wrap/devices/appliance.cpp';Macro='APPLIANCE_WRAPPER_SOURCE';Fixture='coffee_wrapper_test.cpp';Before='if (!f.Set<void*>(L"player", player) || !live()) return false;';After='f.Set<void*>(L"player", player);'},
        @{Name='action_write';Source='src/votv-coop/src/ue_wrap/devices/appliance.cpp';Macro='APPLIANCE_WRAPPER_SOURCE';Fixture='coffee_wrapper_test.cpp';Before='if (!f.Set<uint8_t>(L"action", action) || !live()) return false;';After='f.Set<uint8_t>(L"action", action);'},
        @{Name='bool_serial';Source='src/votv-coop/src/ue_wrap/devices/coffee.cpp';Macro='COFFEE_WRAPPER_SOURCE';Fixture='coffee_wrapper_test.cpp';Before='if (!f.cls.Is(cls)) {';After='if (f.cls.Raw() != cls) {'},
        @{Name='bag_retired';Source='src/votv-coop/src/ue_wrap/devices/coffee.cpp';Macro='COFFEE_WRAPPER_SOURCE';Fixture='coffee_wrapper_test.cpp';Before='o < 0 || !ref.Get()';After='o < 0'}
    )
    foreach($case in $cases){
        $source=[IO.File]::ReadAllText((Join-Path $root $case.Source))
        if(-not $source.Contains($case.Before)){throw "Missing fault anchor: $($case.Name)"}
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $fixture=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($fixture,('#define '+$case.Macro+' "'+$mutant.Replace('\','/')+'"'+"`n"+'#include "'+(Join-Path $PSScriptRoot $case.Fixture).Replace('\','/')+'"'))
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/coffee_stubs" "/I$root/src/votv-coop/src/coop/interactables" "/I$root/src/votv-coop/include" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Fault fixture failed to compile: $($case.Name)"}
        & "$output/$($case.Name).exe"
        if($LASTEXITCODE -ne 1){throw "Fault was not detected: $($case.Name)"}
        Write-Output "PASS rejected fault: $($case.Name)"
    }
}
$global:LASTEXITCODE=0
