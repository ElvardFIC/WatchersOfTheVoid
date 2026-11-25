param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-end-delivery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $includes=@('/I',(Join-Path $PSScriptRoot 'prop_syncer_host_stubs'),'/I',(Join-Path $PSScriptRoot 'prop_publisher_stubs'),'/I','src/votv-coop/include','/I','src/votv-coop/src/coop/props')
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
 if($LASTEXITCODE){throw 'End delivery registry compile failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c @includes "/Fo$output/shared.obj" "$PSScriptRoot/prop_end_delivery_link_test.cpp"
 if($LASTEXITCODE){throw 'End delivery shared-state compile failed'}
 function Compile([string]$name,[string]$header=''){
  $forced=@();if($header){$forced=@("/FI$header")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes @forced "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/prop_end_delivery_test.cpp" "$output/registry.obj" "$output/shared.obj" /link /OPT:REF
  if($LASTEXITCODE){throw "End delivery fixture compile failed: $name"}
 }
 Compile 'test'
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'End delivery fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_end_delivery.h')).Replace("`r`n","`n")
  $cases=@(
   @{Name='world';Before='g_world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='session';Before='g_serial == coop::net::session_serial::Current() &&';After='true &&'},
   @{Name='stops';Before='g_stops == g_session->StopCount() &&';After='true &&'},
   @{Name='role';Before='g_session->role() == coop::net::Role::Host;';After='true;'},
   @{Name='connection';Before='g_session->connected() &&';After='true &&'},
   @{Name='session_pointer';Before='g_session != &s ||';After='false ||'},
   @{Name='recipient';Before='recipient.generation == g_session->peerGenerationForSlot(slot) &&';After='true &&'},
   @{Name='departure';Before='recipient.departure == g_departures[slot] &&';After='true &&'},
   @{Name='ready';Before='if (!g_session->IsSlotWorldReady(slot)) return false;';After='/* ignored readiness */'},
   @{Name='actor';Before='batch->actor.Get() &&';After='batch->actor.Raw() &&'},
   @{Name='class';Before='batch->cls.Get();';After='batch->cls.Raw();'},
   @{Name='class_binding';Before='cls != batch->cls.Raw()';After='false';ExtraBefore='cls == batch->cls.Raw()';ExtraAfter='true'},
   @{Name='eid';Before='static_cast<uint32_t>(eid) != batch->payload.eid';After='false'},
   @{Name='pace';Before='recipient.attempted && elapsed < kRetryMs';After='false'},
   @{Name='clock';Before='now >= recipient.lastAttempt ? now - recipient.lastAttempt : 0';After='now - recipient.lastAttempt'},
   @{Name='refusal';Before='if (!RecipientCurrent(batch, slot) || sent)';After='if (true)'},
   @{Name='identity';Before='latest->second == batch->identity &&';After='true &&'},
   @{Name='completion';Before='found->second == batch) g_pending.erase(found);';After='true) g_pending.erase(found);'},
   @{Name='successor';Before='g_latest[eid] = ++g_identity;';After='/* old proof kept */'},
   @{Name='cancel';Before='batch->cancelled = true;';After='/* cancellation forgot its held snapshots */'},
   @{Name='budget';Before='coop::net::kMaxPeers - 1 && budget;';After='coop::net::kMaxPeers - 1;'},
   @{Name='recursion';Before='if (g_draining || !Bind(s) || g_pending.empty()) return;';After='if (!Bind(s) || g_pending.empty()) return;'},
   @{Name='finite';Before='!std::isfinite(value)';After='false'},
   @{Name='bounds';Before='!coop::net::PropDriveEndInBounds(p)';After='false'},
   @{Name='generation';Before='!p.gen ||';After='false ||'},
   @{Name='reserved';Before='if (p._pad[0] || p._pad[1] || (p.physFlags & 0x80)) return false;';After='/* reserved fields unchecked */'},
   @{Name='controller_reserved';Unit='controller';Before='if (p._pad[0] || p._pad[1] || (p.physFlags & 0x80)) return;';After='/* reserved fields unchecked */'}
  )
  foreach($case in $cases){
   $original=if($case.Unit -eq 'controller'){[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_syncer_host.cpp')).Replace("`r`n","`n")}else{$source}
   if(-not $original.Contains($case.Before)){throw "Missing End mutation anchor: $($case.Name)"}
   $changed=$original.Replace($case.Before,$case.After)
   if($case.ExtraBefore){if(-not $changed.Contains($case.ExtraBefore)){throw "Missing second End mutation anchor: $($case.Name)"};$changed=$changed.Replace($case.ExtraBefore,$case.ExtraAfter)}
   $path=Join-Path $output ($case.Name+'.h')
   if($case.Unit -eq 'controller'){
    $cpp=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($cpp,$changed)
    [IO.File]::WriteAllText($path,('#define PROP_END_CONTROLLER_SOURCE "'+$cpp.Replace('\','/')+'"'+[Environment]::NewLine))
   }else{[IO.File]::WriteAllText($path,$changed)}
   Compile $case.Name $path
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "End mutation did not reach an assertion: $($case.Name)"}
   Write-Output "PASS End delivery fault $($case.Name)"
  }
 }
 if($CompileSources){foreach($unit in @('prop_drive_host','prop_syncer_host')){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$unit.obj" "src/votv-coop/src/coop/props/$unit.cpp"
  if($LASTEXITCODE){throw "End delivery production compile failed: $unit"}
 }}
}finally{Pop-Location}
$global:LASTEXITCODE=0
Write-Output 'PASS publisher/controller End delivery validation'
