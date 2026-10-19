$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2145'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 foreach($name in @('cord','extinguisher','flashlight')) {
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_2145_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
 }
}finally{Pop-Location}
