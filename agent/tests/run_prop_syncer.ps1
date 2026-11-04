param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-syncer-client-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $hostSource=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_drive_host.cpp'))
 $start=$hostSource.IndexOf('coop::net::PropDriveEndPayload EndFromBody(')
 if($start -lt 0){throw 'EndFromBody source missing'}
 $open=$hostSource.IndexOf('{',$start);$end=$open+1;$depth=1
 while($depth -and $end -lt $hostSource.Length){if($hostSource[$end] -eq '{'){$depth++};if($hostSource[$end] -eq '}'){$depth--};$end++}
 if($depth){throw 'EndFromBody source is not balanced'}
 $endSource=$hostSource.Substring($start,$end-$start)
 [IO.File]::WriteAllText((Join-Path $output 'prop_syncer_end.inc'),$endSource)
 $includes=@('/I','agent/tests/prop_syncer_stubs','/I','src/votv-coop/include','/I','src/votv-coop/src/coop/props',"/I$output")
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
 if($LASTEXITCODE){throw 'Prop syncer registry compile failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/test.obj" "/Fe$output/test.exe" agent/tests/prop_syncer_client_test.cpp "$output/registry.obj" /link /OPT:REF
 if($LASTEXITCODE){throw 'Prop syncer fixture compile failed'}
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Prop syncer fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_syncer_client.cpp'))
  $cases=@(
   @{Name='world';Before='world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='session';Before='serial == coop::net::session_serial::Current() &&';After='true &&'},
   @{Name='revision';Before='found->second.revision == value.revision &&';After='true &&'},
   @{Name='class';Before='ue_wrap::reflection::ClassOf(value.ref.Get()) == value.cls.Get()';After='true'},
   @{Name='nested_tick';Before='if (g_ticking || isHost || !Context{}.Current()) return;';After='if (isHost || !Context{}.Current()) return;'},
   @{Name='grant_copy';Before='const auto p = input;';After='const auto& p = input;'},
   @{Name='pending_clock';Before='Elapsed(now, e.claimSentMs) >= kPendingMaxMs';After='now - e.claimSentMs >= kPendingMaxMs'},
   @{Name='finite_pose';Before='!Finite(loc)';After='false'},
   @{Name='finite_velocity';Before='!Finite(lin)';After='false'},
   @{Name='root_refresh';Before='e.rootComp.Reset();';After='/* stale root retained */'},
   @{Name='end_retry';Before='if (!sent) {';After='if (false) {'},
   @{Name='root_binding';Before='return Basic() && root == ref.Get();';After='return Basic();'},
   @{Name='final_hand';Before='if (!Unheld(receipt, now) || !root.Current()) return;';After='if (!root.Current()) return;'},
   @{Name='orphan_retry';Before='if (!current() || !sent) return;';After='if (!current()) return;'},
   @{Name='end_capture_scope';Source='end';Before='if (current && !current()) return {};';After='/* later body read continues */'}
  )
  foreach($case in $cases){
   $original=if($case.Source -eq 'end'){$endSource}else{$source}
   if(-not $original.Contains($case.Before)){throw "Missing prop syncer fault anchor: $($case.Name)"}
   $path=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($path,$original.Replace($case.Before,$case.After))
   $header=Join-Path $output ($case.Name+'.h')
   $macro=if($case.Source -eq 'end'){'PROP_SYNCER_END_SOURCE'}else{'PROP_SYNCER_CLIENT_SOURCE'}
   [IO.File]::WriteAllText($header,('#define '+$macro+' "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/FI$header" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" agent/tests/prop_syncer_client_test.cpp "$output/registry.obj" /link /OPT:REF
   if($LASTEXITCODE){throw "Prop syncer fault compile failed: $($case.Name)"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Prop syncer fault did not reach assertion: $($case.Name)"}
   Write-Output "PASS prop syncer fault $($case.Name)"
  }
 }
 if($CompileSources){
  foreach($unit in @('prop_syncer_client','prop_drive_host')){
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$unit.obj" "src/votv-coop/src/coop/props/$unit.cpp"
   if($LASTEXITCODE){throw "Prop syncer production compile failed: $unit"}
  }
 }
}finally{Pop-Location}
$global:LASTEXITCODE=0
