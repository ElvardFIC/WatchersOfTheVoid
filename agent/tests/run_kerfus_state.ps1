param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/kerfus-state-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/prop_claim_extract.py" $output
if($LASTEXITCODE){throw 'Kerfus dependencies extraction failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/config.obj" "$root/src/votv-coop/src/coop/config/config_registry.cpp"
if($LASTEXITCODE){throw 'Kerfus config compile failed'}
foreach($name in @('kerfus_state','kerfus_native')){
 $libraries=@();if($name -eq 'kerfus_state'){$libraries=@("$output/config.obj")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" @libraries /link /OPT:REF
 if($LASTEXITCODE){throw "Kerfus fixture compile failed: $name"}
 & "$output/$name.exe"
 if($LASTEXITCODE){throw "Kerfus fixture failed: $name"}
}
if($CompileSources){
 foreach($unit in @('coop/creatures/kerfus_state','ue_wrap/actors/kerfus')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "Kerfus production compile failed: $unit"}
 }
}
if($FaultControls){
 $manager=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/kerfus_state.cpp'))
 $native=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/kerfus.cpp'))
 foreach($fault in @(
  @{Name='finite_packet';Before='std::isfinite(p.energy)&&';After='true&&'},
  @{Name='recipient_generation';Before='s->peerGenerationForSlot(send->slot)==send->generation&&';After='true&&'},
  @{Name='recipient_world_revision';Before='s->SlotWorldReadyRevision(send->slot)==send->ready&&';After='true&&'},
  @{Name='refusal';Before='if(!current()||!accepted)continue;';After='if(!current())continue;'},
  @{Name='receipt';Before='it->second==send&&Owner(send)';After='Owner(send)'},
  @{Name='retry_pacing';Before='Paced(send->attempted,send->at,now)';After='false'},
  @{Name='budget';Before='attempts++>=kAttempts';After='false'},
  @{Name='incarnation';Before='now.incarnation==was.incarnation&&';After='true&&'},
  @{Name='refresh_carry';Before='row->needsRefresh=old->needsRefresh&&old->wroteActive&&!old->updDispatched;';After='row->needsRefresh=false;'},
  @{Name='dispatch_receipt';Before='&row->updDispatched);';After='nullptr);'},
  @{Name='write_checkpoint';Before='if(!row->wroteCharging)';After='if(true)'},
  @{Name='class_retry';Before='if(!kind){g_wrongClassSaid.insert(row->packet.elementId);return;}';After='if(!kind){Finish(row);return;}'},
  @{Name='repeat_convergence';Before='if(old&&!old->done&&old->identity.incarnation==identity.incarnation&&Same';After='if(old&&old->identity.incarnation==identity.incarnation&&Same'},
  @{Name='field_type';Native=$true;Before='type!=L"FloatProperty"';After='false'},
  @{Name='field_extent';Native=$true;Before='member.size!=sizeof(float)';After='false'},
  @{Name='boolean_overlap';Native=$true;Before='(fields.activeOff==fields.chargingOff&&(fields.activeMask&fields.chargingMask))';After='false'},
  @{Name='finite_write';Native=$true;Before='if(!std::isfinite(energy))return false;';After='/* accepts invalid energy */'},
  @{Name='function_checkpoint';Native=$true;Before='if(!owned()||size<=0';After='if(size<=0'},
  @{Name='dispatch_consumed';Native=$true;Before='if(dispatched)*dispatched=true;';After='/* missing dispatch receipt */'}
 )){
  $original=if($fault.Native){$native}else{$manager}
  if(-not $original.Contains($fault.Before)){throw "Missing Kerfus fault anchor: $($fault.Name)"}
  $path=Join-Path $output ($fault.Name+'.cpp')
  [IO.File]::WriteAllText($path,$original.Replace($fault.Before,$fault.After))
  $header=Join-Path $output 'fault_source.h'
  $define=if($fault.Native){'KERFUS_NATIVE_SOURCE'}else{'CLAIM_KERFUS_SOURCE'}
  [IO.File]::WriteAllText($header,('#define '+$define+' "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  $fixture=if($fault.Native){'kerfus_native'}else{'kerfus_state'}
  $libraries=@();if(-not $fault.Native){$libraries=@("$output/config.obj")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/FI$header" "/Fo$output/fault.obj" "/Fe$output/fault.exe" "$PSScriptRoot/${fixture}_test.cpp" @libraries /link /OPT:REF
  if($LASTEXITCODE){throw "Kerfus fault failed to compile: $($fault.Name)"}
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/fault.exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($fault.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Kerfus fault did not reach an assertion: $($fault.Name)"}
  Write-Output "PASS Kerfus fault $($fault.Name)"
 }
}
$global:LASTEXITCODE=0
