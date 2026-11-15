param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-syncer-host-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $driver=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_drive_host.cpp'))
 function Extract([string]$signature){
  $start=$driver.IndexOf($signature);if($start -lt 0){throw "Missing driver function: $signature"}
  $params=$driver.IndexOf('(',$start);$cursor=$params+1;$paren=1
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
 $includes=@('/I',(Join-Path $PSScriptRoot 'prop_syncer_host_stubs'),'/I','src/votv-coop/include','/I','src/votv-coop/src/coop/props',"/I$output")
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
 if($LASTEXITCODE){throw 'Host syncer registry compile failed'}
 function Compile([string]$name,[string]$header=''){
  $forced=@();if($header){$forced=@("/FI$header")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes @forced "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/prop_syncer_host_test.cpp" "$output/registry.obj" /link /OPT:REF
  if($LASTEXITCODE){throw "Host syncer compile failed: $name"}
 }
 Compile 'test'
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Host syncer fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_syncer_host.cpp'))
  $cases=@(
   @{Name='world';Before='world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='session';Before='serial == coop::net::session_serial::Current() &&';After='true &&'},
   @{Name='sender';Before='occupant == context.session->peerGenerationForSlot(slot) &&';After='true &&'},
   @{Name='revision';Before='row->revision == value.revision &&';After='true &&'},
   @{Name='class';Before='R::ClassOf(actor) == row.cls.Get() &&';After='true &&'},
   @{Name='drain_recursion';Before='if (g_draining || !context.Current()) return;';After='if (!context.Current()) return;'},
   @{Name='claim_copy';Before='const auto p = input;';After='const auto& p = input;'},
   @{Name='row_copy';Before='const auto e = input;';After='const auto& e = input;'},
   @{Name='row_finite';Before='for (float value : {e.x,e.y,e.z,e.pitch,e.yaw,e.roll}) if (!std::isfinite(value)) return;';After='/* unchecked pose */'},
   @{Name='clock';Before='return now >= before ? now - before : 0;';After='return now - before;'},
   @{Name='grant_continuation';Before='if (!receipt.Current()) return;'+"`n"+'    Answer(s, slot, p.eid';After='Answer(s, slot, p.eid'},
   @{Name='feed_completion';Before='if (!current() || !Save(receipt, owned)) return;';After='Save(receipt, owned);'},
   @{Name='retirement';Before='!coop::prop_drive_host::FindByEid(receipt.value.eid) &&';After='true &&'},
   @{Name='end_continuation';Before='if (!retiring.Current()) return;';After='/* cancelled completion continues */'},
   @{Name='election_copy';Before='const std::array<float, 3> release{releaseLoc[0], releaseLoc[1], releaseLoc[2]};';After='const float* release = releaseLoc;'}
  )
  foreach($case in $cases){
   $original=$source.Replace("`r`n","`n");if(-not $original.Contains($case.Before)){throw "Missing host fault anchor: $($case.Name)"}
   $changed=$original.Replace($case.Before,$case.After)
   # Grant transport repeats the caller's episode proof; this control removes both gates.
   if($case.Name -eq 'grant_continuation'){$changed=$changed.Replace('if (!grant) return true;','return true;')}
   if($case.Name -eq 'election_copy'){$changed=$changed.Replace('for (float value : release)','for (float value : std::array<float,3>{release[0],release[1],release[2]})')}
   $path=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($path,$changed)
   $header=Join-Path $output ($case.Name+'.h');[IO.File]::WriteAllText($header,('#define PROP_SYNCER_HOST_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   Compile $case.Name $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Host syncer fault did not reach assertion: $($case.Name)"}
   Write-Output "PASS host syncer fault $($case.Name)"
  }
 }
 if($CompileSources){foreach($unit in @('prop_syncer_host','prop_drive_host')){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$unit.obj" "src/votv-coop/src/coop/props/$unit.cpp"
  if($LASTEXITCODE){throw "Host syncer production compile failed: $unit"}
 }}
}finally{Pop-Location}
$global:LASTEXITCODE=0
Write-Output 'PASS host syncer validation'
