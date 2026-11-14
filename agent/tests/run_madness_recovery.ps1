$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/madness-recovery-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
 foreach($test in @('recovery','presentation')){
  $extra=@();if($test -eq 'recovery'){$extra=@('/DTEST_STATE_MAX_PEERS=32','/I','agent/tests/state_2145_stubs','/I','agent/tests/text_drone_stubs','/I','agent/tests/stubs')}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @extra /I src/votv-coop/include "/Fo$output/$test.obj" "/Fe$output/$test.exe" "agent/tests/madness_${test}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE){throw "Madness fixture compile failed: $test"}
  & "$output/$test.exe"
  if($LASTEXITCODE){throw "Madness fixture failed: $test"}
 }
 foreach($source in @('coop/world/madness_sync','ue_wrap/world/madness')){
  $leaf=$source.Replace('/','_')
  & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$leaf.obj" "src/votv-coop/src/$source.cpp"
  if($LASTEXITCODE){throw "Madness production compile failed: $source"}
 }
}finally{Pop-Location}
