$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/atv-companion-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I src/votv-coop/include /I src/votv-coop/src/coop/interactables "/Fo$output/companion.obj" "/Fe$output/companion.exe" agent/tests/atv_companion_test.cpp /link /OPT:REF
 if($LASTEXITCODE){throw 'Companion fixture compile failed'}
 & "$output/companion.exe"
 if($LASTEXITCODE){throw 'Companion regression failed'}
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/atv_sync.obj" src/votv-coop/src/coop/interactables/atv_sync.cpp
 if($LASTEXITCODE){throw 'ATV controller compile failed'}
}finally{Pop-Location}
