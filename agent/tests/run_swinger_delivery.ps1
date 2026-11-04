param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/swinger-delivery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Compile([string]$name,[string]$header=''){
  $forced=@();if($header){$forced=@("/FI$header")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/swinger_delivery_stubs /I src/votv-coop/include @forced "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/swinger_delivery_test.cpp /link /OPT:REF
  if($LASTEXITCODE){throw "Swinger delivery fixture compile failed: $name"}
 }
 Compile 'test'
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Swinger delivery fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/interactables/swinger_join_pose.cpp')).Replace("`r`n","`n")
  $cases=@(
   @{Name='world';Before='join->world == ue_wrap::world_identity::Generation() &&';After='true &&'},
   @{Name='session';Before='join->serial == coop::net::session_serial::Current() &&';After='true &&'},
   @{Name='stop';Before='join->session->StopCount() == join->stopped &&';After='true &&'},
   @{Name='role';Before='join->session->role() == coop::net::Role::Host &&';After='true &&'},
   @{Name='recipient';Before='join->session->peerGenerationForSlot(join->slot) == join->generation &&';After='true &&'},
   @{Name='ready';Before='return HostCurrent(join) && join->session->IsSlotWorldReady(join->slot);';After='return HostCurrent(join);'},
   @{Name='actor';Before='lid->actor.Get() &&';After='lid->actor.Raw() &&'},
   @{Name='class';Before='lid->cls.Get() &&';After='lid->cls.Raw() &&'},
   @{Name='binding';Before='bound == lid->actor.Raw()';After='true'},
   @{Name='pace';Before='now < lid->due';After='false'},
   @{Name='refused';Before='if (sent) FinishHostLid(join, lid);';After='FinishHostLid(join, lid);'},
   @{Name='newer';Before='g_hostJoins[join->slot] == join &&';After='true &&'},
   @{Name='recursion';Before='if (g_hostDraining) return;';After='/* recursive drain admitted */'},
   @{Name='finite';Before='!Finite(rest)';After='false'},
   @{Name='bounds';Before='std::fabs(rest.loc.X) > coop::net::kMaxCoord';After='false'},
   @{Name='forgotten';Before='join->forgotten.count(key) ||';After='false ||'},
   @{Name='budget';Before='visited < coop::net::kMaxPeers - 1 && budget';After='visited < coop::net::kMaxPeers - 1'},
   @{Name='unreadable';Before='if (!read) return;';After='if (!read) { FinishHostLid(join, lid); return; }'},
   @{Name='class_binding';Before='ue_wrap::reflection::ClassOf(lid->actor.Raw()) == lid->cls.Raw()';After='true'}
  )
  foreach($case in $cases){
   if(-not $source.Contains($case.Before)){throw "Missing swinger mutation anchor: $($case.Name)"}
   $path=Join-Path $output ($case.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($case.Before,$case.After))
   $header=Join-Path $output ($case.Name+'.h');[IO.File]::WriteAllText($header,('#define SWINGER_DELIVERY_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   Compile $case.Name $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Swinger mutation did not reach an assertion: $($case.Name)"}
   Write-Output "PASS swinger fault $($case.Name)"
  }
 }
 if($CompileSources){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/swinger_join_pose.obj" src/votv-coop/src/coop/interactables/swinger_join_pose.cpp
  if($LASTEXITCODE){throw 'Swinger production compile failed'}
 }
}finally{Pop-Location}
$global:LASTEXITCODE=0
Write-Output 'PASS swinger delivery validation'
