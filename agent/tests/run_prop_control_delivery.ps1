param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-control-delivery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $driver=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_drive_host.cpp'))
 function Extract([string]$signature){
  $start=$driver.IndexOf($signature);if($start -lt 0){throw "Missing driver function: $signature"}
  $cursor=$driver.IndexOf('(',$start)+1;$paren=1
  while($paren -and $cursor -lt $driver.Length){if($driver[$cursor] -eq '('){$paren++};if($driver[$cursor] -eq ')'){$paren--};$cursor++}
  $open=$driver.IndexOf('{',$cursor);$end=$open+1;$depth=1
  while($depth -and $end -lt $driver.Length){if($driver[$end] -eq '{'){$depth++};if($driver[$end] -eq '}'){$depth--};$end++}
  if($depth){throw "Unbalanced driver function: $signature"};$driver.Substring($start,$end-$start)
 }
 $supportStart=$driver.IndexOf('struct VerbProof {');$supportEnd=$driver.IndexOf('// A prop in a hand:', $supportStart)
 if($supportStart -lt 0 -or $supportEnd -lt 0){throw 'Missing actual verb admission support'}
 $support=$driver.Substring($supportStart,$supportEnd-$supportStart)
 $rows=(Extract 'Driven* Find(void* actor)')+"`n"+$support+(@('Driven* Open(','Driven* FindByEid(','uint64_t NewRowRevision()','uint8_t BumpGen(','Driven* OpenClientRow(','Driven* ElectClientRow(','void EraseRow(') | ForEach-Object {Extract $_}) -join "`n"
 [IO.File]::WriteAllText((Join-Path $output 'prop_syncer_host_rows.inc'),$rows)
 $includes=@('/I','agent/tests/prop_syncer_host_stubs','/I','src/votv-coop/include','/I','src/votv-coop/src/coop/props',"/I$output")
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
 if($LASTEXITCODE){throw 'Control delivery registry compile failed'}
 function Compile([string]$name,[string]$header=''){
  $forced=@();if($header){$forced=@("/FI$header")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes @forced "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/prop_control_delivery_test.cpp "$output/registry.obj" /link /OPT:REF
  if($LASTEXITCODE){throw "Control delivery fixture compile failed: $name"}
 }
 Compile 'test'
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Control delivery fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_syncer_host.cpp')).Replace("`r`n","`n")
  $cases=@(
   @{Name='world';Before='world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='session';Before='serial == coop::net::session_serial::Current() &&';After='true &&'},
   @{Name='stop';Before='stops == session->StopCount() &&';After='true &&'},
   @{Name='role';Before='session->role() == coop::net::Role::Host;';After='true;'},
   @{Name='recipient';Before='occupant == context.session->peerGenerationForSlot(slot) &&';After='true &&'},
   @{Name='departure';Before='departure == g_departures[slot] &&';After='true &&'},
   @{Name='ready';Before='if (!answer->recipient.context.session->IsSlotWorldReady(answer->recipient.slot)) return false;';After='/* send before world-ready */'},
   @{Name='actor';Before='Basic() && owner.ref.Get() && owner.cls.Get()';After='Basic() && owner.ref.Raw() && owner.cls.Get()'},
   @{Name='class';Before='Basic() && owner.ref.Get() && owner.cls.Get()';After='Basic() && owner.ref.Get() && owner.cls.Raw()'},
   @{Name='eid';Before='static_cast<uint32_t>(eid) != owner.eid';After='false'},
   @{Name='pace';Before='answer->attempted && Elapsed(now, answer->lastAttempt) < kAnswerRetryMs';After='false'},
   @{Name='clock';Before='return now >= before ? now - before : 0;';After='return now - before;'},
   @{Name='refusal';Before='if (sent) EraseAnswer(answer);';After='EraseAnswer(answer);'},
   @{Name='newer';Before='found->second == answer) g_answers.erase(found);';After='true) g_answers.erase(found);'},
   @{Name='episode';Before='row->episode == owner.episode &&';After='true &&'},
   @{Name='successor';Before='it->second->grant && it->second->owner.eid == eid';After='false'},
   @{Name='revoke_takeback';Before='it->second->grant && it->second->owner.eid == eid';After='it->second->owner.eid == eid'},
   @{Name='budget';Before='(std::min)(kAnswerBudget, answers.size())';After='answers.size()'},
   @{Name='death';Before='if (tombstone) return Basic();';After='/* dead actor read */'},
   @{Name='sequence';Before='a.seq     = seq;';After='a.seq     = 0;'}
  )
  foreach($case in $cases){
   if(-not $source.Contains($case.Before)){throw "Missing control delivery mutation: $($case.Name)"}
   $changed=$source.Replace($case.Before,$case.After)
   $path=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($path,$changed)
   $header=Join-Path $output ($case.Name+'.h');[IO.File]::WriteAllText($header,('#define PROP_SYNCER_HOST_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   Compile $case.Name $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Control delivery mutation did not reach an assertion: $($case.Name)"}
   Write-Output "PASS control delivery fault $($case.Name)"
  }
 }
 if($CompileSources){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/prop_syncer_host.obj" src/votv-coop/src/coop/props/prop_syncer_host.cpp
  if($LASTEXITCODE){throw 'Control delivery production compile failed'}
 }
}finally{Pop-Location}
$global:LASTEXITCODE=0
Write-Output 'PASS grant/revoke delivery validation'
