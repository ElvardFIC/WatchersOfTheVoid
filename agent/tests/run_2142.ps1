$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2142'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if (Test-Path ../_toolchain/env.ps1) { . ../_toolchain/env.ps1 }
 elseif (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
  throw 'Run in an x64 MSVC developer shell, or provide _toolchain/env.ps1.'
 }
 foreach($name in @('pending_birth_2130','item_record_reentry','food_publish_2142')) {
  $stub=if($name -eq 'food_publish_2142'){'agent/tests/food_stubs'}else{'agent/tests/item_record_stubs'}
  & cl /nologo /std:c++20 /EHsc /Gy /I $stub /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" "agent/tests/${name}_test.cpp" /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  if($LASTEXITCODE -ne 0){throw "Test failed: $name"}
 }
} finally {Pop-Location}
