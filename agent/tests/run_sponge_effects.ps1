param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/sponge-effects-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$Name,[string]$Fixture,[bool]$Controller,[string]$SourcePath=''){
  $flags=@('/nologo','/std:c++20','/EHsc','/Gy','/utf-8')
  if($Controller){$flags+=@('/I','agent/tests/sponge_stubs','/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
  $flags+=@('/I','src/votv-coop/include')
  if($SourcePath){
   $macro=if($Controller){'SPONGE_EFFECT_SOURCE'}else{'SPONGE_WRAPPER_SOURCE'}
   $header=Join-Path $output ($Name+'_source.h')
   [IO.File]::WriteAllText($header,('#define '+$macro+' "'+$SourcePath.Replace('\','/')+'"'+[Environment]::NewLine))
   $flags+="/FI$header"
  }
  $exe=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" "agent/tests/$Fixture.cpp" /link /OPT:REF | Out-Host
  if($LASTEXITCODE){throw "Sponge fixture compile failed: $Name"}
  return $exe
 }
 foreach($fixture in @('sponge_effects_sync_test','sponge_effects_lifetime_test','sponge_effects_wrapper_test','sponge_effects_trace_test')){
  $controller=$fixture -in @('sponge_effects_sync_test','sponge_effects_lifetime_test')
  $exe=Build-Fixture $fixture $fixture $controller
  & $exe
  if($LASTEXITCODE){throw "Sponge regression failed: $fixture"}
 }
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/sponge_effects_sync.cpp'))
  $controls=@(
   @{Name='session-serial';Case='session-scope';Before='g_serial==net::session_serial::Current()';After='true'},
   @{Name='exact-post';Case='invocation-post';Before='frame.invocation==call.invocation && ';After=''},
   @{Name='pre-reservation';Case='reserved-pre';Before='g_frames.push_back(frame);';After='/* no reservation */'},
   @{Name='frozen-key';Case='frozen-address';Before='key==frame.key';After='true'},
   @{Name='actor-class';Case='target-currency';Before='R::ClassOf(actor)==cls.Raw()';After='true'},
   @{Name='source-departure';Case='source-occupancy';Before='g_peerEpoch[p.slot]!=event.peerEpoch';After='false'},
   @{Name='source-connection';Case='source-occupancy';Before='Transport(p.slot)!=event.transport';After='false'},
   @{Name='recipient-connection';Case='recipient-occupancy';Before='event.connections[slot]==g_session->peerGenerationForSlot(slot)';After='true'},
   @{Name='capacity-seen';Case='overflow-admission';Before='if(sender || p.op!=1';After='if(g_events.size()>=128)g_lastHost=p.sequence; if(sender || p.op!=1'},
   @{Name='work-budgets';Case='budget-clock';Before='kAuthorizations=8,kPresentations=8';After='kAuthorizations=128,kPresentations=128'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing sponge control anchor: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $exe=Build-Fixture ('fault_'+$control.Name) 'sponge_effects_lifetime_test' $true $path
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($control.Case)+' line ')){throw "Sponge control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
  $wrapper=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/sponge_effects.cpp'))
  $before='if(assigned!=player)return false;'
  if(-not $wrapper.Contains($before)){throw 'Missing trace control anchor'}
  $path=Join-Path $output 'trace-player-link.cpp';[IO.File]::WriteAllText($path,$wrapper.Replace($before,'if(false)return false;'))
  $exe=Build-Fixture 'fault_trace_player_link' 'sponge_effects_trace_test' $false $path
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& $exe 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output 'trace-player-link.log'),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+: !FX::ReadUse'){throw 'Trace control did not reach its assertion'}
  Write-Host ('PASS fault trace-player-link: '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
 }
 if($CompileSources){foreach($source in @('ue_wrap/actors/sponge_effects','coop/props/sponge_effects_sync','coop/dispatch/event_dispatch_world','coop/session/subsystems')){
  $name=$source.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
  if($LASTEXITCODE){throw "Sponge production compile failed: $source"}
 }}
}finally{Pop-Location}
exit 0
