param([switch]$InventoryOnly,[switch]$CollectOnly,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2151'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $source=Get-Content 'src/votv-coop/src/coop/props/remote_prop.cpp' -Raw
 $block=[regex]::Match($source,'(?ms)^struct HoldGate \{.*?^bool IsRejectedRelease\(.*?^\}').Value
 if(-not $block){throw 'Production drive helpers missing'}
 [IO.File]::WriteAllText((Join-Path $output 'drive.h'),$block,[Text.UTF8Encoding]::new($false))
 $release=$source.Substring($source.IndexOf('void OnRelease('))
 if($release.IndexOf('if (IsRejectedRelease(') -lt 0 -or $release.IndexOf('if (IsRejectedRelease(') -gt $release.IndexOf('coop::pvp::NoteThrow(')){throw 'Conflict guard missing or too late'}
 foreach($entry in @(@('coop/props/prop_save_data','bool PrepareForCapture','prepare.h'),@('coop/items/inventory_pickup_sync','ue_wrap::script_gate::Verdict OnPocketPre','pocket.h'))) {
  $body=[regex]::Match((Get-Content "src/votv-coop/src/$($entry[0]).cpp" -Raw),"(?ms)^$($entry[1])\(.*?^\}").Value
  if(-not $body){throw "Production function missing: $($entry[1])"}
  [IO.File]::WriteAllText((Join-Path $output $entry[2]),$body,[Text.UTF8Encoding]::new($false))
 }
 $tests=if($CollectOnly){@('handoff_2151')}elseif($InventoryOnly){@('pocket_state_2151')}else{@('handoff_2151','skin_previews_2151','flashlight_2145','pocket_state_2151')}
 function Compile-Test([string]$TestName,[string]$TestSource){
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/handoff_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$TestName.obj" "/Fe$output/$TestName.exe" $TestSource /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $TestName"}
 }
 foreach($name in $tests) {
  Compile-Test $name (Join-Path $PSScriptRoot "${name}_test.cpp")
  if($name -eq 'skin_previews_2151') {& "$output/$name.exe" (Join-Path $output ([guid]::NewGuid().ToString()))}
  else {& "$output/$name.exe"}
  if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
 }
 if($FaultControls){
  if(-not $InventoryOnly){throw 'Pickup controls require -InventoryOnly'}
  $pocketPath=Join-Path $output 'pocket.h'
  $pocket=[IO.File]::ReadAllText($pocketPath)
  try {
   foreach($name in @('failed-restore-pickup','invalidated-restore-pickup')){
    $mutant=if($name -eq 'failed-restore-pickup'){$pocket.Replace('if (extracted == detail::ExtractRestore::Failed) {','if (false) {')}
     else {$pocket.Replace('if(extracted == detail::ExtractRestore::Invalidated)','if(false)')}
    if($mutant -eq $pocket){throw "Missing pickup mutation site: $name"}
    [IO.File]::WriteAllText($pocketPath,$mutant,[Text.UTF8Encoding]::new($false))
    Compile-Test $name (Join-Path $PSScriptRoot 'pocket_state_2151_test.cpp')
    & "$output/$name.exe"
    if($LASTEXITCODE -ne 1){throw "Pickup control did not fire: $name"}
    Write-Output "PASS expected-failure control: $name"
   }
  } finally {[IO.File]::WriteAllText($pocketPath,$pocket,[Text.UTF8Encoding]::new($false))}
 }
 $files=@('coop/items/inventory_pickup_sync','coop/props/prop_save_data')
 if($CollectOnly){$files=@('coop/items/coingun_collect')}
 elseif(-not $InventoryOnly){$files+=@('coop/items/coingun_collect','coop/items/coingun_sync','coop/player/skin_registry','coop/props/remote_prop','ui/skins_panel')}
 foreach($file in $files) {
  $name=$file -replace '/','_'
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include /I src/votv-coop/third_party/imgui "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
  if($LASTEXITCODE -ne 0){throw "Source compilation failed: $file"}
 }
}finally{Pop-Location}
