param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-stream-scope-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $driver=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_drive_host.cpp'))
 function Extract([string]$signature){
  $start=$driver.IndexOf($signature);if($start -lt 0){throw "Missing row function: $signature"}
  $cursor=$driver.IndexOf('(',$start)+1;$paren=1
  while($paren -and $cursor -lt $driver.Length){if($driver[$cursor] -eq '('){$paren++};if($driver[$cursor] -eq ')'){$paren--};$cursor++}
  $open=$driver.IndexOf('{',$cursor);$end=$open+1;$depth=1
  while($depth -and $end -lt $driver.Length){if($driver[$end] -eq '{'){$depth++};if($driver[$end] -eq '}'){$depth--};$end++}
  if($depth){throw "Unbalanced row function: $signature"};$driver.Substring($start,$end-$start)
 }
 $supportStart=$driver.IndexOf('struct VerbProof {');$supportEnd=$driver.IndexOf('// A prop in a hand:', $supportStart)
 if($supportStart -lt 0 -or $supportEnd -lt 0){throw 'Missing actual verb admission support'}
 $support=$driver.Substring($supportStart,$supportEnd-$supportStart)
 $rows=(Extract 'Driven* Find(void* actor)')+"`n"+$support+(@('Driven* Open(','Driven* FindByEid(','uint64_t NewRowRevision()','uint8_t BumpGen(','Driven* OpenClientRow(','Driven* ElectClientRow(','void EraseRow(') | ForEach-Object {Extract $_}) -join "`n"
 [IO.File]::WriteAllText((Join-Path $output 'stream_scope_rows.inc'),$rows)
 $includes=@('/I',(Join-Path $PSScriptRoot 'prop_stream_scope_stubs'),'/I',(Join-Path $PSScriptRoot 'prop_syncer_host_stubs'),'/I','src/votv-coop/include','/I','src/votv-coop/src/coop/props',"/I$output")
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
 if($LASTEXITCODE){throw 'Scope config registry compile failed'}
 function Compile([string]$name,[string]$header=''){
  $forced=@();if($header){$forced=@("/FI$header")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes @forced "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/prop_stream_scope_test.cpp" "$output/registry.obj" /link /OPT:REF
  if($LASTEXITCODE){throw "Stream scope compile failed: $name"}
 }
 Compile 'test'
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Stream scope fixture failed'}
 if($FaultControls){
  $sources=@{}
  foreach($unit in @('prop_drive_stream','prop_park','prop_wire_parity','prop_syncer_host')){$sources[$unit]=[IO.File]::ReadAllText((Join-Path $root "src/votv-coop/src/coop/props/$unit.cpp")).Replace("`r`n","`n")}
  $cases=@(
   @{Name='immediate_source';Before='return !source.current || source.current();';After='return true;'},
   @{Name='persistent_lifetime';Before='(!drive.source.lifetime || drive.source.lifetime())';After='true'},
   @{Name='target_actor';Before='source.actor.Get() == actor &&';After='true &&'},
   @{Name='target_class';Before='R::ClassOf(actor) == source.cls.Get()';After='true'},
   @{Name='feed_forward';Before='ApplyPose(e, AD::NowMs(), "HOST", owned);';After='ApplyPose(e, AD::NowMs(), "HOST");'},
   @{Name='end_forward';Before='ReceiveEnd(end, owned);';After='ReceiveEnd(end, {});'},
   @{Name='admission_bridge';Unit='prop_syncer_host';Before='coop::prop_drive_stream::FeedAdmitted(r, {receipt.context.session,'+"`n"+'        [receipt] { return receipt.Current(); }, [receipt] { return receipt.Alive(); },'+"`n"+'        receipt.value.ref, receipt.value.cls, receipt.value.episode});';After='coop::prop_drive_stream::FeedAdmitted(r);'},
   @{Name='release_bridge';Unit='prop_syncer_host';Before='coop::prop_drive_stream::ReleaseHost(p, {receipt.context.session,'+"`n"+'        retiring.Guard(), {}, receipt.value.ref, receipt.value.cls, receipt.value.episode});';After='coop::prop_drive_stream::ReleaseHost(p);'},
   @{Name='retirement_revival';Unit='prop_syncer_host';Before='g_retiring.erase(eid);';After='/* obsolete retirement left live */'},
   @{Name='payload_copy';Before='const auto e = input;';After='const auto& e = input;'},
   @{Name='source_copy';Before='const Source owned = source;';After='const Source& owned = source;'},
   @{Name='park_guard';Unit='prop_park';Function='Park';Before='return !source || source();';After='return true;'},
   @{Name='unpark_guard';Unit='prop_park';Function='Unpark';Before='return !source || source();';After='return true;'},
   @{Name='parity_guard';Unit='prop_wire_parity';Before='return !current || current();';After='return true;'},
   @{Name='metadata';Before='coop::prop_child_release::OnParentReleased(actor);'+"`n"+'    if (!current()) return;';After='coop::prop_child_release::OnParentReleased(actor);'},
   @{Name='linear_result';Before='if (!ue_wrap::engine::SetComponentLinearVelocity(mesh, p.linVelX, p.linVelY, p.linVelZ)) return;';After='ue_wrap::engine::SetComponentLinearVelocity(mesh, p.linVelX, p.linVelY, p.linVelZ);'},
   @{Name='angular_result';Before='if (!ue_wrap::engine::SetComponentAngularVelocity(mesh, p.angVelX, p.angVelY, p.angVelZ)) return;';After='ue_wrap::engine::SetComponentAngularVelocity(mesh, p.angVelX, p.angVelY, p.angVelZ);'},
   @{Name='scoped_defer';Before='} else if (sourceGuard.current) {'+"`n"+'            return; // a host retirement receipt cannot become a deferred client replay'+"`n"+'        } else if';After='} else if'}
  )
  foreach($case in $cases){
   $unit=if($case.Unit){$case.Unit}else{'prop_drive_stream'};$source=$sources[$unit]
   if(-not $source.Contains($case.Before)){throw "Missing scope mutation anchor: $($case.Name)"}
   if($case.Function){$start=$source.IndexOf('bool '+$case.Function+'(');$end=if($case.Function -eq 'Park'){$source.IndexOf('void Unpark(',$start)}else{$source.IndexOf('void OnDisconnect(',$start)}
    $changed=$source.Substring(0,$start)+$source.Substring($start,$end-$start).Replace($case.Before,$case.After)+$source.Substring($end)
   }else{$changed=$source.Replace($case.Before,$case.After)}
   $path=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($path,$changed)
   $macro=switch($unit){'prop_park'{'STREAM_SCOPE_PARK_SOURCE'}'prop_wire_parity'{'STREAM_SCOPE_PARITY_SOURCE'}'prop_syncer_host'{'STREAM_SCOPE_CONTROLLER_SOURCE'}default{'STREAM_SCOPE_SOURCE'}}
   $header=Join-Path $output ($case.Name+'.h');[IO.File]::WriteAllText($header,('#define '+$macro+' "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   Compile $case.Name $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Scope mutation did not reach assertion: $($case.Name)"}
   Write-Output "PASS stream scope fault $($case.Name)"
  }
 }
 if($CompileSources){foreach($unit in @('prop_drive_stream','prop_syncer_host','prop_drive_host','prop_park','prop_wire_parity')){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$unit.obj" "src/votv-coop/src/coop/props/$unit.cpp"
  if($LASTEXITCODE){throw "Scope production compile failed: $unit"}
 }}
}finally{Pop-Location}
$global:LASTEXITCODE=0
Write-Output 'PASS stream source scope validation'
