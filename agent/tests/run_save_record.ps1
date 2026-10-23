param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/save-record-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $source=(Get-Content 'src/votv-coop/src/ue_wrap/actors/save_record.cpp' -Raw).Replace("`r`n","`n")
 $context=[regex]::Match($source,'(?ms)^uint64_t g_codecEpoch.*?(?=^// Class ->)').Value
 [IO.File]::WriteAllText((Join-Path $output 'context.inc'),$context,[Text.UTF8Encoding]::new($false))
 $blocks=foreach($name in @('EnsureConventionChecked','SpliceElements','SpliceGroups','CaptureRecord','ApplyRecord','ApplyRecordWhile','CaptureRecordVia','ApplyRecordVia','ApplyRecordViaWhile','ResetCodecCache')){
  $prefix=if($name -in @('SpliceElements','SpliceGroups')){'template <class [^>]+>\r?\nvoid '}elseif($name -eq 'ResetCodecCache'){'void '}else{'bool '}
  $match=[regex]::Match($source,"(?ms)^$prefix$name\(.*?^\}").Value
  if(-not $match -and $name -notin @('ApplyRecordWhile','ApplyRecordViaWhile')){throw "Missing production function: $name"}
  if($match){$match}
 }
 $functions=$blocks -join "`n"
 function Check([string]$Name,[string]$Body,[bool]$Negative){
  [IO.File]::WriteAllText((Join-Path $output 'functions.inc'),$Body,[Text.UTF8Encoding]::new($false))
  $guarded=[int][bool]$context
  $rawGuarded=[int]$source.Contains('bool ApplyRecordViaWhile(')
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/DGUARDED_CODEC=$guarded" "/DRAW_CODEC=$rawGuarded" /I src/votv-coop/include "/Fo$output/$Name.obj" "/Fe$output/$Name.exe" (Join-Path $PSScriptRoot 'save_record_guard_test.cpp') /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Codec test compile failed: $Name"}
  $savedPreference=$ErrorActionPreference
  try{$ErrorActionPreference='Continue';$report=& "$output/$Name.exe" 2>&1 | Out-String;$code=$LASTEXITCODE}
  finally{$ErrorActionPreference=$savedPreference}
  Write-Output $report.TrimEnd()
  if($Negative){if($code -ne 1 -or $report -notmatch 'FAIL \d+:'){throw "Control did not fail its assertion: $Name"};Write-Output "PASS expected-failure control: $Name"}
  elseif($code -ne 0){throw 'Codec guard regression failed'}
 }
 Check 'codec' $functions $false
 if($FaultControls){
  foreach($name in @('stale-base-capture','stale-final-dispatch','stale-capture-result','stale-convention-read','ignores-caller','retired-cache','premature-convention',
   'raw-stale-dispatch','raw-ignores-caller','raw-stale-capture')){
   $rawApply=[regex]::Match($functions,'(?ms)^bool ApplyRecordViaWhile\(.*?^\}').Value
   $mutant=switch($name){
    'stale-base-capture' {$functions.Replace('if (!CallForRecord(actor, baseFn.Get(), base) || !current())','if (!CallForRecord(actor, baseFn.Get(), base))')}
    'stale-final-dispatch' {$functions.Replace("if(!current())return false;`n    return Call(actor, f)",'return Call(actor, f)')}
    'stale-capture-result' {$functions.Replace('return CallForRecord(actor, getFn.Get(), out) && current();','return CallForRecord(actor, getFn.Get(), out);')}
    'stale-convention-read' {$functions.Replace("if(!current())return false;`n    if (!CallForRecord(actor, leafFn.Get(), leaf))",'if (!CallForRecord(actor, leafFn.Get(), leaf))')}
    'ignores-caller' {$functions.Replace('&& guard();',';')}
    'retired-cache' {$functions.Replace('++g_codecEpoch;','/* unchanged codec epoch */')}
    'premature-convention' {$functions.Replace('it->second.conventionChecking = true;','it->second.conventionChecking = true;it->second.conventionChecked = true;')}
    'raw-stale-dispatch' {$functions.Replace($rawApply,$rawApply.Replace(' || !current())return false;',')return false;'))}
    'raw-ignores-caller' {$functions.Replace($rawApply,$rawApply.Replace('&& guard();',';'))}
    'raw-stale-capture' {$functions.Replace('return current() && CallForRecord(actor, fn.Get(), out) && current();','return current() && CallForRecord(actor, fn.Get(), out);')}
   }
   if($mutant -eq $functions){throw "Missing codec mutation site: $name"}
   Check $name $mutant $true
  }
  [IO.File]::WriteAllText((Join-Path $output 'functions.inc'),$functions,[Text.UTF8Encoding]::new($false))
 }
 if($CompileSources){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/save_record.obj" src/votv-coop/src/ue_wrap/actors/save_record.cpp
  if($LASTEXITCODE -ne 0){throw 'Production codec compile failed'}
 }
}finally{Pop-Location}
exit 0
