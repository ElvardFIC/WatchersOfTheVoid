param([switch]$FaultControls,[switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/prop-verb-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/prop_verb_extract.py" $output
if($LASTEXITCODE){throw 'Verb handoff extraction failed'}
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/config.obj" "$root/src/votv-coop/src/coop/config/config_registry.cpp"
if($LASTEXITCODE){throw 'Verb config compile failed'}
function Compile([string]$name,[string]$header=''){
 $extra=@();if($header){$extra=@("/FI$header")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$output" "/I$PSScriptRoot/prop_publisher_stubs" "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/src/coop/props" @extra "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/prop_verb_test.cpp" "$output/config.obj" /link /OPT:REF
 if($LASTEXITCODE){throw "Verb fixture compile failed: $name"}
}
Compile 'test'
& "$output/test.exe"
if($LASTEXITCODE){throw 'Verb fixture failed'}
if($FaultControls){
 $body=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_drive_host.cpp'))
 $handoffPath=Join-Path $output 'verb_handoff.inc';$handoff=[IO.File]::ReadAllText($handoffPath)
 try{
 foreach($case in @(
  @{Name='world';Before='world == ue_wrap::world_identity::Generation() &&';After='true &&'},
  @{Name='world_pointer';Before='worldObject == ue_wrap::world_identity::CurrentWorld() &&';After='true &&'},
  @{Name='session';Before='session == g_session &&';After='true &&'},
  @{Name='serial';Before='serial == coop::net::session_serial::Current() &&';After='true &&'},
  @{Name='stop';Before='session->StopCount() == stops &&';After='true &&'},
  @{Name='role';Before='session->role() == coop::net::Role::Host';After='true'},
  @{Name='connection';Before='session->connected() &&';After='true &&'},
  @{Name='actor';Before='Scope() && actor.Get() &&';After='Scope() && actor.Raw() &&'},
  @{Name='class';Before='actor.Get() && cls.Get() &&';After='actor.Get() && cls.Raw() &&'},
  @{Name='vacancy';Before='proof.Current() && !Find(actor) && (!candidate || !FindByEid(candidate))';After='proof.Current()'},
  @{Name='recursion';Before='if (!opening.entered) return nullptr;';After='/* nested admission allowed */'},
  @{Name='reason';Before='const std::string reason = why ? why : "";';After='const char* reason = why ? why : "";';ExtraBefore='reason.c_str()';ExtraAfter='reason'},
  @{Name='host_id';Before='eid >= coop::element::kHostRangeSize';After='eid == coop::element::kInvalidId'},
  @{Name='key_bound';Before='key.size() > sizeof(coop::net::WireKey{}.data)';After='false';ExtraBefore='wire.data[wire.len++] = static_cast<char>(ch);';ExtraAfter='if(wire.len<sizeof wire.data)wire.data[wire.len++] = static_cast<char>(ch);'},
  @{Name='key_none';Before='key == L"None"';After='false'},
  @{Name='key_encoding';Before='ch > 255';After='false'},
  @{Name='canonical_eid';Before='finalEid != candidate';After='false'},
  @{Name='row_revision';Before='row->revision == value.revision &&';After='true &&'},
  @{Name='handoff_parent';Before='row->handoffFrom != before.revision';After='false'},
  @{Name='handoff_consume';Before='next.handoffFrom = 0;';After='/* predecessor left reusable */'},
  @{Name='handoff_origin';Before='d.handoffFrom = receipt.value.revision;';After='d.handoffFrom = receipt.value.episode;';Handoff=$true}
 )){
  $source=if($case.Handoff){$handoff}else{$body}
  if(-not $source.Contains($case.Before)){throw "Missing verb fault anchor: $($case.Name)"}
  $source=$source.Replace($case.Before,$case.After)
  if($case.ExtraBefore){$source=$source.Replace($case.ExtraBefore,$case.ExtraAfter)}
  $header=''
  if($case.Handoff){[IO.File]::WriteAllText($handoffPath,$source)}else{
   $path=Join-Path $output 'verb_fault.cpp';[IO.File]::WriteAllText($path,$source)
   $header=Join-Path $output 'fault_source.h'
   [IO.File]::WriteAllText($header,('#define PROP_PUBLISHER_SOURCE "'+$path.Replace([char]92,[char]47)+'"'+[Environment]::NewLine))
  }
  Compile $case.Name $header
  $saved=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
  [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
  if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Verb fault did not reach an assertion: $($case.Name)"}
  Write-Output "PASS verb fault $($case.Name)"
  [IO.File]::WriteAllText($handoffPath,$handoff)
 }
 }finally{[IO.File]::WriteAllText($handoffPath,$handoff)}
}
if($CompileSources){
 foreach($unit in @('coop/props/prop_drive_host','coop/props/prop_syncer_host','coop/creatures/npc_pose_host','coop/items/hook_prop_claim','coop/creatures/kerfus_state','coop/props/watoy_sync')){
  $name=$unit.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/GameNetworkingSockets/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/$unit.cpp"
  if($LASTEXITCODE){throw "Verb production compile failed: $unit"}
 }
}
$global:LASTEXITCODE=0
