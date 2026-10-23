param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-action-delivery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$Name,[string]$SourcePath){
  $flags=@('/nologo','/std:c++20','/EHsc','/utf-8','/Gy','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs','/I','src/votv-coop/include')
  if($SourcePath){$header=Join-Path $output ($Name+'_source.h');[IO.File]::WriteAllText($header,('#define PROP_ACTION_SOURCE "'+$SourcePath.Replace('\','/')+'"'+[Environment]::NewLine));$flags+="/FI$header"}
  $exe=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" agent/tests/prop_action_delivery_test.cpp /link /OPT:REF | Out-Host
  if($LASTEXITCODE){throw "Prop action delivery compile failed: $Name"}
  return $exe
 }
 $exe=Build-Fixture 'delivery' ''
 & $exe
 if($LASTEXITCODE){throw 'Prop action delivery regression failed'}
 & (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') agent/tests/mre_native_test.py
 if($LASTEXITCODE){throw 'Cooked MRE contract failed'}
 if($FaultControls){
  $controller=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_action_sync.cpp'))
  $controls=@(
   @{Name='prequeue-send';Case='nested-fifo';Edits=@(@{Before='g_pending.push_back(queued);';After='if(first){s->SendReliableToSlot(0,coop::net::ReliableKind::PropActionIntent,&p,sizeof(p));return current();}g_pending.push_back(queued);'})},
   @{Name='nested-drain';Case='nested-fifo';Edits=@(@{Before='if(g_draining)return;';After='if(false)return;'})},
   @{Name='drain-budget';Case='bounded-drain';Edits=@(@{Before='work<8';After='work<256'})},
   @{Name='session-serial';Case='context-retirement';Edits=@(@{Before='serial==g_serial';After='true'},@{Before='g_serial==coop::net::session_serial::Current()';After='true'})},
   @{Name='target-class';Case='queued-target';Edits=@(@{Before='R::ClassOf(actor.Raw())==type.Raw()';After='true'})},
   @{Name='spawn-scope';Case='hand-mre-scope';Edits=@(@{Before='bool IsLocalMreOutput(void* actor,void* callerFunction) {';After='bool IsLocalMreOutput(void* actor,void* callerFunction) { if(actor && callerFunction)return true;'})},
   @{Name='one-open';Case='host-mre-once';Edits=@(@{Before='return row.actor.Raw()==target;';After='return false;'})},
   @{Name='function-incarnation';Case='host-mre-preparation';Edits=@(@{Before='TargetCurrent(attempt.actor,attempt.type) && functionRef.Get() && ownerRef.Get()';After='TargetCurrent(attempt.actor,attempt.type) && ownerRef.Get()'})},
   @{Name='safe-revision';Case='safe-requeue';Edits=@(@{Before='latest->second.serial==row.serial';After='true'})}
  )
  $mutantRoot=Join-Path $output 'mutants';New-Item -ItemType Directory -Force -Path $mutantRoot | Out-Null
  foreach($control in $controls){
   $copy=$controller
   foreach($edit in $control.Edits){if(-not $copy.Contains($edit.Before)){throw "Missing prop action control anchor: $($control.Name)"};$copy=$copy.Replace($edit.Before,$edit.After)}
   $path=Join-Path $mutantRoot ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $exe=Build-Fixture ('fault_'+$control.Name) $path
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($control.Case)+' line ')){throw "Prop action control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
 foreach($source in @('coop/props/prop_action_sync','coop/props/prop_spawn_authoring')){
  $name=$source.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
  if($LASTEXITCODE){throw "Prop action production compile failed: $source"}
 }
}finally{Pop-Location}
exit 0
