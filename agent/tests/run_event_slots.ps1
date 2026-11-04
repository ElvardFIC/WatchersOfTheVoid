param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/event-slot-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/event_slot_stubs /I src/votv-coop/include "/Fo$output/test.obj" "/Fe$output/test.exe" agent/tests/event_slot_canonical_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Event slot fixture compile failed'}
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Event slot fixture failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/event_slot_stubs /I src/votv-coop/include "/Fo$output/claims.obj" "/Fe$output/claims.exe" agent/tests/event_slot_claim_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Event slot claim fixture compile failed'}
 & "$output/claims.exe"
 if($LASTEXITCODE){throw 'Event slot claim fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/interactables/floppy_slot_sync.cpp'))
  $controls=@(
   @{Name='sequence';Before='if(order.known && int32_t(sequence-order.sequence)<=0)return false;';After='/* older canonical admitted */'},
   @{Name='retry';Before='if(!ApplySlot(kind,index,devices[index],item.value,current,retryWrite) || !current())continue;';After='ApplySlot(kind,index,devices[index],item.value,current,retryWrite);if(!current())continue;'},
   @{Name='presentation';Before='const bool retryWrite=g_pendingWriteDebt.count(key)!=0;';After='const bool retryWrite=false;'},
   @{Name='revision';Before='row->second==item.revision';After='true'},
   @{Name='claim';Before='if(!host && g_pendingCanonical.count(ShadowKey(kind,i)))continue;';After='/* stale local claim admitted */'},
   @{Name='reentry';Before='if(g_pendingDraining)return;';After='/* nested canonical writes admitted */'},
   @{Name='recipient';Before='if (!recipients[slot]) continue;';After='if (!recipients[slot] || slot == 1) continue;'},
   @{Name='refusal';Before='whole = whole && sent;';After='whole = true;'},
   @{Name='snapshot';Before='g_shadow.insert_or_assign(key, digest);';After='PrimeShadow(kind, index, device);'},
   @{Name='world';Before='world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='join-debt';Before='else g_connectRetry.at(peerSlot).next = NowMs() + kConnectRetryMs;';After='else g_connectRetry.erase(peerSlot);'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing event slot control: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $header=Join-Path $output ($control.Name+'.h')
   [IO.File]::WriteAllText($header,('#define EVENT_SLOT_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I agent/tests/event_slot_stubs /I src/votv-coop/include "/Fo$output/$($control.Name).obj" "/Fe$output/$($control.Name).exe" agent/tests/event_slot_canonical_test.cpp /link /OPT:REF
   if($LASTEXITCODE){throw "Event slot control compile failed: $($control.Name)"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($control.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Event slot control did not reach assertion: $($control.Name)"}
   Write-Output "PASS event slot fault $($control.Name)"
  }
  $claimControls=@(
   @{Name='claim-world';Before='world != ue_wrap::world_identity::Generation() ||';After='false ||'},
   @{Name='claim-session';Before='serial != coop::net::session_serial::Current() || !session->connected() ||';After='!session->connected() ||'},
   @{Name='claim-actor';Before='return peer.Current() && target.Get() && cls.Get() && R::ClassOf(target.Get()) == cls.Get();';After='return peer.Current() && cls.Get() && R::ClassOf(target.Raw()) == cls.Get();'},
   @{Name='claim-class';Before='return peer.Current() && target.Get() && cls.Get() && R::ClassOf(target.Get()) == cls.Get();';After='return peer.Current() && target.Get();'},
   @{Name='claim-peer';Before='occupant != session->peerGenerationForSlot(slot)';After='false'},
   @{Name='claim-departure';Before='role != session->role() || departure != g_peerEpoch[slot] ||';After='role != session->role() ||'},
   @{Name='claim-recursion';Before='if (g_clientClaiming) return;';After='if (g_clientClaiming) { g_retry.erase(key); return; }'}
  )
  foreach($control in $claimControls){
   if(-not $source.Contains($control.Before)){throw "Missing claim control: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $header=Join-Path $output ($control.Name+'.h')
   [IO.File]::WriteAllText($header,('#define EVENT_SLOT_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I agent/tests/event_slot_stubs /I src/votv-coop/include "/Fo$output/$($control.Name).obj" "/Fe$output/$($control.Name).exe" agent/tests/event_slot_claim_test.cpp /link /OPT:REF
   if($LASTEXITCODE){throw "Claim control compile failed: $($control.Name)"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($control.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Claim control did not reach assertion: $($control.Name)"}
   Write-Output "PASS claim fault $($control.Name)"
  }
 }
 if($CompileSources){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/floppy_slot_sync.obj" src/votv-coop/src/coop/interactables/floppy_slot_sync.cpp
  if($LASTEXITCODE){throw 'Event slot production compile failed'}
 }
}finally{Pop-Location}
