param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2129'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . (Join-Path $root '../_toolchain/env.ps1')
    foreach ($suite in @('save_codec_2129','wrappers_2129')) {
        & cl /nologo /std:c++20 /EHsc /I agent\tests\stubs /I src\votv-coop\include "/Fo$output\$suite.obj" "/Fe$output\$suite.exe" "agent\tests\${suite}_test.cpp" /link cabinet.lib
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $suite" }
        & "$output\$suite.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $suite" }
    }
    $cases=@(
        @{Name='missing_bunk_place';Suite='wrappers_2129';File='coop/props/prop_spawn_authoring.cpp';Before='R::NameEquals(R::NameOf(owner), L"prop_bunkbedSleepable_C")';After='false'},
        @{Name='missing_bunk_fold';Suite='wrappers_2129';File='coop/props/prop_spawn_authoring.cpp';Before='R::NameEquals(R::NameOf(owner), L"bed_bunkMattress_C")';After='false'},
        @{Name='missing_crc';Suite='save_codec_2129';File='coop/save/save_blob_codec.cpp';Before='slot_file_read::Crc32(out.data(), out.size()) != crc';After='false'},
        @{Name='missing_size_check';Suite='save_codec_2129';File='coop/save/save_blob_codec.cpp';Before='p.decodedBytes > kMaxBlobBytes';After='false'}
    )
    foreach ($case in $cases) {
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_test.cpp')))
        $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
        $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
        $copy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($copy,$harness)
        & cl /nologo /std:c++20 /EHsc /I agent\tests\stubs /I src\votv-coop\include "/Fo$output\$($case.Name).obj" "/Fe$output\$($case.Name).exe" $copy /link cabinet.lib
        if ($LASTEXITCODE -ne 0) { throw "Mutation compilation failed: $($case.Name)" }
        & "$output\$($case.Name).exe"
        if ($LASTEXITCODE -eq 0) { throw "Mutation was not detected: $($case.Name)" }
        Write-Host "PASS: negative control rejected: $($case.Name)"
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
