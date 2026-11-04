param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/cord-continuation-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $includes=@('/I','agent/tests/cord_2170_stubs','/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs','/I','src/votv-coop/include')
 function Compile([string]$name,[string]$header=''){
  $forced=@();if($header){$forced=@("/FI$header")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 @includes @forced "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/cord_continuation_test.cpp /link /OPT:REF
  if($LASTEXITCODE){throw "Cord continuation compile failed: $name"}
 }
 Compile 'test'
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Cord continuation fixture failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 @includes "/Fo$output/delivery.obj" "/Fe$output/delivery.exe" agent/tests/cord_delivery_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Cord recipient delivery compile failed'}
 & "$output/delivery.exe"
 if($LASTEXITCODE){throw 'Cord recipient delivery fixture failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 @includes "/Fo$output/drag.obj" "/Fe$output/drag.exe" agent/tests/cord_drag_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Cord drag compile failed'}
 & "$output/drag.exe"
 if($LASTEXITCODE){throw 'Cord drag fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/interactables/cord_sync.cpp')).Replace("`r`n","`n")
  $cases=@(
   @{Name='session_pointer_reset';Before='if(g_bound==s && g_world==gen';After='if(true && g_world==gen'},
   @{Name='session_serial_reset';Before='g_world==gen && g_serial==serial &&';After='g_world==gen && true &&'},
   @{Name='stop_reset';Before='g_stops==s->StopCount() && g_role==s->role() && s->connected()';After='true && g_role==s->role() && s->connected()'},
   @{Name='role_reset';Before='g_stops==s->StopCount() && g_role==s->role() && s->connected()';After='g_stops==s->StopCount() && true && s->connected()'},
   @{Name='world_reset';Before='g_bound==s && g_world==gen &&';After='g_bound==s && true &&'},
   @{Name='disconnected_reset';Before='g_role==s->role() && s->connected()';After='g_role==s->role() && true'},
   @{Name='same_world_index';Before='if(!sameWorld){g_actors.clear();g_scratch.clear();}';After='g_actors.clear();g_scratch.clear();'},
   @{Name='reconnect_index';Before='g_session.store(nullptr); g_sent.clear();';After='g_session.store(nullptr);g_actors.clear();g_scratch.clear();g_world=0;g_sent.clear();'},
   @{Name='stop_continuation';Before='stopped==session->StopCount() &&';After='true &&'},
   @{Name='role_continuation';Before='role==session->role();';After='true;'},
   @{Name='peer_generation';Before='peer.generation!=context.session->peerGenerationForSlot(slot) ||';After='false ||'},
   @{Name='peer_readiness';Before='(ready && !context.session->IsSlotWorldReady(slot))';After='false'},
   @{Name='peer_departure';Before='peer.departure!=g_departures[slot] ||';After='false ||';Before2='departure!=g_departures[target] ||';After2='false ||'},
   @{Name='unbound_host';Before='if(target>=0 || context.role==net::Role::Client)valid=false;';After='if(target>=0)valid=false;'},
   @{Name='actor_serial';Before='if(!actor.Get()||!cls.Get()';After='if(!actor.Raw()||!cls.Get()'},
   @{Name='class_serial';Before='if(!actor.Get()||!cls.Get()';After='if(!actor.Get()||!cls.Raw()'},
   @{Name='parent_lifetime';Before='return !hasParent || (parent.Get()&&parentCls.Get()&&R::ClassOf(parent.Raw())==parentCls.Raw());';After='return true;'},
   @{Name='publication_revision';Before='last->revision!=operation ||';After='false ||'},
   @{Name='outbox_completion';Before='const auto found=std::find(g_outbox.begin(),g_outbox.end(),row);';After='const auto found=std::find_if(g_outbox.begin(),g_outbox.end(),[&](const auto& pending){return W::Same(pending->p.cord,row->p.cord);});'},
   @{Name='outbox_refusal';Before='const bool keep=!sent&&CommandCurrent(row);';After='const bool keep=false;'},
   @{Name='release_completion';Before='const auto found=std::find(g_releases.begin(),g_releases.end(),row);';After='const auto found=g_releases.begin();'},
   @{Name='release_refusal';Before='if((sent||!valid||';After='if((true||!valid||'},
   @{Name='intent_copy';Before='const auto p=input;';After='const auto& p=input;'},
   @{Name='clock';Before='return now>=before?now-before:0;';After='return now-before;'},
   @{Name='repaint_context';Before='return context.Current() && epoch==g_epoch && world==ue_wrap::world_identity::Generation() && Client();';After='return epoch==g_epoch && world==ue_wrap::world_identity::Generation() && Client();'}
  )
  foreach($case in $cases){
   if(-not $source.Contains($case.Before)){throw "Missing cord mutation anchor: $($case.Name)"}
   $mutated=$source.Replace($case.Before,$case.After)
   if($case.Before2){if(-not $mutated.Contains($case.Before2)){throw "Missing second cord mutation anchor: $($case.Name)"};$mutated=$mutated.Replace($case.Before2,$case.After2)}
   $path=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($path,$mutated)
   $header=Join-Path $output ($case.Name+'.h');[IO.File]::WriteAllText($header,('#define CORD_SYNC_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   Compile $case.Name $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Cord mutation did not reach an assertion: $($case.Name)"}
   Write-Output "PASS cord continuation fault $($case.Name)"
  }
 }
 if($CompileSources){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/cord_sync.obj" src/votv-coop/src/coop/interactables/cord_sync.cpp
  if($LASTEXITCODE){throw 'Cord production compile failed'}
 }
}finally{Pop-Location}
$global:LASTEXITCODE=0
Write-Output 'PASS cord continuation validation'
