param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/event-slot-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/test.obj" "/Fe$output/test.exe" agent/tests/event_slot_canonical_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Event slot fixture compile failed'}
 & "$output/test.exe"
 if($LASTEXITCODE){throw 'Event slot fixture failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/interactables/floppy_slot_sync.cpp'))
  $controls=@(
   @{Name='sequence';Before='if(order.known && int32_t(sequence-order.sequence)<=0)return false;';After='/* older canonical admitted */'},
   @{Name='retry';Before='if(!ApplySlot(kind,index,devices[index],item.value,current,retryWrite) || !current())continue;';After='ApplySlot(kind,index,devices[index],item.value,current,retryWrite);if(!current())continue;'},
   @{Name='presentation';Before='const bool retryWrite=g_pendingWriteDebt.count(key)!=0;';After='const bool retryWrite=false;'},
   @{Name='revision';Before='row->second==item.revision';After='true'},
   @{Name='claim';Before='if(!host && g_pendingCanonical.count(ShadowKey(kind,i)))continue;';After='/* stale local claim admitted */'},
   @{Name='reentry';Before='if(g_pendingDraining)return;';After='/* nested canonical writes admitted */'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing event slot control: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $header=Join-Path $output ($control.Name+'.h')
   [IO.File]::WriteAllText($header,('#define EVENT_SLOT_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I src/votv-coop/include "/Fo$output/$($control.Name).obj" "/Fe$output/$($control.Name).exe" agent/tests/event_slot_canonical_test.cpp /link /OPT:REF
   if($LASTEXITCODE){throw "Event slot control compile failed: $($control.Name)"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($control.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Event slot control did not reach assertion: $($control.Name)"}
   Write-Output "PASS event slot fault $($control.Name)"
  }
 }
 if($CompileSources){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/floppy_slot_sync.obj" src/votv-coop/src/coop/interactables/floppy_slot_sync.cpp
  if($LASTEXITCODE){throw 'Event slot production compile failed'}
 }
}finally{Pop-Location}
