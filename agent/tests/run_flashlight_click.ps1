param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/flashlight-click-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/flashlight_click_stubs /I src/votv-coop/include "/Fo$output/flashlight_click.obj" "/Fe$output/flashlight_click.exe" agent/tests/flashlight_click_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Flashlight click compile failed'}
 & "$output/flashlight_click.exe"
 if($LASTEXITCODE){throw 'Flashlight click regression failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/player/flashlight_click_sound.cpp'))
  $controls=@(
   @{Name='slot';Changes=@(@('slot>=players::kMaxPeers || ',''))},
   @{Name='puppet';Changes=@(@('registry.IsLocal(actor) || registry.PeerIdOfActor(actor)!=slot','false'),@('!registry.IsLocal(actor) && registry.PeerIdOfActor(actor)==slot','true'))},
   @{Name='lifetime';Changes=@(@('puppet.Current()','true'))},
   @{Name='scope';Changes=@(@('row.serial!=serial || row.world!=world || row.person!=person || !row.puppet.Is(actor)','false'))},
   @{Name='readiness';Changes=@(@('if(!current())return;','row.state=desired;if(!current())return;'))},
   @{Name='sound';Changes=@(@('if(!g_sound.Current()){','if(!g_sound.object.Raw()){'))},
   @{Name='attenuation';Changes=@(@('if(!g_attenuation.Current()){','if(!g_attenuation.object.Raw()){'))},
   @{Name='reentry';Changes=@(@('row.revision==revision && row.busy &&','true &&'))},
   @{Name='position';Changes=@(@('!std::isfinite(position.X)','false'))},
   @{Name='no-fallback';Changes=@(@(' || !g_attenuation.Current()',''),@(' || !attenuation.Current()',''))}
  )
  foreach($control in $controls){
   $mutant=$source;$changes=$control.Changes
   if($changes[0] -is [string]){$changes=@(,$changes)}
   foreach($change in $changes){if(-not $mutant.Contains($change[0])){throw "Missing click control anchor: $($control.Name)"};$mutant=$mutant.Replace($change[0],$change[1])}
   $name='fault_'+$control.Name;$path=Join-Path $output ($name+'.cpp');[IO.File]::WriteAllText($path,$mutant)
   $header=Join-Path $output ($name+'.h');[IO.File]::WriteAllText($header,('#define FLASHLIGHT_CLICK_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/FI$header" /I agent/tests/flashlight_click_stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" agent/tests/flashlight_click_test.cpp /link /OPT:REF
   if($LASTEXITCODE){throw "Click control compile failed: $name"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$name.exe" $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Click control did not reach an assertion: $name"}
   Write-Host ('PASS fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
 if($CompileSources){foreach($file in @('coop/player/flashlight_click_sound','coop/player/item_activate')){
  $name=$file.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE){throw "Flashlight click production compile failed: $file"}
 }}
}finally{Pop-Location}
exit 0
