param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/food-callbacks-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$Name,[string]$SourcePath){
  $flags=@('/nologo','/std:c++20','/EHsc','/utf-8','/Gy','/I','agent/tests/food_stubs','/I','src/votv-coop/include','/I','src/votv-coop/src/coop/props')
  if($SourcePath){$header=Join-Path $output ($Name+'_source.h');[IO.File]::WriteAllText($header,('#define FOOD_CALLBACK_SOURCE "'+$SourcePath.Replace('\','/')+'"'+[Environment]::NewLine));$flags+="/FI$header"}
  $exe=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" agent/tests/food_callbacks_test.cpp /link /OPT:REF | Out-Host
  if($LASTEXITCODE){throw "Food callbacks compile failed: $Name"}
  return $exe
 }
 $exe=Build-Fixture 'callbacks' ''
 & $exe
 if($LASTEXITCODE){throw 'Food callbacks regression failed'}
 & (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') agent/tests/food_callbacks_native_test.py
 if($LASTEXITCODE){throw 'Cooked food output contract failed'}
 if($FaultControls){
  $controller=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_food_sync.cpp'))
  $controls=@(
   @{Name='untracked-admission';Case='untracked-eat';Edits=@(@{Before='const auto refused=s->role()==coop::net::Role::Client?sg::Verdict::Cancel:sg::Verdict::Run;';After='const auto refused=sg::Verdict::Run;'})},
   @{Name='invalid-condition';Case='invalid-condition';Edits=@(@{Before='if(!b.have || !Valid(now) || now.uses<=0)return refused;';After='if(false)return refused;'})},
   @{Name='nested-eat';Case='nested-eat';Edits=@(@{Before='return row.actor==c.object;';After='return false;'})},
   @{Name='captured-address';Case='key-retirement';Edits=@(@{Before='if(!BiteAddressCurrent(b))return sg::Verdict::Cancel;';After='if(false)return sg::Verdict::Cancel;'})},
   @{Name='session-serial';Case='session-restart-post';Edits=@(@{Before='g_serial==coop::net::session_serial::Current()';After='true'},@{Before='serial==g_serial';After='true'})},
   @{Name='output-scope';Case='output-scope';Edits=@(@{Before='if(!IsClient() || !actor || !callerFunction || !R::IsLive(callerFunction))return false;';After='if(IsClient() && actor && callerFunction)return true;'})},
   @{Name='refilled-final';Case='host-refilled-final';Edits=@(@{Before='live.uses==0';After='true'})},
   @{Name='function-incarnation';Case='metadata-retirement';Edits=@(@{Before='valid() && functionRef.Get() && ownerRef.Get()';After='valid() && ownerRef.Get()'})}
  )
  $mutantRoot=Join-Path $output 'mutants';New-Item -ItemType Directory -Force -Path $mutantRoot | Out-Null
  foreach($control in $controls){
   $copy=$controller
   foreach($edit in $control.Edits){if(-not $copy.Contains($edit.Before)){throw "Missing food control anchor: $($control.Name)"};$copy=$copy.Replace($edit.Before,$edit.After)}
   $path=Join-Path $mutantRoot ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $exe=Build-Fixture ('fault_'+$control.Name) $path
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($control.Case)+' line ')){throw "Food control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
 foreach($source in @('coop/props/prop_food_sync','coop/props/prop_spawn_authoring')){
  $name=$source.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
  if($LASTEXITCODE){throw "Food callbacks production compile failed: $source"}
 }
}finally{Pop-Location}
exit 0
