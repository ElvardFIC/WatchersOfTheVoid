param([switch]$WrapperOnly,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/container-recovery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $reflection=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/core/reflection_props.cpp'))
 $offset=[regex]::Match($reflection,'(?ms)^int32_t InstanceOffset::Of\(.*?^\}').Value
 if(-not $offset){throw 'Production InstanceOffset seam missing'}
 [IO.File]::WriteAllText((Join-Path $output 'instance_offset_production.inc'),("namespace ue_wrap::reflection {`n"+$offset+"`n}"))
 & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$output" /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/container_refund_wrapper_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Container refund wrapper fixture compile failed'}
 & "$output/wrapper.exe"
 if($LASTEXITCODE){throw 'Container refund wrapper regression failed'}
 & "$env:USERPROFILE/.local/bin/python3.15.exe" agent/tests/container_recovery_native_test.py
 if($LASTEXITCODE){throw 'Container recovery cooked native contract failed'}
 if(-not $WrapperOnly){
  function Build-Controller([string]$Name,[string]$SourcePath){
   $flags=@('/nologo','/std:c++20','/EHsc','/utf-8','/Gy','/I','agent/tests/container_recovery_stubs','/I','src/votv-coop/include')
   if($SourcePath){$header=Join-Path $output ($Name+'_source.h');[IO.File]::WriteAllText($header,('#define CONTAINER_RECOVERY_SOURCE "'+$SourcePath.Replace('\','/')+'"'+[Environment]::NewLine));$flags+="/FI$header"}
   $exe=Join-Path $output ($Name+'.exe')
   & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" agent/tests/container_recovery_test.cpp /link /OPT:REF | Out-Host
   if($LASTEXITCODE){throw "Container recovery controller compile failed: $Name"}
   return $exe
  }
  $controller=Build-Controller 'controller' ''
  & $controller
  if($LASTEXITCODE){throw 'Container recovery controller regression failed'}
  if($FaultControls){
   $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/items/container_extract_recovery.cpp'))
   $mutations=@(
    @{Name='refund-refusal';Case='refund-retry';Edits=@(@{Before='if(!committed)return;';After='if(false)return;'})},
    @{Name='carrier-quarantine';Case='destroy-retry';Edits=@(@{Before='if(!current() || row->actor.Get())return;';After='if(!current())return;'})},
    @{Name='partial-spawn';Case='partial-spawn';Edits=@(@{Before='if(!actor && layout.begun>=0)';After='if(false)'})},
    @{Name='ambiguous-debit';Case='fault-unwind';Edits=@(@{Before='if(!row->finished && !row->actor.Raw())row->uncertain=true;';After='if(!row->finished && !row->actor.Raw())row->absent=true;'})},
    @{Name='reservation-cap';Case='reservation-cap';Edits=@(@{Before='g_debts.size()>=kDebtCap';After='false'})},
    @{Name='nested-install';Case='nested-install';Edits=@(@{Before='if(g_epoch!=epoch+1 || g_session)return;';After='if(false)return;'})},
    @{Name='accepted-credit';Case='successful-pocket';Edits=@(@{Before='accepted && *accepted && row->restored';After='false'},@{Before='row->pocketSeen && row->restored && layout.pocket>=0 && call.locals[layout.pocket]';After='false'})}
   )
   $mutantRoot=Join-Path $output 'mutants';New-Item -ItemType Directory -Force -Path $mutantRoot | Out-Null
   foreach($mutation in $mutations){
    $copy=$source
    foreach($edit in $mutation.Edits){if(-not $copy.Contains($edit.Before)){throw "Missing recovery control anchor: $($mutation.Name)"};$copy=$copy.Replace($edit.Before,$edit.After)}
    $path=Join-Path $mutantRoot ($mutation.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
    $exe=Build-Controller ('fault_'+$mutation.Name) $path
    $savedPreference=$ErrorActionPreference
    try{$ErrorActionPreference='Continue';$report=& $exe $mutation.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
    [IO.File]::WriteAllText((Join-Path $mutantRoot ($mutation.Name+'.log')),$report)
    if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($mutation.Case)+' line ')){throw "Recovery control did not reach its assertion: $($mutation.Name)"}
    Write-Host ('PASS fault '+$mutation.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
   }
  }
 }
 foreach($source in @('coop/items/container_extract_recovery','coop/items/inventory_pickup_sync','ue_wrap/actors/container_inventory','ue_wrap/actors/save_record','coop/props/container_contents_sync')){
  $name=$source.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$source.cpp"
  if($LASTEXITCODE){throw "Recovery production-header compile failed: $source"}
 }
}finally{Pop-Location}
exit 0
