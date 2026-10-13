param([switch]$CompileSources)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2165'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/authority_2165_stubs /I src/votv-coop/include "/Fo$output/authority.obj" "/Fe$output/authority.exe" agent/tests/authority_2165_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Authority test compile failed'}
 & "$output/authority.exe"
 if($LASTEXITCODE -ne 0){throw 'Authority test failed'}
 if($CompileSources){
  foreach($file in @('event_schedule_authority','event_fire_sync','spawn_authority')) {
   & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$file.obj" "src/votv-coop/src/coop/world/$file.cpp"
   if($LASTEXITCODE -ne 0){throw "Source compile failed: $file"}
  }
 }
}finally{Pop-Location}
