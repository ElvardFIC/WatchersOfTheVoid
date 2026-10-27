param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/hand-toy-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($testName in @('hand_toy_2157','dingus_2155','prop_knock_2157')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$testName.obj" "/Fe$output/$testName.exe" "agent/tests/${testName}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $testName"}
  & "$output/$testName.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $testName"}
 }
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/toy_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/hand_toy_lifetime.obj" "/Fe$output/hand_toy_lifetime.exe" agent/tests/hand_toy_lifetime_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Hand toy lifetime compile failed'}
 & "$output/hand_toy_lifetime.exe"
 if($LASTEXITCODE){throw 'Hand toy lifetime failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/hand_toy_budget.obj" "/Fe$output/hand_toy_budget.exe" agent/tests/hand_toy_budget_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Hand toy budget compile failed'}
 & "$output/hand_toy_budget.exe"
 if($LASTEXITCODE){throw 'Hand toy budget failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/player/hand_toy_audio.cpp'))
  $controls=@(
   @{Name='scope';Changes=@(@('g_serial==net::session_serial::Current()','true'),@('g_serial!=net::session_serial::Current()','false'))},
   @{Name='source';Changes=@(@('a.connection==b.connection','true'))},
   @{Name='recipient';Changes=@(@('a.connection==b.connection','true'))},
   @{Name='frozen-hand';Changes=@(@('frame.generation==hand_item::Generation(frame.slot)','true'))},
   @{Name='frames';Changes=@(@('g_frames.push_back(frame);','/* late reservation */'),@('const bool valid=selected && FrameCurrent(frame);','g_frames.push_back(frame);const bool valid=selected && FrameCurrent(frame);'))},
   @{Name='admission';Changes=@(@('if(p.op && g_pulses.size()>=32)return;','if(p.op && g_pulses.size()>=32){g_seen[p.slot][p.op]=p.sequence;return;}'))},
   @{Name='payload';Changes=@(@('const Packet p=packet;','const Packet& p=packet;'))},
   @{Name='budget';Changes=@(@('g_sendBudget=64;','g_sendBudget=4096;'))},
   @{Name='clock';Changes=@(@(' && Fresh(pulse.expires) && live.Get()',' && live.Get()'))},
   @{Name='completion';Changes=@(@('Same(latest.targets.slots[slot],row.targets.slots[slot])','true'))}
  )
  foreach($control in $controls){
   $mutant=$source;$changes=$control.Changes
   if($changes[0] -is [string]){$changes=@(,$changes)}
   foreach($change in $changes){if(-not $mutant.Contains($change[0])){throw "Missing hand toy control anchor: $($control.Name)"};$mutant=$mutant.Replace($change[0],$change[1])}
   $name='fault_'+$control.Name;$path=Join-Path $output ($name+'.cpp');[IO.File]::WriteAllText($path,$mutant)
   $header=Join-Path $output ($name+'.h');[IO.File]::WriteAllText($header,('#define HAND_TOY_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I agent/tests/toy_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/hand_toy_lifetime_test.cpp /link /OPT:REF
   if($LASTEXITCODE){throw "Hand toy control compile failed: $name"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$name.exe" $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Hand toy control did not reach an assertion: $name"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
 if($CompileSources){
  foreach($file in @('coop/player/hand_toy_audio','coop/props/dingus_music_sync','ue_wrap/actors/dingus_music','coop/props/prop_knock','coop/props/prop_drive_host','coop/dispatch/event_dispatch_world','coop/session/subsystems')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
