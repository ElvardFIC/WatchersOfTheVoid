param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/handtaken-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($script in @('prop_claim_extract','omega_command_extract','handtaken_extract')){
 & "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/$script.py" $output
 if($LASTEXITCODE){throw "HandTaken extraction failed: $script"}
}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/config.obj" "$root/src/votv-coop/src/coop/config/config_registry.cpp"
if($LASTEXITCODE){throw 'HandTaken config compile failed'}
foreach($name in @('handtaken','handtaken_native')){
 $libraries=@();if($name -eq 'handtaken'){$libraries=@("$output/config.obj")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/served_player_stubs" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" @libraries /link /OPT:REF
 if($LASTEXITCODE){throw "HandTaken fixture compile failed: $name"}
 & "$output/$name.exe"
 if($LASTEXITCODE){throw "HandTaken fixture failed: $name"}
}
if($CompileSources){
 foreach($unit in @('coop/creatures/kerfur_command','ue_wrap/engine/engine_mainplayer','ue_wrap/actors/inventory')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "HandTaken production compile failed: $unit"}
 }
}


if($FaultControls){
 $sources=@{
  command=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/kerfur_command.cpp'))
  native=[IO.File]::ReadAllText((Join-Path $output 'handtaken_native.inc'))
  inventory=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/inventory.cpp'))
 }
 foreach($fault in @(
  @{Name='refused_debt';Before='if(!sent)break;ForgetNotice(notice);';After='ForgetNotice(notice);'},
  @{Name='recipient_generation';Before='s->peerGenerationForSlot(h->slot)==h->generation&&';After='true&&'},
  @{Name='recipient_ready';Before='s->SlotWorldReadyRevision(h->slot)==h->ready;';After='true;'},
  @{Name='capacity';Before='queue.size()>=kMaxTakenNotices';After='false'},
  @{Name='exact_notice';Before='if(*it==notice){queue.erase(it);return;}';After='if(true){queue.erase(it);return;}'},
  @{Name='client_role';Before='g_boundExecution->role!=coop::net::Role::Client';After='false'},
  @{Name='cleanup_receipt';Before='g_takenCleanup!=work';After='false';Before2='g_takenCleanup==work';After2='true'},
  @{Name='local_body';Before='local==work->layout->pawn.actor.Raw()&&';After='true&&'},
  @{Name='hand_scalar';Before='if(!scalar)return false;';After='/* unproven */'},
  @{Name='class_match';Before='object.Current()&&name==want';After='object.Current()'},
  @{Name='field_compare';Before='return ReadPtr(body,h->holdOff)==work->expectedHold&&ReadPtr(body,h->grabOff)==work->expectedGrab;';After='return true;'},
  @{Name='inventory_retry';Before='if(!wrote)return;';After='/* consume refusal */'},
  @{Name='refresh_retry';Before='if(!dispatched)return;';After='/* consume refusal */'},
  @{Name='shared_prop';Before='const bool distinct=work->holdMatch&&work->held.actor.Raw()!=work->grabbed.actor.Raw();';After='const bool distinct=work->holdMatch;'},
  @{Name='inventory_caller';Unit='inventory';Before='if(!WriteEquipRecord(staged.data(),record) || !current())return false;';After='if(!WriteEquipRecord(staged.data(),record))return false;'},
  @{Name='native_owner';Unit='native';Before='(!owner||owner())';After='true'},
  @{Name='native_function';Unit='native';Before='if(!frame.valid()||!current()||!fn.Get())return false;';After='if(!frame.valid()||!current())return false;'},
  @{Name='native_dispatch';Unit='native';Before='if(dispatched)*dispatched=true;';After='/* no receipt */'},
  @{Name='release_overlap';Unit='native';Before='if(overlap(grab,handle)||overlap(grab,component)||overlap(handle,component))return false;';After='/* overlap accepted */'},
  @{Name='release_scalar';Unit='native';Before='return f.offset==offset&&f.size==sizeof(void*);';After='return true;'},
  @{Name='release_successor';Unit='native';Before='if(!current()||read(grab)!=actor||read(handle)!=phc||read(component)!=capturedComponent)return false;';After='/* stale fields */'}
 )){
  $unit=if($fault.Unit){$fault.Unit}else{'command'};$original=$sources[$unit]
  if(-not $original.Contains($fault.Before)){throw "Missing HandTaken fault anchor: $($fault.Name)"}
  $mutant=$original.Replace($fault.Before,$fault.After);if($fault.Before2){if(-not $original.Contains($fault.Before2)){throw 'Missing secondary fault anchor'};$mutant=$mutant.Replace($fault.Before2,$fault.After2)}
  $path=Join-Path $output ($fault.Name+'.cpp');[IO.File]::WriteAllText($path,$mutant)
  $define=@{command='OMEGA_COMMAND_SOURCE';native='HANDTAKEN_NATIVE_SOURCE';inventory='INVENTORY_APPLY_SOURCE'}[$unit]
  $header=Join-Path $output 'fault_source.h';[IO.File]::WriteAllText($header,('#define '+$define+' "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  $fixture=if($unit -eq 'command'){'handtaken'}else{'handtaken_native'}
  $libraries=@();if($unit -eq 'command'){$libraries=@("$output/config.obj")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/served_player_stubs" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/FI$header" "/Fo$output/fault.obj" "/Fe$output/fault.exe" "$PSScriptRoot/${fixture}_test.cpp" @libraries /link /OPT:REF
  if($LASTEXITCODE){throw "HandTaken fault compile failed: $($fault.Name)"}
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/fault.exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($fault.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "HandTaken fault did not reach an assertion: $($fault.Name)"}
  Write-Output "PASS HandTaken fault $($fault.Name)"
 }
}
$global:LASTEXITCODE=0
