param([string]$Source,[string]$Case,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/sat-console-callbacks-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$SourcePath,[string]$Name){
  $flags=@('/nologo','/std:c++20','/EHsc','/Gy','/utf-8','/I','agent/tests/sat_console_stubs','/I','src/votv-coop/include')
  if($SourcePath){
   $includePath=[IO.Path]::GetFullPath($SourcePath).Replace('\','/');$header=Join-Path $output ($Name+'_source_override.h')
   [IO.File]::WriteAllText($header,('#define SAT_CONSOLE_SOURCE "'+$includePath+'"'+[Environment]::NewLine));$flags+="/FI$header"
  }
  $exe=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" agent/tests/sat_console_callbacks_test.cpp /link /OPT:REF | Out-Host
  if($LASTEXITCODE -ne 0){throw "Satellite console callbacks fixture compile failed: $Name"}
  return $exe
 }
 $executable=Build-Fixture -SourcePath $Source -Name 'callbacks'
 if($Case){& $executable $Case}else{& $executable}
 if($LASTEXITCODE -ne 0){throw 'Satellite console callbacks regression failed'}
 if(-not $Source){
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/controller_production.obj" src/votv-coop/src/coop/interactables/sat_console_sync.cpp
  if($LASTEXITCODE -ne 0){throw 'Satellite console production-header compile failed'}
 }
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/wrapper.obj" "/Fe$output/wrapper.exe" agent/tests/sat_console_input_wrapper_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Satellite console native input wrapper fixture compile failed'}
 & "$output/wrapper.exe"
 if($LASTEXITCODE -ne 0){throw 'Satellite console native input wrapper regression failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/native_lifetime.obj" "/Fe$output/native_lifetime.exe" agent/tests/sat_console_native_lifetime_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Console native lifetime compile failed'}
 & "$output/native_lifetime.exe"
 if($LASTEXITCODE -ne 0){throw 'Console native lifetime regression failed'}
 if($FaultControls){
  $sourceFile=Join-Path $root 'src/votv-coop/src/coop/interactables/sat_console_sync.cpp'
  if($Source){$sourceFile=[IO.Path]::GetFullPath($Source)}
  $sourceText=[IO.File]::ReadAllText($sourceFile);$mutantRoot=Join-Path $output 'mutants'
  New-Item -ItemType Directory -Force -Path $mutantRoot | Out-Null
  # Every mutation is confined to a source COPY. Compilation must succeed, and
  # the named scenario must reach its runtime assertion, to count as detected.
  $mutations=@(
   @{Name='consume-refusal';Case='consume-failure';Replacements=@(@{Before='!SC::ConsumeInputWhile(term,current)';After='(SC::ConsumeInputWhile(term,current),false)'})},
   @{Name='input-ownership';Case='consume-reentry';Replacements=@(@{Before='if(input->source.epoch==source.epoch && input->source.term.Is(term))return sg::Verdict::Cancel;';After='if(false)return sg::Verdict::Cancel;'})},
   @{Name='post-source';Case='stale-post';Replacements=@(@{Before='if(!SurfaceCurrent(frame.source))return;';After='if(false)return;'})},
   @{Name='unwind';Case='unwound-frame';Replacements=@(@{Before='!sg::IsInvocationActive(frame.invocation)';After='false'},@{Before='sg::IsInvocationActive(it->invocation)';After='true'})},
   @{Name='clear-revision';Case='display-failure-clear';Replacements=@(@{Before='revision!=g_displayRevision';After='false'})},
   @{Name='display-widget';Case='display-retirement';Replacements=@(@{Before='if(!ok || !SurfaceCurrent(source)) {';After='if(!ok) {'})},
   @{Name='replay-invocation';Case='replay-attribution';Replacements=@(@{Before='!g_replay->claimed && !g_replay->probing';After='!g_replay->probing'})},
   @{Name='replay-preclaim';Case='replay-preclaim';Replacements=@(@{Before='text==replay.text && type==replay.type';After='true'})},
   @{Name='watch-identity';Case='watch-rollback';Replacements=@(@{Before='if (graph) sg::UnwatchName(kRowUbergraph, kTagGraph, OnBodyPre, OnBodyPost);';After='if (graph) sg::UnwatchName(kRowUbergraph, kTagGraph, nullptr, OnBodyPost);'})}
  )
  foreach($mutation in $mutations){
   $copy=$sourceText
   foreach($replacement in $mutation.Replacements){
    if(-not $copy.Contains($replacement.Before)){throw "Missing callback fault-control source anchor: $($mutation.Name)"}
    $copy=$copy.Replace($replacement.Before,$replacement.After)
   }
   $path=Join-Path $mutantRoot ($mutation.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $control=Build-Fixture -SourcePath $path -Name ('fault_'+$mutation.Name)
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $control $mutation.Case 2>&1 | Out-String;$status=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ($mutation.Name+'.log')),$report)
   if($status -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($mutation.Case)+' line ')){throw "Callback fault control did not fail its assertion: $($mutation.Name) (exit $status)"}
   Write-Host ('PASS fault '+$mutation.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
  $wrapperPath=Join-Path $root 'src/votv-coop/src/ue_wrap/desk/sat_console.cpp'
  $wrapperText=[IO.File]::ReadAllText($wrapperPath)
  $layoutCurrent=[regex]::Match($wrapperText.Replace("`r`n","`n"),'(?ms)^bool LayoutCurrent\(\) \{.*?^\}').Value
  if(-not $layoutCurrent){throw 'Missing console LayoutCurrent control anchor'}
  $wrapperMutations=@(
   @{Name='metadata-retry';Before='if(g_resolving)return false;';After='if(g_resolving || g_attempts>=kMaxPostClassAttempts)return false;'},
   @{Name='partial-layout';Before="    ResetLayout();`n`n    void* term = R::FindClass(kTerminalClass);";After="`n    void* term = R::FindClass(kTerminalClass);"},
   @{Name='metadata-currency';Before=$layoutCurrent;After='bool LayoutCurrent() { return g_resolved; }'},
   @{Name='string-capacity';Before='if(view.num<0 || view.max<view.num || size_t(view.num)>limit+1)return false;';After='if(view.num<0 || size_t(view.num)>limit+1)return false;'}
  )
  foreach($mutation in $wrapperMutations){
   $copy=$wrapperText.Replace("`r`n","`n")
   if(-not $copy.Contains($mutation.Before)){throw "Missing wrapper fault-control source anchor: $($mutation.Name)"}
   $copy=$copy.Replace($mutation.Before,$mutation.After)
   $path=Join-Path $mutantRoot ($mutation.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $header=Join-Path $mutantRoot ($mutation.Name+'.h')
   [IO.File]::WriteAllText($header,('#define SAT_CONSOLE_WRAPPER_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $exe=Join-Path $mutantRoot ($mutation.Name+'.exe')
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/FI$header" "/Fo$mutantRoot/$($mutation.Name).obj" "/Fe$exe" agent/tests/sat_console_input_wrapper_test.cpp /link /OPT:REF
   if($LASTEXITCODE -ne 0){throw "Wrapper fault control failed compilation: $($mutation.Name)"}
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe 2>&1 | Out-String;$status=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ($mutation.Name+'.log')),$report)
   if($status -eq 0 -or $report -notmatch 'FAIL wrapper line '){throw "Wrapper fault control did not fail its assertion: $($mutation.Name)"}
   Write-Host ('PASS fault '+$mutation.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
  $nativeControls=@(
   @{Name='binding';Before=$layoutCurrent;After='bool LayoutCurrent() { return g_resolved && std::all_of(g_layoutRefs.begin(),g_layoutRefs.end(),[](const CachedObjRef& ref){return ref.Get()!=nullptr;}); }'},
   @{Name='atlas';Before=' || owner.cls.Get()!=g_layoutRefs[1].Get()';After=''},
   @{Name='factory';Before='context.Current()';After='true'},
   @{Name='init';Before='(!dish || selectedDish.Is(dish))';After='true'},
   @{Name='command';Before='written==line';After='true'},
   @{Name='log';Before='owner.Current()';After='true'},
   @{Name='box-class';Before='textBoxClass.Get() && R::ClassOf(box)==textBoxClass.Get() && ';After=''},
   @{Name='clear';Before='if(logText<0 || !owner.Current() ||';After='if(!owner.Current() ||'},
   @{Name='arguments';Before='bytes<=size_t(size-offset)';After='true'},
   @{Name='panels';Before=' || snapshot.max<snapshot.num';After=''}
  )
  foreach($control in $nativeControls){
   $copy=$wrapperText.Replace("`r`n","`n")
   if(-not $copy.Contains($control.Before)){throw "Missing native console control anchor: $($control.Name)"}
   $path=Join-Path $mutantRoot ('native_'+$control.Name+'.cpp');[IO.File]::WriteAllText($path,$copy.Replace($control.Before,$control.After))
   $header=Join-Path $mutantRoot ('native_'+$control.Name+'.h')
   [IO.File]::WriteAllText($header,('#define SAT_CONSOLE_WRAPPER_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $exe=Join-Path $mutantRoot ('native_'+$control.Name+'.exe')
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/FI$header" "/Fo$mutantRoot/native_$($control.Name).obj" "/Fe$exe" agent/tests/sat_console_native_lifetime_test.cpp /link /OPT:REF
   if($LASTEXITCODE -ne 0){throw "Native console control compile failed: $($control.Name)"}
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Name 2>&1 | Out-String;$status=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $mutantRoot ('native_'+$control.Name+'.log')),$report)
   if($status -eq 0 -or $report -notmatch 'FAIL wrapper line '){throw "Native console control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault native-'+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
}finally{Pop-Location}
exit 0
