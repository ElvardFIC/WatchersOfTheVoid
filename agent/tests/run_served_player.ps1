param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/served-player-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($script in @('prop_claim_extract','served_player_extract')){
 & "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/$script.py" $output
 if($LASTEXITCODE){throw "Served-player extraction failed: $script"}
}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/config.obj" "$root/src/votv-coop/src/coop/config/config_registry.cpp"
if($LASTEXITCODE){throw 'Served-player config compile failed'}
foreach($name in @('served_player','served_insert_native')){
 $libraries=@();if($name -eq 'served_player'){$libraries=@("$output/config.obj")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/served_player_stubs" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" @libraries /link /OPT:REF
 if($LASTEXITCODE){throw "Served-player fixture compile failed: $name"}
 & "$output/$name.exe"
 if($LASTEXITCODE){throw "Served-player fixture failed: $name"}
}
if($CompileSources){
 foreach($unit in @('coop/creatures/served_player','coop/creatures/kerfus_follow','ue_wrap/actors/kerfur')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "Served-player production compile failed: $unit"}
 }
}
if($FaultControls){
 $sources=@{
  service=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/served_player.cpp'))
  follow=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/kerfus_follow.cpp'))
  insert=[IO.File]::ReadAllText((Join-Path $output 'served_insert.inc'))
 }
 foreach($fault in @(
  @{Name='epoch';Before='epoch==g_epoch&&';After='true&&'},
  @{Name='serial';Before='serial==N::session_serial::Current()&&';After='true&&'},
  @{Name='generation';Before='s->peerGenerationForSlot(row->slot)!=row->generation';After='false'},
  @{Name='ready_revision';Before='s->SlotWorldReadyRevision(row->slot)!=row->ready';After='false'},
  @{Name='release_token';Before='it->second->lease.token!=lease.token';After='false'},
  @{Name='receipt';Before='it->second==row';After='true'},
  @{Name='caller';Before='(!caller||caller())';After='true'},
  @{Name='puppet_identity';Before='const bool valid=result.puppet->valid();if(!fresh())return false;';After='const bool valid=result.puppet->valid();if(!Installed(row))return false;'},
  @{Name='callee';Before='callee!=before.fn.Raw()';After='false'},
  @{Name='out_bounds';Before='sizeof(void*)>size-offset';After='false'},
  @{Name='out_size';Before='width!=sizeof(void*)';After='false'},
  @{Name='out_type';Before='type==L"ObjectProperty"';After='true'},
  @{Name='insert_selection';Before='!ue_wrap::kerfur::IsInsertPlayerReadWhile';After='false&&ue_wrap::kerfur::IsInsertPlayerReadWhile'},
  @{Name='arm_knowledge';Before='if(seam->revision==before.revision)seam->armKnown=false;';After='/* loses unknown arm state */'},
  @{Name='native_fallback';Before='||!body.actor.Raw()';After=''},
  @{Name='follow_read';Unit='follow';Before='if(!ue_wrap::kerfus::ReadActiveWhile(actor,active,current)||!current())';After='if((ue_wrap::kerfus::ReadActiveWhile(actor,active,current),false)||!current())'},
  @{Name='follow_debt';Unit='follow';Before='if(!ue_wrap::kerfus::ReadActiveWhile(row->actor.Raw(),active,current)||!current())continue;';After='if(!ue_wrap::kerfus::ReadActiveWhile(row->actor.Raw(),active,current)||!current()){Drop(row);continue;}'},
  @{Name='follow_release';Unit='follow';Before='SP::UnserveOwned(row->lease);';After='SP::Unserve(row->actor.Raw());'},
  @{Name='host_cleanup';Unit='follow';Before='if(!slot){g_activator.erase(actor);';After='if(!slot){SP::Unserve(actor);g_activator.erase(actor);'},
  @{Name='follow_recursion';Unit='follow';Before='if(g_ticking)return;';After='if(false)return;'},
  @{Name='insert_class';Unit='insert';Before='!layout.cls.Is(cls)';After='false'},
  @{Name='insert_type';Unit='insert';Before='type!=L"ObjectProperty"';After='false'},
  @{Name='insert_extent';Unit='insert';Before='sizeof(void*)>layout.size-layout.offset';After='false'},
  @{Name='insert_width';Unit='insert';Before='field.size==sizeof(void*)';After='true'},
  @{Name='insert_caller';Unit='insert';Before='(!current||current())';After='true'},
  @{Name='insert_result';Unit='insert';Before='return out==callerLocals+layout.offset;';After='return true;'}
 )){
  $unit=if($fault.Unit){$fault.Unit}else{'service'};$original=$sources[$unit]
  if(-not $original.Contains($fault.Before)){throw "Missing service fault anchor: $($fault.Name)"}
  $path=Join-Path $output ($fault.Name+'.cpp');[IO.File]::WriteAllText($path,$original.Replace($fault.Before,$fault.After))
  $define=@{service='SERVED_PLAYER_SOURCE';follow='KERFUS_FOLLOW_SOURCE';insert='SERVED_INSERT_SOURCE'}[$unit]
  $header=Join-Path $output 'fault_source.h';[IO.File]::WriteAllText($header,('#define '+$define+' "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  $fixture=if($unit -eq 'insert'){'served_insert_native'}else{'served_player'}
  $libraries=@();if($unit -ne 'insert'){$libraries=@("$output/config.obj")}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/served_player_stubs" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" "/I$root/src/votv-coop/src/coop/items" "/FI$header" "/Fo$output/fault.obj" "/Fe$output/fault.exe" "$PSScriptRoot/${fixture}_test.cpp" @libraries /link /OPT:REF
  if($LASTEXITCODE){throw "Service fault compile failed: $($fault.Name)"}
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/fault.exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($fault.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Service fault did not reach an assertion: $($fault.Name)"}
  Write-Output "PASS service fault $($fault.Name)"
 }
}
$global:LASTEXITCODE=0
