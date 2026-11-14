param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-publisher-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $includes=@('/I','agent/tests/prop_publisher_stubs','/I','src/votv-coop/include','/I','src/votv-coop/src/coop/props')
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c /I src/votv-coop/include "/Fo$output/registry.obj" src/votv-coop/src/coop/config/config_registry.cpp
 if($LASTEXITCODE){throw 'Publisher config registry compile failed'}
 function Compile([string]$name,[string]$header=''){
  $forced=@();if($header){$forced=@("/FI$header")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes @forced "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/prop_publisher_test.cpp "$output/registry.obj" /link /OPT:REF
  if($LASTEXITCODE){throw "Publisher fixture compile failed: $name"}
 }
 Compile 'test'
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Publisher fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_drive_host.cpp')).Replace("`r`n","`n")
  $cases=@(
   @{Name='world';Before='world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='session';Before='serial == coop::net::session_serial::Current() &&';After='true &&'},
   @{Name='revision';Before='row->revision == value.revision &&';After='true &&'},
   @{Name='class';Before='R::ClassOf(value.ref.Get()) == value.cls.Get() &&';After='true &&'},
   @{Name='tick_recursion';Before='if (!BindSession(s, true) || g_ticking) return;';After='if (!BindSession(s, true)) return;'},
   @{Name='pump_recursion';Before='if (!BindSession(s, isHost) || g_pumping) return;';After='if (!BindSession(s, isHost)) return;'},
   @{Name='root_refresh';Before='d.rootComp.Reset();';After='/* stale root retained */'},
   @{Name='root_binding';Before='return Basic() && root == ref.Get();';After='return Basic();'},
   @{Name='pose_bounds';Before='!PositionInBounds(loc)';After='false'},
   @{Name='velocity_finite';Before='!Finite(lin)';After='false'},
   @{Name='final_hand';Before='if (!Unheld(receipt) || !root.Current()) return 0;';After='if (!root.Current()) return 0;'},
   @{Name='end_retry';Before='coop::prop_end_delivery::Tick(s);';After='/* retained End delivery not drained */'},
   @{Name='send_completion';Before='if (sent) Retire(receipt);';After='if (true) Retire(receipt);'},
   @{Name='pump_stage';Before='coop::prop_syncer_host::Admit(s);'+"`n"+'        if (!context.Current()) return;';After='coop::prop_syncer_host::Admit(s);'},
   @{Name='slot_owner';Before='value.ownerGeneration == context.session->peerGenerationForSlot(slot) &&';After='true &&'},
   @{Name='clock';Before='return now >= before ? now - before : 0;';After='return now - before;'}
  )
  foreach($case in $cases){
   if(-not $source.Contains($case.Before)){throw "Missing publisher mutation anchor: $($case.Name)"}
   $path=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($case.Before,$case.After))
   $header=Join-Path $output ($case.Name+'.h');[IO.File]::WriteAllText($header,('#define PROP_PUBLISHER_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   Compile $case.Name $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Publisher mutation did not reach an assertion: $($case.Name)"}
   Write-Output "PASS publisher fault $($case.Name)"
  }
 }
 if($CompileSources){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/prop_drive_host.obj" src/votv-coop/src/coop/props/prop_drive_host.cpp
  if($LASTEXITCODE){throw 'Publisher production compile failed'}
 }
}finally{Pop-Location}
$global:LASTEXITCODE=0
Write-Output 'PASS host publisher validation'
