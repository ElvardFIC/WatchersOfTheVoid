param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/npc-pose-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/npc_pose_extract.py" $output
if($LASTEXITCODE){throw 'NPC pose extraction failed'}
function CompileFixture([string]$name,[string[]]$extra=@()) {
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/npc_join_stubs" "/I$root/src/votv-coop/include" @extra "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/npc_pose_test.cpp" /link /OPT:REF
 if($LASTEXITCODE){throw "NPC pose fixture compile failed: $name"}
}
CompileFixture 'test'
& "$output/test.exe"
if($LASTEXITCODE){throw 'NPC pose fixture failed'}
if($FaultControls){
 $body=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/creatures/npc_pose_host.cpp'))
 foreach($case in @(
  @{Name='recursion';Before='if(g_ticking)return;';After='if(g_ticking){ coop::npc_join_delivery::Tick(); return; }'},
  @{Name='session';Before='session==GetSession()&&';After='true&&'},
  @{Name='serial';Before='serial==N::session_serial::Current()&&';After='true&&'},
  @{Name='world';Before='world==ue_wrap::world_identity::Generation()&&';After='true&&'},
  @{Name='world_pointer';Before='worldObject==ue_wrap::world_identity::CurrentWorld()&&';After='true&&'},
  @{Name='stop';Before='stops==session->StopCount()&&';After='true&&'},
  @{Name='running';Before='session->running()&&';After='true&&'},
  @{Name='role';Before='session->role()==role';After='true'},
  @{Name='epoch';Before='epoch==g_epoch&&';After='true&&'},
  @{Name='element';Before='now.incarnation==identity.incarnation&&';After='true&&'},
  @{Name='actor_serial';Before='ref.Get()&&cls.Get()';After='ref.Raw()&&cls.Get()'},
  @{Name='class_serial';Before='ref.Get()&&cls.Get()';After='ref.Get()&&cls.Raw()'},
  @{Name='reverse';Before='E::Registry::Get().EidForActor(actor.ref.Raw())==identity.id';After='true'},
  @{Name='final_rows';Before='if(item.row.Current())';After='if(true)'},
  @{Name='fairness';Before='g_turn++%total';After='0'},
  @{Name='hand';Before='if(!SelectionCurrent(hold)||inHand)return;';After='if(!SelectionCurrent(hold))return;'},
  @{Name='selection';Before='selected==hold->prop.ref.Raw();';After='true;'},
  @{Name='thaw_retry';Before='if(current()&&thawed)Erase(hold);';After='if(current())Erase(hold);'},
  @{Name='release_once';Before='if(!hold->released){';After='if(true){'},
  @{Name='other_holder';Before='if(other!=hold&&Selected(other)&&';After='if(false&&'},
  @{Name='velocity';Before='!VectorOkay(velocity,N::kMaxSpeed)';After='false'},
  @{Name='look_finite';Before='hasLook&&VectorOkay(look,N::kMaxCoord)';After='hasLook'},
  @{Name='yaw_finite';Before='hasYaw&&std::isfinite(yaw)';After='hasYaw'}
 )){
  if(-not $body.Contains($case.Before)){throw "Missing pose fault anchor: $($case.Name)"}
  $path=Join-Path $output 'pose_fault.cpp'
  [IO.File]::WriteAllText($path,$body.Replace($case.Before,$case.After))
  $header=Join-Path $output 'fault_source.h'
  [IO.File]::WriteAllText($header,('#define NPC_POSE_SOURCE "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  CompileFixture $case.Name @("/FI$header")
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Pose fault did not reach an assertion: $($case.Name)"}
  Write-Output "PASS NPC pose fault $($case.Name)"
 }
}
if($CompileSources){
 foreach($unit in @('coop/creatures/npc_pose_host','coop/net/session_npc','coop/creatures/npc_sync','coop/props/prop_drive_host')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "NPC pose production compile failed: $unit"}
 }
}
$global:LASTEXITCODE=0
