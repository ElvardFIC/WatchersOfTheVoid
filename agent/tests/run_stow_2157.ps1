$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2157'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 $source=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/coop/props/prop_save_data.cpp'))
 $first=$source.IndexOf('void DeclareClassOwnedElsewhere(')
 $last=$source.IndexOf('namespace {', $first)
 if($first -lt 0 -or $last -le $first){throw 'Coverage function boundaries missing'}
 [IO.File]::WriteAllText((Join-Path $output 'coverage.h'),$source.Substring($first,$last-$first))
 $codec=[IO.File]::ReadAllText((Join-Path $root 'src/votv-coop/src/ue_wrap/actors/save_record.cpp'))
 $classField=[regex]::Match($codec,'(?ms)^bool WriteClassField\(.*?^\}').Value
 if(-not $classField){throw 'Production class restoration helper missing'}
 [IO.File]::WriteAllText((Join-Path $output 'class_field.h'),$classField)
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/state_2145_stubs /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include "/Fo$output/stow.obj" "/Fe$output/stow.exe" agent/tests/stow_2157_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Stow test compile failed'}
 & "$output/stow.exe"
 if($LASTEXITCODE -ne 0){throw 'Stow test failed'}
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/prop_save_data.obj" src/votv-coop/src/coop/props/prop_save_data.cpp
 if($LASTEXITCODE -ne 0){throw 'Save-data source compile failed'}
}finally{Pop-Location}
