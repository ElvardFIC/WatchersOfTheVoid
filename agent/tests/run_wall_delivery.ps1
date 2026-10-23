param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/wall-delivery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$Name,[string]$SourcePath,[string]$Define){
  $flags=@('/nologo','/std:c++20','/EHsc','/utf-8','/Gy','/I','agent/tests/authority_2165_stubs','/I','src/votv-coop/include')
  if($SourcePath){$header=Join-Path $output ($Name+'_source.h');[IO.File]::WriteAllText($header,('#define '+$Define+' "'+$SourcePath.Replace('\','/')+'"'+[Environment]::NewLine));$flags+="/FI$header"}
  $exe=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" agent/tests/wall_delivery_test.cpp /link /OPT:REF | Out-Host
  if($LASTEXITCODE){throw "Wall delivery fixture compile failed: $Name"}
  return $exe
 }
 $exe=Build-Fixture 'delivery' '' ''
 & $exe
 if($LASTEXITCODE){throw 'Wall delivery regression failed'}
 if($FaultControls){
  $controller=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/world/wall_build_sync.cpp'))
  $wrapper=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/wall_build.cpp'))
  $controls=@(
   @{Name='entered-retry';Case='entered-failure';Edits=@(@{Before='!handled && !entered && PairCurrent(p)';After='!handled && PairCurrent(p)'})},
   @{Name='session-serial';Case='context-retirement';Edits=@(@{Before='serial!=g_serial';After='false'},@{Before='g_serial==net::session_serial::Current()';After='true'},@{Before='g_feedSerial==net::session_serial::Current()';After='true'})},
   @{Name='enrollment-reentry';Case='enrollment-reentry';Edits=@(@{Before='if (g_feeding == c.object && g_feedMaterial.Raw()==material) return SG::Verdict::Cancel;';After='if(false)return SG::Verdict::Cancel;'})},
   @{Name='wrapper-source';Case='metadata-retirement';Wrapper=$true;Edits=@(@{Before='return callerCurrent() && world==world_identity::Generation();';After='return world==world_identity::Generation();'})},
   @{Name='component-query';Case='query-retirement';Wrapper=$true;Edits=@(@{Before='componentCurrent()';After='true'})},
   @{Name='watch-short-circuit';Case='registration-retry';Edits=@(@{Before='if(!g_registered[i])g_registered[i]=';After='if(!g_registered[i] && (i==0 || g_registered[i-1]))g_registered[i]='})},
   @{Name='stale-post';Case='stale-post';Edits=@(@{Before='if(Current(body.session,body.epoch) && Host()';After='if(Host()'})},
   @{Name='replay-pair';Case='pre-dispatch-retirement';Edits=@(@{Before='if(!g_feedPair || !PairCurrent(*g_feedPair))return SG::Verdict::Cancel;';After='if(false)return SG::Verdict::Cancel;'})}
  )
  $mutantRoot=Join-Path $output 'mutants';New-Item -ItemType Directory -Force -Path $mutantRoot | Out-Null
  foreach($control in $controls){
   $copy=if($control.Wrapper){$wrapper}else{$controller}
   foreach($edit in $control.Edits){if(-not $copy.Contains($edit.Before)){throw "Missing wall control anchor: $($control.Name)"};$copy=$copy.Replace($edit.Before,$edit.After)}
   $path=Join-Path $mutantRoot ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $define=if($control.Wrapper){'WALL_WRAPPER_SOURCE'}else{'WALL_DELIVERY_SOURCE'}
   $exe=Build-Fixture ('fault_'+$control.Name) $path $define
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($control.Case)+' line ')){throw "Wall control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
 foreach($source in @('coop/world/wall_build_sync','ue_wrap/actors/wall_build')){
  $name=$source.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
  if($LASTEXITCODE){throw "Wall delivery production compile failed: $source"}
 }
}finally{Pop-Location}
exit 0
