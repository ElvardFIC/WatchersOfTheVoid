param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/light-presentation-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
 function Compile-Presentation($exe,$header=''){
  $extra=@();if($header){$extra+="/FI$header"}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include @extra "/Fo$exe.obj" "/Fe$exe" agent/tests/light_presentation_test.cpp /link /OPT:REF
  if($LASTEXITCODE){throw 'Light presentation fixture compile failed'}
 }
 $exe=Join-Path $output 'presentation.exe';Compile-Presentation $exe
 & $exe
 if($LASTEXITCODE){throw 'Light presentation regression failed'}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/native_result.obj" "/Fe$output/native_result.exe" agent/tests/native_result_2126_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Native hook scope fixture compile failed'}
 & "$output/native_result.exe"
 if($LASTEXITCODE){throw 'Native hook scope regression failed'}
 foreach($file in @('ue_wrap/devices/lightswitch','ue_wrap/core/ufunction_hook')){
  $leaf=Split-Path -Leaf $file
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE){throw "Production compile failed: $file"}
 }
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/ue_wrap/devices/lightswitch.cpp')).Replace("`r`n","`n")
  $controls=@(
   @{Name='lamp';Before='!f.Set<bool>(L"isActive", on) || !current()';After='!f.Set<bool>(L"isActive", on)'},
   @{Name='ambient';Before='!f.Set<bool>(L"active", on) || !ready()';After='!f.Set<bool>(L"active", on)'},
   @{Name='binding';Before='*reinterpret_cast<void* const*>(static_cast<const char*>(actor)+offset)==component';After='true'},
   @{Name='metadata';Before='!g_soundFunction.Current() || !g_soundFunction.object.Is(fn)';After='false'},
   @{Name='bounds';Before='return offset>=0 && offset<=frame && bytes<=static_cast<size_t>(frame-offset);';After='return offset>=0;'},
   @{Name='packet';Before='sound.object.Get(),at,attenuation.object.Get(),volume,pitch';After='sound.object.Get(),at,attenuation.object.Get(),*reinterpret_cast<const float*>(params+g_volumeOff),*reinterpret_cast<const float*>(params+g_pitchOff)'},
   @{Name='attenuation';Before='if (!g_clickAttenuation.Current() && !g_buildingAttenuation)';After='if (!g_clickAttenuation.object.Raw() && !g_buildingAttenuation)'},
   @{Name='finite';Before='!std::isfinite(volume) || volume<0 || !std::isfinite(pitch) || pitch<=0';After='false'},
   @{Name='source';Before='source==g_replayedSwitch && identity->Current()';After='source==g_replayedSwitch'},
   @{Name='reentry';Before='if (!g_clickAttenuation.Current() && !g_buildingAttenuation)';After='if (!g_clickAttenuation.Current())'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing presentation control anchor: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$source.Replace($control.Before,$control.After))
   $header=Join-Path $output ($control.Name+'.h');[IO.File]::WriteAllText($header,('#define LIGHTSWITCH_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $exe=Join-Path $output ($control.Name+'.exe');Compile-Presentation $exe $header
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Presentation control did not reach assertion: $($control.Name)"}
   Write-Output ('PASS presentation fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
}finally{Pop-Location}
