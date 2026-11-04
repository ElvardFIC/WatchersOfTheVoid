param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/tower-ops-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $includes=@('/I','agent/tests/tower_ops_stubs','/I','agent/tests/tower_stubs','/I','src/votv-coop/include')
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
 if($LASTEXITCODE){throw 'Tower operation registry compile failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/test.obj" "/Fe$output/test.exe" agent/tests/tower_ops_test.cpp "$output/registry.obj" /link /OPT:REF
 if($LASTEXITCODE){throw 'Tower operation fixture compile failed'}
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Tower operation fixture failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/capture.obj" "/Fe$output/capture.exe" agent/tests/tower_capture_test.cpp "$output/registry.obj" /link /OPT:REF
 if($LASTEXITCODE){throw 'Tower capture compile failed'}
 & "$output/capture.exe"
 if($LASTEXITCODE){throw 'Tower capture fixture failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/client_delivery.obj" "/Fe$output/client_delivery.exe" agent/tests/tower_client_delivery_test.cpp "$output/registry.obj" /link /OPT:REF
 if($LASTEXITCODE){throw 'Tower client delivery compile failed'}
 & "$output/client_delivery.exe"
 if($LASTEXITCODE){throw 'Tower client delivery fixture failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/refund.obj" "/Fe$output/refund.exe" agent/tests/tower_refund_test.cpp "$output/registry.obj" /link /OPT:REF
 if($LASTEXITCODE){throw 'Tower refund compile failed'}
 & "$output/refund.exe"
 if($LASTEXITCODE){throw 'Tower refund fixture failed'}
 & "$env:USERPROFILE/.local/bin/python3.15.exe" agent/tests/spawn_attempt_extract.py "$output/spawn_attempt.inc"
 if($LASTEXITCODE){throw 'Spawn attempt extraction failed'}
 & cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/Fo$output/spawn.obj" "/Fe$output/spawn.exe" agent/tests/spawn_attempt_test.cpp
 if($LASTEXITCODE){throw 'Spawn attempt compile failed'}
 & "$output/spawn.exe"
 if($LASTEXITCODE){throw 'Spawn attempt fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/world/fuse_tower_ops.cpp'))
  $controls=@(
   @{Name='world';Before='world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='session';Before='serial == coop::net::session_serial::Current() &&';After='true &&'},
   @{Name='sender';Before='occupant && occupant == session->peerGenerationForSlot(slot) &&';After='true &&'},
   @{Name='tower';Before='source.Current() && actor.Get() && cls.Get() && R::ClassOf(actor.Get()) == cls.Get()';After='source.Current() && cls.Get() && R::ClassOf(actor.Raw()) == cls.Get()'},
   @{Name='class';Before='source.Current() && actor.Get() && cls.Get() && R::ClassOf(actor.Get()) == cls.Get()';After='source.Current() && actor.Get()'},
   @{Name='detach';Before='g_waiting[slot].pop_front();';After='/* active receipt left in queue */'},
   @{Name='fifo';Before='g_waiting[slot].push_front(std::move(work));';After='g_waiting[slot].push_back(std::move(work));'},
   @{Name='rate';Before='g_rate[slot].Refund(kOpBudget);';After='/* rate debt lost */'},
   @{Name='budget';Before='size_t budget = kMaxQueued + 1;';After='size_t budget = kMaxQueued * coop::net::kMaxPeers;'},
   @{Name='continuation';Before='if (!current()) return Take::Cancelled;';After='/* old invocation continues */'},
   @{Name='reservation';Before='if (sp.ring[candidate].reserved) continue;';After='/* reserved spend overwritten */'},
   @{Name='capture_frame';Test='tower_capture';Before='return size >= 0 && size_t(offset) <= size_t(size) && bytes <= size_t(size) - size_t(offset);';After='return true;'},
   @{Name='capture_function';Test='tower_capture';Before='if (c.fn.Get() == fn && c.cls.Get() && R::ClassOf(fn) == c.cls.Get())';After='if(c.fn.Raw()==fn)'},
   @{Name='capture_invocation';Test='tower_capture';Before='invocation == call.invocation && stack == call.stack && depth == call.depth && locals == call.locals';After='true'},
   @{Name='capture_stops';Test='tower_capture';Before='stops == session->StopCount() && IsClient(session) && session->connected() &&';After='IsClient(session) && session->connected() &&'},
   @{Name='refund_attempt';Test='tower_refund';Before='if(debt->attempted ||';After='if(false ||'},
   @{Name='refund_ack';Test='tower_refund';Before='f.count && !RefundPending(uint8_t(i),f.At(0))';After='f.count'},
   @{Name='refund_output';Test='tower_refund';Before='if(fuse && R::IsLive(fuse)){';After='if(true){'},
   @{Name='move_commit';Test='tower_refund';Before='if(!committed){';After='if(false){'},
   @{Name='refund_ready';Test='tower_refund';Before='if (!Capture(s, slot).Current(false)) {';After='if (!Capture(s, slot).Current()) {'},
   @{Name='sequence_window';Test='tower_capture';Before='if(!ClientSequenceRoom())return;';After=''},
   @{Name='client_ack';Test='tower_client_delivery';Before='command->session->SendReliableToSlot(0,coop::net::ReliableKind::FuseTowerState,&packet,sizeof packet);';After='if(command->session->SendReliableToSlot(0,coop::net::ReliableKind::FuseTowerState,&packet,sizeof packet))g_clientCommands.erase(std::remove(g_clientCommands.begin(),g_clientCommands.end(),command),g_clientCommands.end());'},
   @{Name='client_fifo';Test='tower_client_delivery';Before='if(command->attempted && (now<command->at || now-command->at<500))return;';After='if(command->attempted && (now<command->at || now-command->at<500))continue;'},
   @{Name='claim_ack_completion';Test='tower_client_delivery';Before="if(!retired)g_unknownPulls.push_back(claim);`n    }";After="if(!retired)g_unknownPulls.push_back(claim);`n    }`n    g_claims.clear();"},
   @{Name='pull_output';Test='tower_client_delivery';Before='DestroyLocalProp(c.output.Raw(),';After='DestroyLocalProp(coop::players::Registry::Get().Local(),'},
   @{Name='claim_reservation';Test='tower_client_delivery';Before='g_claims.size()+g_unknownPulls.size()+g_clientCaptures.size()+g_postingClaims';After='g_claims.size()+g_unknownPulls.size()+g_clientCaptures.size()'},
   @{Name='paid_admission';Test='tower_client_delivery';Before='if(inserts)return true;';After='if(false)return true;'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing tower op control: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $header=Join-Path $output ($control.Name+'.h')
   [IO.File]::WriteAllText($header,('#define TOWER_OPS_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $fixture=if($control.Test){"agent/tests/$($control.Test)_test.cpp"}else{'agent/tests/tower_ops_test.cpp'}
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/FI$header" "/Fo$output/$($control.Name).obj" "/Fe$output/$($control.Name).exe" $fixture "$output/registry.obj" /link /OPT:REF
   if($LASTEXITCODE){throw "Tower op control compile failed: $($control.Name)"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($control.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Tower op control did not reach assertion: $($control.Name)"}
   Write-Output "PASS tower operation fault $($control.Name)"
  }
 }
 if($CompileSources){
  foreach($unit in @('fuse_tower_ops','fuse_tower_kind')){
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$unit.obj" "src/votv-coop/src/coop/world/$unit.cpp"
   if($LASTEXITCODE){throw "Tower operation production compile failed: $unit"}
  }
 }
}finally{Pop-Location}
$global:LASTEXITCODE=0
