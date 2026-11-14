param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/omega-command-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($script in @('prop_claim_extract','omega_command_extract')){
 & "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/$script.py" $output
 if($LASTEXITCODE){throw "Omega extraction failed: $script"}
}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/config.obj" "$root/src/votv-coop/src/coop/config/config_registry.cpp"
if($LASTEXITCODE){throw 'Omega config compile failed'}
foreach($name in @('omega_command','omega_verb_native')){
 $libraries=@();if($name -eq 'omega_command'){$libraries=@("$output/config.obj")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/served_player_stubs" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" @libraries /link /OPT:REF
 if($LASTEXITCODE){throw "Omega fixture compile failed: $name"}
 & "$output/$name.exe"
 if($LASTEXITCODE){throw "Omega fixture failed: $name"}
}
if($CompileSources){
 foreach($unit in @('coop/creatures/kerfur_command','coop/creatures/served_player','ue_wrap/actors/kerfur')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "Omega production compile failed: $unit"}
 }
}
if($FaultControls){
 $sources=@{
  command=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/kerfur_command.cpp'))
  service=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/served_player.cpp'))
  verb=[IO.File]::ReadAllText((Join-Path $output 'omega_verb.inc'))
 }
 foreach($fault in @(
  @{Name='world';Before='world==ue_wrap::world_identity::Generation()&&';After='true&&'},
  @{Name='serial';Before='serial==coop::net::session_serial::Current()&&';After='true&&'},
  @{Name='sender_generation';Before='e.scope.session->peerGenerationForSlot(e.slot)==e.generation&&';After='true&&'},
  @{Name='sender_ready';Before='e.scope.session->SlotWorldReadyRevision(e.slot)==e.ready&&';After='true&&'},
  @{Name='incarnation';Before='now.incarnation==e.identity.incarnation&&';After='true&&'},
  @{Name='service_proof';Before='if(e->service){if(!SP::Owns(e->service))return false;}';After='if(e->service){}'},
  @{Name='job_read';Before='!K::ReadKerfurState(actor,state,spooky,face)';After='false&&K::ReadKerfurState(actor,state,spooky,face)'},
  @{Name='host_guard';Before='t_inHostExec=previous;';After='t_inHostExec=false;'},
  @{Name='hand_layout';Before='if(!scalar)return false;';After='/* unproven field */'},
  @{Name='hand_field';Before='hand->wroteHold&&ReadPtr(pawn,hand->holdOff)==hand->stagedHold';After='hand->wroteHold'},
  @{Name='hand_weak';Before='hand->previousHold.Get()';After='hand->previousHold.Raw()'},
  @{Name='hand_receipt';Before='it->second==hand';After='true'},
  @{Name='uncertain_rollback';Before='if(!e->dispatched&&e->service)';After='if(e->service)'},
  @{Name='captured_cas';Unit='service';Before='row==before.row&&';After='true&&'},
  @{Name='host_reservation';Unit='service';Before='if(!row->slot){if(row->seams)return false;}';After='if(!row->slot)return false;'},
  @{Name='prior_recipient';Unit='service';Before='const bool valid=!previous||Current(previous);';After='const bool valid=true;'},
  @{Name='restore_receipt';Unit='service';Before='if(!before.state||!before.state->scope.Current()||before.state->robot!=attempt.robot||!Owns(attempt)||(current&&!current()))return false;';After='if(!before.state)return false; Unserve(attempt.robot);if(before.state->row)Serve(attempt.robot,before.state->row->slot,before.state->row->seams);return true;'},
  @{Name='verb_player_type';Unit='verb';Before='playerType!=L"ObjectProperty"';After='false'},
  @{Name='verb_name_type';Unit='verb';Before='nameType!=L"StrProperty"';After='false'},
  @{Name='verb_name_size';Unit='verb';Before='nsize!=16';After='false'},
  @{Name='verb_overlap';Unit='verb';Before='(playerOff<nameOff+16&&nameOff<playerOff+sizeof(void*))';After='false'},
  @{Name='verb_receipt';Unit='verb';Before='if(dispatched)*dispatched=true;';After='/* no receipt */'},
  @{Name='verb_owned_text';Unit='verb';Before='Text text{ownedName.c_str(),int32_t(count),int32_t(count)};';After='Text text{name,int32_t(count),int32_t(count)};'},
  @{Name='verb_caller';Unit='verb';Before='(!current||current())';After='true'}
 )){
  $unit=if($fault.Unit){$fault.Unit}else{'command'};$original=$sources[$unit]
  if(-not $original.Contains($fault.Before)){throw "Missing Omega fault anchor: $($fault.Name)"}
  $path=Join-Path $output ($fault.Name+'.cpp');[IO.File]::WriteAllText($path,$original.Replace($fault.Before,$fault.After))
  $define=@{command='OMEGA_COMMAND_SOURCE';service='SERVED_PLAYER_SOURCE';verb='OMEGA_VERB_SOURCE'}[$unit]
  $header=Join-Path $output 'fault_source.h';[IO.File]::WriteAllText($header,('#define '+$define+' "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  $fixture=if($unit -eq 'verb'){'omega_verb_native'}else{'omega_command'}
  $libraries=@();if($unit -ne 'verb'){$libraries=@("$output/config.obj")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/served_player_stubs" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/FI$header" "/Fo$output/fault.obj" "/Fe$output/fault.exe" "$PSScriptRoot/${fixture}_test.cpp" @libraries /link /OPT:REF
  if($LASTEXITCODE){throw "Omega fault compile failed: $($fault.Name)"}
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/fault.exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($fault.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Omega fault did not reach an assertion: $($fault.Name)"}
  Write-Output "PASS Omega fault $($fault.Name)"
 }
}
$global:LASTEXITCODE=0
