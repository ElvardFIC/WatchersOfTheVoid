param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2127'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    foreach ($suite in @('radio_wrapper_2127','gravity_2124')) {
        & cl /nologo /std:c++20 /EHsc /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\$suite.obj" "/Fe$output\$suite.exe" "agent\tests\${suite}_test.cpp"
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $suite" }
        & "$output\$suite.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $suite" }
    }
    & $Python agent\tests\blueprint_2127_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Native gun diagnosis validation failed' }
    $cases=@(
        @{Name='catalog_stride';Suite='radio_wrapper_2127';File='ue_wrap/world/radio_player.cpp';Before='g_stride=R::FindParamSize(R::FindDispatchFunctionCached(cls,L"openLink"),L"data");';After='g_stride=R::StructSize(type);'},
        @{Name='orphan_input';Suite='gravity_2124';File='coop/items/gravity_motion.cpp';Before='players::Registry::Get().IsPuppet(R::OuterOf(c.object))';After='false'}
    )
    foreach ($case in $cases) {
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_test.cpp')))
        $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
        # The wrapper harness also compiles the real ParamFrame implementation.
        $harness=$harness.Replace('../../src/votv-coop/src/ue_wrap/core/call.cpp',(($root -replace '\\','/')+'/src/votv-coop/src/ue_wrap/core/call.cpp'))
        $copy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($copy,$harness)
        & cl /nologo /std:c++20 /EHsc /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\$($case.Name).obj" "/Fe$output\$($case.Name).exe" $copy
        if ($LASTEXITCODE -ne 0) { throw "Mutation compilation failed: $($case.Name)" }
        & "$output\$($case.Name).exe"
        if ($LASTEXITCODE -eq 0) { throw "Mutation was not detected: $($case.Name)" }
        Write-Host "PASS: negative control rejected: $($case.Name)"
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
