param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/prop-claim-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/prop_claim_extract.py" $output
if($LASTEXITCODE){throw 'Claim extraction/source-route checks failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/config.obj" "$root/src/votv-coop/src/coop/config/config_registry.cpp"
if($LASTEXITCODE){throw 'Claim config compile failed'}
function Compile([string]$name,[string]$header=''){
 $extra=@();if($header){$extra=@("/FI$header")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" @extra "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/prop_claim_test.cpp" "$output/config.obj" /link /OPT:REF
 if($LASTEXITCODE){throw "Claim fixture compile failed: $name"}
}
Compile 'test'
& "$output/test.exe"
if($LASTEXITCODE){throw 'Claim fixture failed'}
if($FaultControls){
 $sources=@{}
 foreach($entry in @(
  @('core','src/votv-coop/src/coop/props/prop_drive_host.cpp'),
  @('hook','src/votv-coop/src/coop/items/hook_prop_claim.cpp'),
  @('npc','src/votv-coop/src/coop/creatures/npc_pose_host.cpp'),
  @('kerfus','src/votv-coop/src/coop/creatures/kerfus_state.cpp')
 )){$sources[$entry[0]]=[IO.File]::ReadAllText((Join-Path $root $entry[1]))}
 $handoffPath=Join-Path $output 'claim_handoff.inc';$handoff=[IO.File]::ReadAllText($handoffPath)
 $watoyPath=Join-Path $output 'claim_watoy.inc';$watoy=[IO.File]::ReadAllText($watoyPath)
 $release=[regex]::Match($sources.core,'(?ms)^void Release\(void\* actor\) \{.*?^\}').Value
 try{
 foreach($case in @(
  @{Name='unique_token';Before='if (!++g_claimToken) ++g_claimToken; return g_claimToken;';After='return 1;'},
  @{Name='idempotent';Before='if (ClaimsCurrent(before) && before.claimed && !before.handoffFrom && (before.claimMask & bit) && before.claimTokens[index])';After='if (false)'},
  @{Name='add_owner';Before='next.claimMask |= bit;';After='next.claimMask = bit;'},
  @{Name='release_token';Before='!= lease.token';After='false'},
  @{Name='epoch';Before='lease.epoch == g_epoch &&';After='true &&'},
  @{Name='world';Before='lease.world == ue_wrap::world_identity::Generation() &&';After='true &&'},
  @{Name='serial';Before='lease.serial == coop::net::session_serial::Current() &&';After='true &&'},
  @{Name='world_pointer';Before='lease.worldObject == ue_wrap::world_identity::CurrentWorld();';After='true;'},
  @{Name='bound_stop';Before='g_stops == stops &&';After='true &&'},
  @{Name='bound_serial';Before='g_serial == serial &&';After='true &&'},
  @{Name='bound_world';Before='g_world == world &&';After='true &&'},
  @{Name='keep_other';Before='next.claimMask &= ~SourceBit(lease.source);';After='next.claimMask = 0;'},
  @{Name='last_release';Before='if (!proof.Matches(before) || !LeaseScope(lease)) return false;';After='if (!LeaseScope(lease)) return false;'},
  @{Name='caller';Before='VerbProof proof(actor, current);';After='VerbProof proof(actor);'},
  @{Name='hand_episode';Before='ClearClaims(next); next.claimed = false;';After='/* earlier contributors survive the hand */'},
  @{Name='legacy_isolation';Before=$release;After='void Release(void* actor) { if(auto* row=Find(actor)){row->claimMask=0;row->claimed=false;} }'},
  @{Name='handoff';Unit='handoff';Before='d.claimMask = 0; d.claimTokens.fill(0);';After='/* inherited client claims */'},
  @{Name='abandoned';Before='if (!d.syncer && d.claimed && !d.claimMask && d.handoffFrom) {';After='if (false) {'},
  @{Name='hook_epoch';Unit='hook';Before='epoch==g_claimEpoch&&';After='true&&'},
  @{Name='hook_release';Unit='hook';Before='(void)coop::prop_drive_host::ReleaseOwned(lease,current);';After='coop::prop_drive_host::Release(actor);'},
  @{Name='npc_other_claim';Unit='npc';Before='coop::prop_drive_host::HasClaims(hold->prop.ref.Raw())';After='false'},
  @{Name='kerfus_receipt';Unit='kerfus';Before='it->second.revision==r.revision';After='true'},
  @{Name='watoy_receipt';Unit='watoy';Before='it->second.state.sequence==previous.state.sequence';After='true'}
 )){
  $unit=if($case.Unit){$case.Unit}else{'core'}
  $original=if($unit -eq 'handoff'){$handoff}elseif($unit -eq 'watoy'){$watoy}else{$sources[$unit]}
  if(-not $original.Contains($case.Before)){throw "Missing claim fault anchor: $($case.Name)"}
  if($case.Name -eq 'release_token'){
   $changed=$original.Replace('row->claimTokens[static_cast<size_t>(lease.source)] != lease.token','false').Replace('found->claimTokens[static_cast<size_t>(lease.source)] != lease.token','false')
  }else{$changed=$original.Replace($case.Before,$case.After)}
  $header=''
  if($unit -eq 'handoff'){[IO.File]::WriteAllText($handoffPath,$changed)}
  elseif($unit -eq 'watoy'){[IO.File]::WriteAllText($watoyPath,$changed)}
  else{
   $path=Join-Path $output 'claim_fault.cpp';[IO.File]::WriteAllText($path,$changed)
   $define=@{core='PROP_PUBLISHER_SOURCE';hook='CLAIM_HOOK_SOURCE';npc='CLAIM_NPC_SOURCE';kerfus='CLAIM_KERFUS_SOURCE'}[$unit]
   $header=Join-Path $output 'fault_source.h'
   [IO.File]::WriteAllText($header,('#define '+$define+' "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  }
  Compile $case.Name $header
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Claim fault did not reach an assertion: $($case.Name)"}
  Write-Output "PASS claim fault $($case.Name)"
  [IO.File]::WriteAllText($handoffPath,$handoff);[IO.File]::WriteAllText($watoyPath,$watoy)
 }
 }finally{[IO.File]::WriteAllText($handoffPath,$handoff);[IO.File]::WriteAllText($watoyPath,$watoy)}
}
if($CompileSources){
 foreach($unit in @('coop/props/prop_drive_host','coop/props/prop_syncer_host','coop/creatures/npc_pose_host','coop/items/hook_prop_claim','coop/creatures/kerfus_state','coop/props/watoy_sync','coop/creatures/kerfur_command','coop/props/remote_prop','coop/world/salt_rocket_flight','coop/creatures/grayboar_sync','coop/props/prop_syncer_client')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "Claim production compile failed: $unit"}
 }
}
$global:LASTEXITCODE=0
