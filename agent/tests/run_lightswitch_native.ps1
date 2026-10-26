param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/lightswitch-native-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
 function Compile-Lights($exe,$header=''){
  $extra=@();if($header){$extra+="/FI$header"}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include @extra "/Fo$exe.obj" "/Fe$exe" agent/tests/lightswitch_native_test.cpp /link /OPT:REF
  if($LASTEXITCODE){throw 'Lightswitch native fixture compile failed'}
 }
 $exe=Join-Path $output 'native.exe';Compile-Lights $exe
 & $exe
 if($LASTEXITCODE){throw 'Lightswitch native regression failed'}
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/lightswitch.obj" src/votv-coop/src/ue_wrap/devices/lightswitch.cpp
 if($LASTEXITCODE){throw 'Lightswitch production compile failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/ue_wrap/devices/lightswitch.cpp')).Replace("`r`n","`n")
  $controls=@(
   @{Name='root-reload';Before='if (RootLayoutCurrent()) return true;';After='if (g_resolved.load()) return true;'},
   @{Name='switch-reload';Before='if (SwitchLayoutCurrent()) return true;';After='if (g_swResolved.load()) return true;'},
   @{Name='root-fields';Before='const int32_t keyOff=R::FindPropertyOffset(rootCls,L"Key");';After='const int32_t keyOff=(std::max)(0,R::FindPropertyOffset(rootCls,L"Key"));'},
   @{Name='switch-fields';Before='if(keyOff<0 || objectsOff<0 ||';After='if(objectsOff<0 ||'},
   @{Name='root-mask';Before='[g_isActiveOff] & g_isActiveMask';After='[g_isActiveOff] & 0xFF'},
   @{Name='switch-mask';Before='[g_swAOff] & g_swAMask';After='[g_swAOff] & 0xFF'},
   @{Name='breaker-mask';Before='[g_breakerOff] & g_breakerMask';After='[g_breakerOff] & 0xFF'},
   @{Name='links';Before='snapshot.max<snapshot.num';After='false'},
   @{Name='group-calls';Before='!f.Set<void*>(L"owner",root)';After='false'},
   @{Name='use-lifetime';Before='if(!EnsureReplaySound() || !identity.Current() || !SwitchLayoutCurrent())return false;';After='if(!EnsureReplaySound() || !SwitchLayoutCurrent())return false;'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing lightswitch control anchor: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $header=Join-Path $output ($control.Name+'.h');[IO.File]::WriteAllText($header,('#define LIGHTSWITCH_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $exe=Join-Path $output ($control.Name+'.exe');Compile-Lights $exe $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Lightswitch control did not reach assertion: $($control.Name)"}
   Write-Output ('PASS light fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
}finally{Pop-Location}
