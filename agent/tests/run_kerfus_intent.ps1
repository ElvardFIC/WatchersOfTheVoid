param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/kerfus-intent-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/prop_claim_extract.py" $output
if($LASTEXITCODE){throw 'Kerfus command dependency extraction failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/config.obj" "$root/src/votv-coop/src/coop/config/config_registry.cpp"
if($LASTEXITCODE){throw 'Kerfus command config compile failed'}
foreach($name in @('kerfus_intent','kerfus_action_native')){
 $libraries=@();if($name -eq 'kerfus_intent'){$libraries=@("$output/config.obj")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" @libraries /link /OPT:REF
 if($LASTEXITCODE){throw "Kerfus command fixture compile failed: $name"}
 & "$output/$name.exe"
 if($LASTEXITCODE){throw "Kerfus command fixture failed: $name"}
}
if($CompileSources){
 foreach($unit in @('coop/creatures/kerfus_intent','ue_wrap/actors/kerfus')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "Kerfus command production compile failed: $unit"}
 }
}
if($FaultControls){
 $manager=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/kerfus_intent.cpp'))
 $native=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/kerfus.cpp'))
 foreach($fault in @(
  @{Name='epoch';Before='epoch==g_epoch&&';After='true&&'},
  @{Name='serial';Before='serial==N::session_serial::Current()&&';After='true&&'},
  @{Name='sender_generation';Before='s->peerGenerationForSlot(row->slot)==row->generation&&';After='true&&'},
  @{Name='sender_ready_revision';Before='s->SlotWorldReadyRevision(row->slot)==row->ready&&';After='true&&'},
  @{Name='reserved';Before='&&!p._pad[0]&&!p._pad[1]&&!p._pad[2]';After=''},
  @{Name='incarnation';Before='now.incarnation==identity.incarnation&&';After='true&&'},
  @{Name='body_wait';Before='if(subject.outcome==EL::IntentOutcome::NoBody)return N::Served::Wait;';After='if(subject.outcome==EL::IntentOutcome::NoBody)return N::Served::Done;'},
  @{Name='budget';Before='kBudget{10.f,10.f}';After='kBudget{11.f,10.f}'},
  @{Name='queue_capacity';Before='kCapacity=16';After='kCapacity=32'},
  @{Name='transport_refusal';Before='if(!installed()||!accepted)return;';After='if(!installed())return;'},
  @{Name='retry_pacing';Before='now-command->at<kRetryMs';After='false'},
  @{Name='frame_extent';Before='!Span(offset,width,size)';After='false'},
  @{Name='parameter_type';Before='(type==expected||(type==L"EnumProperty"&&std::wstring(expected)==L"ByteProperty"))';After='true'},
  @{Name='string_terminator';Before='||chars[header.num-1]!=0';After=''},
  @{Name='no_world_refusal';Before='s&&s->running()&&s->role()==N::Role::Client?sg::Verdict::Cancel:sg::Verdict::Run';After='sg::Verdict::Run'},
  @{Name='dispatched_wait';Before='if(!command->dispatched)return N::Served::Wait;';After='if(!command->dispatched)return N::Served::Done;'},
  @{Name='presser';Before='slot=uint8_t(running->owner->slot);';After='slot=0;'},
  @{Name='scope_copy';Before='if(g_bound){const auto scope=*g_bound;Register(scope);}';After='if(g_bound)Register(*g_bound);'},
  @{Name='player_type';Native=$true;Before='playerType!=L"ObjectProperty"';After='false'},
  @{Name='action_overlap';Native=$true;Before='(actionOff>=playerOff&&actionOff<playerOff+sizeof(void*))';After='false'},
  @{Name='player_width';Native=$true;Before='playerSize!=sizeof(void*)';After='false'},
  @{Name='action_type';Native=$true;Before='(actionType!=L"ByteProperty"&&actionType!=L"EnumProperty")';After='false'},
  @{Name='function_checkpoint';Native=$true;Before='const int32_t size=R::FunctionFrameSize(function.Raw());if(!owned()||size<=0';After='const int32_t size=R::FunctionFrameSize(function.Raw());if(size<=0'},
  @{Name='native_receipt';Native=$true;Before='if(dispatched)*dispatched=true;';After='/* missing receipt */'},
  @{Name='player_lifetime';Native=$true;Before='(player&&!who.Get())';After='false';ExtraBefore='(!player||who.Get())';ExtraAfter='true'}
 )){
  $original=if($fault.Native){$native}else{$manager}
  if(-not $original.Contains($fault.Before)){throw "Missing command fault anchor: $($fault.Name)"}
  $changed=$original.Replace($fault.Before,$fault.After)
  if($fault.ExtraBefore){$changed=$changed.Replace($fault.ExtraBefore,$fault.ExtraAfter)}
  $path=Join-Path $output ($fault.Name+'.cpp');[IO.File]::WriteAllText($path,$changed)
  $header=Join-Path $output 'fault_source.h';$define=if($fault.Native){'KERFUS_NATIVE_SOURCE'}else{'KERFUS_INTENT_SOURCE'}
  [IO.File]::WriteAllText($header,('#define '+$define+' "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  $fixture=if($fault.Native){'kerfus_action_native'}else{'kerfus_intent'}
  $libraries=@();if(-not $fault.Native){$libraries=@("$output/config.obj")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/FI$header" "/Fo$output/fault.obj" "/Fe$output/fault.exe" "$PSScriptRoot/${fixture}_test.cpp" @libraries /link /OPT:REF
  if($LASTEXITCODE){throw "Command fault compile failed: $($fault.Name)"}
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/fault.exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($fault.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Command fault did not reach an assertion: $($fault.Name)"}
  Write-Output "PASS command fault $($fault.Name)"
 }
}
$global:LASTEXITCODE=0
