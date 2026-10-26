param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2145'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($name in @('cord','extinguisher','flashlight')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_2145_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
 }
 if($FaultControls){
  $faultRoot=Join-Path $root 'build/flashlight-state-check'
  New-Item -ItemType Directory -Force -Path $faultRoot | Out-Null
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/player/item_activate.cpp')).Replace("`r`n","`n")
  $mutations=@(
   @{Name='session';Before='RetireState();g_scopeState=now;';After='g_scopeState=now;'},
   @{Name='peer';Before='if(old.known && (old.person!=now.person || old.generation!=now.generation))OnDisconnectForSlot(slot);';After='if(false)OnDisconnectForSlot(slot);'},
   @{Name='actor';Before="const auto belongs=[&]{return !coop::players::Registry::Get().IsLocal(puppetActor) &&`n        coop::players::Registry::Get().PeerIdOfActor(puppetActor)==senderPeerSlot;};";After='const auto belongs=[] { return true; };'},
   @{Name='payload';Before='const auto payload=incoming;';After='const auto& payload=incoming;'},
   @{Name='first-setter';Before='if(!current())return false;';After='if(false)return false;'},
   @{Name='send-completion';Before='if(revision!=g_sendRevision || !publisher.Is(coop::players::Registry::Get().Local())){';After='if(!publisher.Is(coop::players::Registry::Get().Local())){'}
  )
  foreach($mutation in $mutations){
   if(-not $source.Contains($mutation.Before)){throw "Missing flashlight fault-control anchor: $($mutation.Name)"}
   $path=Join-Path $faultRoot ($mutation.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($mutation.Before,$mutation.After))
   $header=Join-Path $faultRoot ($mutation.Name+'.h')
   [IO.File]::WriteAllText($header,('#define FLASHLIGHT_STATE_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $exe=Join-Path $faultRoot ($mutation.Name+'.exe')
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/FI$header" "/Fo$faultRoot/$($mutation.Name).obj" "/Fe$exe" agent/tests/flashlight_2145_test.cpp /link /OPT:REF
   if($LASTEXITCODE -ne 0){throw "Flashlight control compile failed: $($mutation.Name)"}
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $mutation.Name 2>&1 | Out-String;$status=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $faultRoot ($mutation.Name+'.log')),$report)
   if($status -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Flashlight control did not fail its assertion: $($mutation.Name)"}
   Write-Host ('PASS flashlight control '+$mutation.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
}finally{Pop-Location}
