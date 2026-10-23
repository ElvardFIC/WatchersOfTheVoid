param([string]$Source, [string]$Case, [switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/sat-console-delivery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$SourcePath,[string]$Name) {
  $flags=@('/nologo','/std:c++20','/EHsc','/Gy','/utf-8','/I','agent/tests/sat_console_stubs','/I','src/votv-coop/include')
  if($SourcePath){
   $includePath=[IO.Path]::GetFullPath($SourcePath).Replace('\','/')
   $overrideHeader=Join-Path $output ($Name+'_source_override.h')
   [IO.File]::WriteAllText($overrideHeader,('#define SAT_CONSOLE_SOURCE "'+$includePath+'"'+[Environment]::NewLine))
   $flags += "/FI$overrideHeader"
  }
  $executable=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$executable" agent/tests/sat_console_delivery_test.cpp /link /OPT:REF | Out-Host
  if($LASTEXITCODE -ne 0){throw "Satellite console fixture compile failed: $Name"}
  return $executable
 }
 $executable=Build-Fixture -SourcePath $Source -Name 'console'
 if($Case){& $executable $Case}else{& $executable}
 if($LASTEXITCODE -ne 0){throw 'Satellite console delivery regression failed'}
 if(-not $Source){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/controller_production.obj" src/votv-coop/src/coop/interactables/sat_console_sync.cpp
  if($LASTEXITCODE -ne 0){throw 'Satellite console production-header compile failed'}
 }
 if($FaultControls){
  $sourceFile=Join-Path $root 'src/votv-coop/src/coop/interactables/sat_console_sync.cpp'
  if($Source){$sourceFile=[IO.Path]::GetFullPath($Source)}
  $sourceText=[IO.File]::ReadAllText($sourceFile)
  $mutantRoot=Join-Path $output 'mutants'
  New-Item -ItemType Directory -Force -Path $mutantRoot | Out-Null
  # Each control changes a production-source COPY. A compile failure or an
  # unrelated process failure never counts as an observed regression.
  $mutations=@(
   @{Name='capacity';Case='capacity';Before='constexpr size_t kMaxTerminals   = coop::net::kPeerCapacity - 1;';After='constexpr size_t kMaxTerminals   = 24;'},
   @{Name='recipient';Case='recipient-reset';Before='g_peerKnown[slot] && (g_peerGeneration[slot]!=generation || g_peerGuid[slot]!=guid)';After='false'},
   @{Name='flush';Case='queue-append';Before='!BindPeer(s,slot) || g_flushing[slot]';After='!BindPeer(s,slot)'},
   @{Name='world';Case='source-changes';Before='WI::Generation()==g_world';After='true'},
   @{Name='busy';Case='busy-widget';Before='!s || !g_haveMirrorBusy';After='!s || !g_mirrorBusy'},
   @{Name='history';Case='history-mutation';Before='revision==g_historyRevision';After='true'},
   @{Name='broadcast';Case='broadcast-clear';Before='!g_batching && Clock::now() >= o.nextTry';After='Clock::now() >= o.nextTry'},
   @{Name='command-source';Case='native-command-source';Before='const auto currentRequest=[&]{return Recipient(s,slot,generation,requestGuid,requestEpoch,binding);};';After='const auto currentRequest=[]{return true;};'}
  )
  foreach($mutation in $mutations){
   if(-not $sourceText.Contains($mutation.Before)){throw "Missing fault-control source anchor: $($mutation.Name)"}
   $mutantPath=Join-Path $mutantRoot ($mutation.Name+'.cpp')
   [IO.File]::WriteAllText($mutantPath,$sourceText.Replace($mutation.Before,$mutation.After))
   $control=Build-Fixture -SourcePath $mutantPath -Name ('fault_'+$mutation.Name)
   $savedPreference=$ErrorActionPreference
   try {
    $ErrorActionPreference='Continue'
    $report=& $control $mutation.Case 2>&1 | Out-String
    $status=$LASTEXITCODE
   } finally {$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ($mutation.Name+'.log')),$report)
   if($status -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($mutation.Case)+' line ')){
    throw "Fault control did not fail its regression assertion: $($mutation.Name) (exit $status)"
   }
   Write-Host ('PASS fault '+$mutation.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
} finally {Pop-Location}
exit 0
