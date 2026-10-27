param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2125'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    & cl /nologo /std:c++20 /EHsc /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\radio.obj" "/Fe$output\radio.exe" agent\tests\radio_2125_test.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Radio harness compilation failed' }
    & "$output\radio.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Radio harness failed' }
    & $Python agent\tests\blueprint_2125_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Radio cooked Blueprint validation failed' }
    $sourcePath=Join-Path $root 'src\votv-coop\src\coop\world\radio_sync.cpp'
    $source=[IO.File]::ReadAllText($sourcePath)
    $cases=@(
        @{Name='prediction';Before='if (Predicted(id)) return;';After='/* prediction fence removed */'},
        @{Name='automatic_end';Before='? SG::Verdict::Cancel : SG::Verdict::Run;';After='? SG::Verdict::Run : SG::Verdict::Run;'},
        @{Name='retry_bound';Before='now-attempt.since<15000';After='true'}
    )
    foreach ($case in $cases) {
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'radio_2125_test.cpp'))
        $harness=$harness.Replace('../../src/votv-coop/src/coop/world/radio_sync.cpp',($mutant -replace '\\','/'))
        $testCopy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($testCopy,$harness)
        & cl /nologo /std:c++20 /EHsc /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\$($case.Name).obj" "/Fe$output\$($case.Name).exe" $testCopy
        if ($LASTEXITCODE -ne 0) { throw "Mutation compilation failed: $($case.Name)" }
        & "$output\$($case.Name).exe"
        if ($LASTEXITCODE -eq 0) { throw "Mutation was not detected: $($case.Name)" }
        Write-Host "PASS: negative control rejected: $($case.Name)"
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
