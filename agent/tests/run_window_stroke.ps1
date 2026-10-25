param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/window-stroke-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 function Build-Fixture([string]$Name,[string]$SourcePath,[string]$Fixture='window_stroke_test',[string]$Macro='WINDOW_STROKE_SOURCE'){
  $flags=@('/nologo','/std:c++20','/EHsc','/utf-8','/Gy','/I','agent/tests/window_stroke_stubs','/I','src/votv-coop/include')
  if($SourcePath){$header=Join-Path $output ($Name+'_source.h');[IO.File]::WriteAllText($header,('#define '+$Macro+' "'+$SourcePath.Replace('\','/')+'"'+[Environment]::NewLine));$flags+="/FI$header"}
  $exe=Join-Path $output ($Name+'.exe')
  & cl @flags "/Fo$output/$Name.obj" "/Fe$exe" "agent/tests/$Fixture.cpp" /link /OPT:REF | Out-Host
  if($LASTEXITCODE){throw "Window stroke fixture compile failed: $Name"}
  return $exe
 }
 $exe=Build-Fixture 'controller' ''
 & $exe
 if($LASTEXITCODE){throw 'Window stroke controller regression failed'}
 $exe=Build-Fixture 'canvas' '' 'window_canvas_test'
 & $exe
 if($LASTEXITCODE){throw 'Window canvas regression failed'}
 & (Join-Path $env:USERPROFILE '.local/bin/python3.15.exe') agent/tests/window_canvas_native_test.py
 if($LASTEXITCODE){throw 'Cooked window canvas contract failed'}
 if($FaultControls){
  $controller=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/interactables/window_stroke_sync.cpp'))
  $controls=@(
   @{Name='refusal';Case='refused-fifo';Before='if (!sent) return;';After='if (false) return;'},
   @{Name='send-budget';Case='send-budget';Before='work<kDrainPerTick';After='work<256'},
   @{Name='serial';Case='source-reset';Before='g_serial != coop::net::session_serial::Current()';After='false'},
   @{Name='validation';Case='validation';Before='!Valid(dab) || ';After=''},
   @{Name='scan-serial';Case='scan-serial';Before='return g_window.Get();';After='return g_window.Raw();'},
   @{Name='replay-window';Case='replay-ownership';Before='identity.Get()!=win || ResolveWindow()!=win';After='false';AdditionalBefore='identity.Get()==win && ResolveWindow()==win';AdditionalAfter='true'},
   @{Name='source-current';Case='send-retirement';Before='g_serial == coop::net::session_serial::Current()';After='true'},
   @{Name='preparation-loss';Case='preparation-retry';Before='if(result==WC::DrawResult::Retry)return;';After='if(result==WC::DrawResult::Retry){g_inbound.pop_front();return;}'}
  )
  foreach($control in $controls){
   if(-not $controller.Contains($control.Before)){throw "Missing window control anchor: $($control.Name)"}
   $copy=$controller.Replace($control.Before,$control.After)
   if($control.ContainsKey('AdditionalBefore')){if(-not $copy.Contains($control.AdditionalBefore)){throw 'Missing second controller anchor'};$copy=$copy.Replace($control.AdditionalBefore,$control.AdditionalAfter)}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $exe=Build-Fixture ('fault_'+$control.Name) $path
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($control.Case)+' line ')){throw "Window control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
  $canvas=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/devices/window_canvas.cpp'))
  $controls=@(
   @{Name='argument-size';Case='draw-bindings';Before='size!=sizeof(T) || ';After=''},
   @{Name='vector-type';Case='vector-layout';Before='!R::NameEquals(R::NameOf(schema),L"Vector2D")';After='false'},
   @{Name='scalar-result';Case='draw-preparation';Before='return bound() && called;';After='return bound();'},
   @{Name='wrapper-epoch';Case='factory-lifetime';Before='epoch==g_epoch';After='true'},
   @{Name='held-source';Case='brush-read-retirement';Before='Read<void*>(player,holding)==held';After='true'},
   @{Name='unknown-signal';Case='signal-gate';Before='if(!valid())return true;';After='if(!valid())return false;'},
   @{Name='factory-pin';Case='factory-lifetime';Before='if(!owned.Pin(material) || !bound() || !result.Current() || !type.Get())';After='if(!owned.Pin(material))'},
   @{Name='draw-admission';Case='native-admission';Before=':DrawResult::Attempted';After=':DrawResult::Retry'},
   @{Name='canvas-binding';Case='scalar-retirement';Before='Read<void*>(window,fields.canv)!=canvas';After='false'}
  )
  foreach($control in $controls){
   if(-not $canvas.Contains($control.Before)){throw "Missing canvas control anchor: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$canvas.Replace($control.Before,$control.After))
   $exe=Build-Fixture ('fault_'+$control.Name) $path 'window_canvas_test' 'WINDOW_CANVAS_SOURCE'
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Case 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch ('FAIL '+[regex]::Escape($control.Case)+' line ')){throw "Canvas control did not reach its assertion: $($control.Name)"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/production.obj" src/votv-coop/src/coop/interactables/window_stroke_sync.cpp
 if($LASTEXITCODE){throw 'Window stroke production compile failed'}
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/canvas-production.obj" src/votv-coop/src/ue_wrap/devices/window_canvas.cpp
 if($LASTEXITCODE){throw 'Window canvas production compile failed'}
}finally{Pop-Location}
exit 0
