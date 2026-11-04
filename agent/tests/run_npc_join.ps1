param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/npc-join-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/npc_join_extract.py" $output
if($LASTEXITCODE){throw 'NPC join extraction failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/npc_join_stubs" "/I$root/src/votv-coop/include" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/npc_join_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'NPC join compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'NPC join fixture failed'}
if($FaultControls){
 $join=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/npc_join_delivery.cpp'))
 $keyPath=Join-Path $output 'npc_save_key.inc';$key=[IO.File]::ReadAllText($keyPath)
 $element=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/element/element.cpp'))
 $registry=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/element/registry.cpp'))
 $completePath=Join-Path $output 'npc_snapshot_complete.inc';$complete=[IO.File]::ReadAllText($completePath)
 try {
  foreach($case in @(
   @{Name='join_receipt';Body=$join;Before='g_joins[join->context.slot]==join&&';After='true&&'},
   @{Name='session_pointer';Body=$join;Before='session==coop::npc_sync::GetSession()&&';After='true&&'},
   @{Name='session_serial';Body=$join;Before='serial==N::session_serial::Current()&&';After='true&&'},
   @{Name='world';Body=$join;Before='world==ue_wrap::world_identity::Generation()&&';After='true&&'},
   @{Name='stop';Body=$join;Before='session->StopCount()==stops&&';After='true&&'},
   @{Name='role';Body=$join;Before='session->role()==N::Role::Host&&';After='true&&'},
   @{Name='recipient';Body=$join;Before='session->peerGenerationForSlot(slot)==generation&&';After='true&&'},
   @{Name='ready_revision';Body=$join;Before='session->SlotWorldReadyRevision(slot)==ready&&';After='true&&'},
   @{Name='actor';Body=$join;Before='!row.actor.Get()';After='!row.actor.Raw()'},
   @{Name='class';Body=$join;Before='!row.cls.Get()';After='!row.cls.Raw()'},
   @{Name='incarnation';Body=$join;Before='now.incarnation==row.identity.incarnation&&';After='true&&'},
   @{Name='actor_reverse';Body=$join;Before='return E::Registry::Get().EidForActor(now.actor)==now.id;';After='return true;'},
   @{Name='refusal';Body=$join;Before='if(!accepted)return;';After='/* refused receipt consumed */'},
   @{Name='pace';Body=$join;Before='return attempted&&now>=at&&now-at<kRetryMs;';After='return false;'},
   @{Name='budget';Body=$join;Before='kNativeBudget=16';After='kNativeBudget=1000'},
   @{Name='scan_budget';Body=$join;Before='kScanBudget=64';After='kScanBudget=10000'},
   @{Name='fairness';Body=$join;Before='g_cursor=slot+1==N::kMaxPeers?1:slot+1;';After='g_cursor=1;'},
   @{Name='recursion';Body=$join;Before='if(g_draining)return;';After='/* recursive drain admitted */'},
   @{Name='completion_wait';Body=$join;Before='if(HasPending(slot))continue;';After='/* NPC fence bypassed */'},
   @{Name='completion_token';Body=$join;Before='g_snapshots[slot]->token==token&&';After='true&&'},
   @{Name='completion_receipt';Body=$join;Before='g_completions[slot]==completion&&';After='true&&'},
   @{Name='completion_validation';Body=$join;Before='if(!coherent)';After='if(false)'},
   @{Name='completion_refusal';Body=$join;Before='||!accepted)continue;';After='||false)continue;'},
   @{Name='key_cache';Body=$key;Before='if(!memo.Is(cls.Raw()))';After='if(memo.Raw()!=cls.Raw())'},
   @{Name='key_unreadable';Body=$key;Before='if(!current()||text.empty())return false;';After='if(!current())return false;'},
   @{Name='key_layout';Body=$key;Before='int32_t offset=-1;std::wstring type;bool found=false;';After='static int32_t offset=-1;std::wstring type;bool found=false;'},
   @{Name='completion_scope';Body=$complete;Before='CloseBracket_(slot);';After='CloseBracket_(g_currentTargetSlot);'},
   @{Name='element_identity';Body=$element;Before='m_incarnation = g_nextIncarnation.fetch_add(1, std::memory_order_relaxed);';After='m_incarnation = 1;'},
   @{Name='element_rebind';Body=$element;Before='if (m_id != kInvalidId &&';After='if (m_id != kInvalidId && old != a &&'},
   @{Name='registry_rebind';Body=$registry;Before='const ActorIdent ai = IdentOf(newActor, newIdx);';After='if(oldActor==newActor)return; const ActorIdent ai = IdentOf(newActor, newIdx);'}
  )) {
   if(-not $case.Body.Contains($case.Before)){throw "Missing NPC join fault anchor: $($case.Name)"}
   $mutated=$case.Body.Replace($case.Before,$case.After)
   $extra=@()
   if($case.Name.StartsWith('key_')){[IO.File]::WriteAllText($keyPath,$mutated)}
   elseif($case.Name -eq 'completion_scope'){[IO.File]::WriteAllText($completePath,$mutated)}
   elseif($case.Name -eq 'registry_rebind'){
    $path=Join-Path $output 'registry_fault.cpp';[IO.File]::WriteAllText($path,$mutated)
    $header=Join-Path $output 'fault_source.h'
    [IO.File]::WriteAllText($header,('#define REGISTRY_SOURCE "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
    $extra=@("/FI$header")
   }
   elseif($case.Name.StartsWith('element_')){
    $path=Join-Path $output 'element_fault.cpp';[IO.File]::WriteAllText($path,$mutated)
    $header=Join-Path $output 'fault_source.h'
    [IO.File]::WriteAllText($header,('#define ELEMENT_SOURCE "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
    $extra=@("/FI$header")
   }else{
    $path=Join-Path $output 'join_fault.cpp';[IO.File]::WriteAllText($path,$mutated)
    $header=Join-Path $output 'fault_source.h'
    [IO.File]::WriteAllText($header,('#define NPC_JOIN_SOURCE "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
    $extra=@("/FI$header")
   }
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/npc_join_stubs" "/I$root/src/votv-coop/include" @extra "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" "$PSScriptRoot/npc_join_test.cpp" /link /OPT:REF
   if($LASTEXITCODE){throw "NPC join fault compile failed: $($case.Name)"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "NPC join mutation did not reach an assertion: $($case.Name)"}
   Write-Output "PASS NPC join fault $($case.Name)"
   [IO.File]::WriteAllText($keyPath,$key)
   [IO.File]::WriteAllText($completePath,$complete)
  }
 }finally{[IO.File]::WriteAllText($keyPath,$key);[IO.File]::WriteAllText($completePath,$complete)}
}
if($CompileSources){
 foreach($unit in @('coop/creatures/npc_join_delivery','coop/creatures/npc_pose_host','coop/creatures/npc_sync','coop/props/prop_snapshot','coop/session/connect_replay','coop/element/element','coop/element/registry','ue_wrap/actors/kerfur')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "NPC join production compile failed: $unit"}
 }
}
$global:LASTEXITCODE=0
