param([string]$Python='python')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research\tests_2126'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    . .\_toolchain\env.ps1
    foreach ($name in @('native_result','power_door')) {
        & cl /nologo /std:c++20 /EHsc /Gy /Gw /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\$name.obj" "/Fe$output\$name.exe" "agent\tests\${name}_2126_test.cpp" /link /OPT:REF /OPT:ICF
        if ($LASTEXITCODE -ne 0) { throw "Harness compilation failed: $name" }
        & "$output\$name.exe"
        if ($LASTEXITCODE -ne 0) { throw "Harness failed: $name" }
    }
    & $Python agent\tests\blueprint_2126_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Cooked Blueprint checks failed' }
    $cases=@(
        @{Name='native_overread';Suite='native_result_2126';File='ue_wrap/core/ufunction_hook.cpp';Before='if (result && s.resultBytes==sizeof(void*))';After='if (result)'},
        @{Name='door_prediction';Suite='power_door_2126';File='coop/world/power_grid.cpp';Before='e.door=d.value ? 2 : 1;';After='/* prediction removed */'},
        @{Name='cover_owner';Suite='service_panel_2124';File='coop/world/service_panel_sync.cpp';Before='(!remote_prop::IsActorUnderAnyDrive(cover) || remote_prop::DrivenActor(slot)==cover)';After='true'}
    )
    foreach ($case in $cases) {
        $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
        if (-not $source.Contains($case.Before)) { throw "Mutation target changed: $($case.Name)" }
        $mutant=Join-Path $output ($case.Name+'_source.cpp')
        [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
        $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_test.cpp')))
        $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
        $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
        & cl /nologo /std:c++20 /EHsc /Gy /Gw /I agent\tests\stubs /I src\votv-coop\include /I src\votv-coop\src "/Fo$output\$($case.Name).obj" "/Fe$output\$($case.Name).exe" $copy /link /OPT:REF /OPT:ICF
        if ($LASTEXITCODE -ne 0) { throw "Mutation compile failed: $($case.Name)" }
        & "$output\$($case.Name).exe"
        if ($LASTEXITCODE -eq 0) { throw "Mutation not detected: $($case.Name)" }
        Write-Host "PASS: rejected negative control $($case.Name)"
    }
    $global:LASTEXITCODE=0
} finally { Pop-Location }
