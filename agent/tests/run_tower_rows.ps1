param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/tower-rows-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
. (Join-Path $root '../_toolchain/env.ps1')
$ops=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/world/fuse_tower_ops.cpp'))
$fifo=[regex]::Match($ops,'(?ms)^struct RefusedFifo \{.*?^\};').Value
$told=[regex]::Match($ops,'(?ms)^void RefusedTold\(.*?^\}').Value
if(-not $fifo -or -not $told){throw 'Production refusal receipt source missing'}
[IO.File]::WriteAllText((Join-Path $output 'tower_refused_fifo.inc'),$fifo)
[IO.File]::WriteAllText((Join-Path $output 'tower_refused_told.inc'),$told)
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/tower_stubs" "/I$root/src/votv-coop/include" "/I$output" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/tower_rows_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Tower rows fixture compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Tower rows regression failed'}

& (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') (Join-Path $PSScriptRoot 'tower_native_contract_test.py')
if($LASTEXITCODE){throw 'Tower cooked contract failed'}
if($FaultControls){
 $cases=@(
  @{Name='refused_broadcast';Source='rows';Before='if(!Host(scope)||!sent)return false;';After='if(!Host(scope))return false;'},
  @{Name='session_serial';Source='rows';Before='s.serial==net::session_serial::Current() &&';After='true &&'},
  @{Name='peer_generation';Source='rows';Before='scope.session->peerGenerationForSlot(slot)==generation&&';After='true&&'},
  @{Name='dirty_revision';Source='rows';Before='if(revision!=g_dirtyRevision)return false;';After=''},
  @{Name='recursive_send';Source='rows';Before='||g_sending';After=''},
  @{Name='join_revision';Source='rows';Before='debts[slot].revision!=debt.revision';After='false'},
  @{Name='refusal_sequence';Source='ops';Before=' || f.At(0) != expectedSeq';After=''},
  @{Name='oversized_rows';Source='rows';Before='if(p.op!=net::kFuseTowerOpRows||p.count>kFuseTowers){p.count=0;return;}';After='if(p.count>kFuseTowers)p.count=kFuseTowers;'},
  @{Name='kept_revision';Source='rows';Before='k.revision==note.revision&&';After='true&&'},
  @{Name='role_replacement';Source='rows';Before='s.role==installed->role()';After='true'},
  @{Name='recursive_apply';Source='rows';Before='if(!Client(scope)||g_applyDepth)return;';After='if(!Client(scope))return;'},
  @{Name='paint_retry';Source='kind';Before='if(!Write(f))return false;';After='if(!Write(f)){if(ctx.writes)ctx.writes(0,bit);else localDirty&=~bit;return false;}'},
  @{Name='effect_receipt';Source='kind';Before='const bool first=First(bit);';After='const bool first=true;'},
  @{Name='continuation_scope';Source='kind';Before='(!ctx.current||ctx.current())';After='true'},
  @{Name='coordinate_bounds';Source='kind';Before='c.lightCount>CT::kMaxLights||';After=''},
  @{Name='canonical_repair_cells';Source='kind';Before='if((run.Wanted()&Bit(Repair))&&c.lightCount==l.cellCount)';After='if(false)'},
  @{Name='state_delegate';Source='kind';Before='return RT::WriteBroken(tower,broken);';After='return RT::SetBroken(tower,broken,false);'},
  @{Name='ack_prediction';Source='rows';Before='else{k->revision=++g_revision;k->pending=true;}';After='else{}'},
  @{Name='panel_identity';Source='rows';Before='Elapsed(now,note.panelAskedMs)<kPanelRetryMs';After='Elapsed(now,g_kept[(i+1)%g_kept.size()].panelAskedMs)<kPanelRetryMs'},
  @{Name='operation_scope';Source='kind';Before='if (!active()) return;';After='/* cancelled operation continues */'},
  @{Name='operation_write';Source='kind';Before='if (!CT::WriteFuse(tower, slot, 0)) return false;';After='CT::WriteFuse(tower, slot, 0);'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/coop/world/fuse_tower_'+$case.Source+'.cpp')))
  if(-not $source.Contains($case.Before)){throw "Missing tower fault anchor: $($case.Name)"}
  $changed=$source.Replace($case.Before,$case.After)
  if($case.Source -eq 'ops'){$changed=[regex]::Match($changed,'(?ms)^void RefusedTold\(.*?^\}').Value}
  $mutant=Join-Path $output ($case.Name+'_source.inc')
  [IO.File]::WriteAllText($mutant,$changed)
  $macro=if($case.Source -eq 'rows'){'TOWER_ROWS_SOURCE'}elseif($case.Source -eq 'kind'){'TOWER_KIND_SOURCE'}else{'TOWER_TOLD_SOURCE'}
  $fixture=Join-Path $output ($case.Name+'_test.cpp')
  [IO.File]::WriteAllText($fixture,('#define '+$macro+' "'+$mutant.Replace('\','/')+'"'+"`n"+'#include "'+(Join-Path $PSScriptRoot 'tower_rows_test.cpp').Replace('\','/')+'"'))
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/tower_stubs" "/I$root/src/votv-coop/include" "/I$output" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" $fixture /link /OPT:REF
  if($LASTEXITCODE){throw "Tower fault compile failed: $($case.Name)"}
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
  if($code -ne 1 -or $report -notmatch 'FAIL \d+:'){throw "Tower fault not detected: $($case.Name)"}
  Write-Output "PASS rejected tower fault: $($case.Name)"
 }
}
$global:LASTEXITCODE=0
Write-Output 'PASS tower rows validation'
