$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2129_audit'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . .\_toolchain\env.ps1
    function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
        & cl /nologo /std:c++20 /EHsc /Gy /I agent\tests\stubs /I src\votv-coop\include "/Fo$output\$name.obj" "/Fe$output\$name.exe" $source /link /OPT:REF cabinet.lib
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $name" }
        & "$output\$name.exe" "$output\slots_$name"
        $rc=$LASTEXITCODE
        if ($negative) {
            if ($rc -ne 1) { throw "Negative control must fail an assertion (1), got $rc for $name" }
            Write-Host "PASS: negative control rejected: $name"
        } elseif ($rc -ne 0) { throw "Test failed: $name" }
    }
    Compile-AndRun 'save_receive' 'agent\tests\save_receive_2129_audit_test.cpp' $false
    $cases=@(
        @{Name='short_read';File='slot_file_read.cpp';Before='return f.good() && f.gcount() == n;';After='return f.good() || f.eof();'},
        @{Name='unchecked_close';File='save_transfer.cpp';Before='f.close();';After='/* destructor close ignores flush failure */'},
        @{Name='uncaught_alloc';File='save_transfer.cpp';Before='catch (const std::bad_alloc&)';After='catch (const int&)'},
        @{Name='empty_save_dir';File='save_transfer.cpp';Before='if (dir.empty()) {';After='if (false) {'}
    )
    foreach ($case in $cases) {
        $relative='coop/save/'+$case.File
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$relative)))
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'save_receive_2129_audit_test.cpp'))
        $harness=$harness.Replace('../../src/votv-coop/src/'+$relative,($mutant -replace '\\','/'))
        $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
        $copy=Join-Path $output ($case.Name+'_test.cpp')
        [IO.File]::WriteAllText($copy,$harness)
        Compile-AndRun $case.Name $copy $true
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
