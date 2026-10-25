param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/prop-action-native-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$Name,[string]$Suite,[string]$SourcePath){
  $flags=@('/nologo','/std:c++20','/EHsc','/utf-8','/Gy')
  if($Suite -eq 'actions'){$flags+=@('/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
  $flags+=@('/I','src/votv-coop/include')
  $define=switch($Suite){'actions'{'PROP_ACTION_SOURCE'}'hit'{'HIT_RESULT_SOURCE'}'wall'{'WALL_WRAPPER_SOURCE'}}
  $fixture=switch($Suite){'actions'{'prop_action_native'}'hit'{'hit_result_lifetime'}'wall'{'wall_damage_lifetime'}}
  if($SourcePath){$header=Join-Path $output ($Name+'_source.h');[IO.File]::WriteAllText($header,('#define '+$define+' "'+$SourcePath.Replace('\','/')+'"'+[Environment]::NewLine));$flags+="/FI$header"}
  $exe=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" "agent/tests/${fixture}_test.cpp" /link /OPT:REF | Out-Host
  if($LASTEXITCODE){throw "Native action fixture compile failed: $Name"}
  return $exe
 }
 foreach($suite in @('actions','hit','wall')){
  $exe=Build-Fixture $suite $suite ''
  & $exe
  if($LASTEXITCODE){throw "Native action regression failed: $suite"}
 }
 & (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') agent/tests/prop_action_native_contract_test.py
 if($LASTEXITCODE){throw 'Cooked native action contract failed'}
 if($FaultControls){
  $sources=@{actions=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_action_sync.cpp'));
   hit=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/engine/hit_result.cpp'));
   wall=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/wall_build.cpp'))}
  $controls=@(
   @{Name='live-tool';Suite='actions';Case='damage-preparation';Edits=@(@{Before='IsSwingValid(coop::hand_item::HeldItem(senderSlot),p.damage)';After='true'})},
   @{Name='target-key';Suite='actions';Case='damage-target-key';Edits=@(@{Before='return author() && liveKey==key;';After='return author();'})},
   @{Name='native-function';Suite='actions';Case='damage-preparation';Edits=@(@{Before='function.Get() && owner.Get()';After='owner.Get()'})},
   @{Name='client-input';Suite='actions';Case='client-input';Edits=@(@{Before='bool Current()const {';After='bool Current()const { return true;'})},
   @{Name='native-getter';Suite='actions';Case='saw-native-getter';Edits=@(@{Before='frame.GetRaw(L"return",&blocked,sizeof(blocked)) && prepared() && !blocked';After='frame.GetRaw(L"return",&blocked,sizeof(blocked)) && prepared()'})},
   @{Name='location-binding';Suite='hit';Case='required-bindings';Edits=@(@{Before='if(!f.Set<FVector>(L"Location", location) || !current() || !f.Set<FVector>(L"ImpactPoint", location) || !current())return false;';After='f.Set<FVector>(L"Location", location);f.Set<FVector>(L"ImpactPoint", location);if(!current())return false;'})},
   @{Name='field-owner';Suite='hit';Case='field-retirement';Edits=@(@{Before='return caller() && source.Current() && destination.Get() && type.Get() && R::ClassOf(object)==cls;';After='return caller() && source.Current();'})},
   @{Name='struct-identity';Suite='hit';Case='schema-and-result';Edits=@(@{Before='R::PropertyInnerStruct(frame.function(),param)!=g_hitStruct';After='false'},@{Before='R::PropertyInnerStruct(frame.function(),param)==g_hitStruct';After='true'})},
   @{Name='blocking-result';Suite='hit';Case='schema-and-result';Edits=@(@{Before='!layout.ok || !(hit[layout.blockByte]&layout.blockMask) || !current()';After='!layout.ok || !current()'})},
   @{Name='wall-source';Suite='wall';Case='wall-damage-preparation';Edits=@(@{Before='return callerCurrent() && world==world_identity::Generation()';After='return world==world_identity::Generation()'})}
  )
  $mutantRoot=Join-Path $output 'mutants';New-Item -ItemType Directory -Force -Path $mutantRoot | Out-Null
  foreach($control in $controls){
   $copy=$sources[$control.Suite]
   foreach($edit in $control.Edits){if(-not $copy.Contains($edit.Before)){throw "Missing native action control anchor: $($control.Name)"};$copy=$copy.Replace($edit.Before,$edit.After)}
   $path=Join-Path $mutantRoot ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $exe=Build-Fixture ('fault_'+$control.Name) $control.Suite $path
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($control.Case)+' line ')){throw "Native action control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
 foreach($source in @('coop/props/prop_action_sync','ue_wrap/engine/hit_result','ue_wrap/actors/wall_build')){
  $name=$source.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
  if($LASTEXITCODE){throw "Native action production compile failed: $source"}
 }
}finally{Pop-Location}
exit 0
