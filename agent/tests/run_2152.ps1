param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2152'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($name in @('hook_retrieve_2152','skin_previews_2151')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/hook_2152_stubs /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Test compile failed: $name"}
  if($name -eq 'skin_previews_2151'){& "$output/$name.exe" (Join-Path $output ([guid]::NewGuid().ToString()))}
  else{& "$output/$name.exe"}
  if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
 }
 if($CompileSources){
  foreach($file in @('coop/items/hook_retrieve','ue_wrap/actors/hook','coop/items/hook_sync','coop/items/hook_anchor','coop/items/hook_prop_claim','coop/dispatch/event_dispatch_world')) {
   $name=$file -replace '/','_'
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
