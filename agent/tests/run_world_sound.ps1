param([switch]$CompileSources)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root 'build/world-sound-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        $environment = Join-Path $root 'build/local-msvc.ps1'
        if (-not (Test-Path -LiteralPath $environment)) { $environment = Join-Path $root '../_toolchain/env.ps1' }
        . $environment
    }
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/world_sound_stubs /I src/votv-coop/include "/Fo$output/world_sound.obj" "/Fe$output/world_sound.exe" agent/tests/world_sound_test.cpp /link /OPT:REF
    if ($LASTEXITCODE -ne 0) { throw 'World sound test compile failed' }
    & "$output/world_sound.exe"
    if ($LASTEXITCODE -ne 0) { throw 'World sound test failed' }
    if ($CompileSources) {
        foreach ($file in @('coop/world/world_sound_sync', 'coop/world/event_cue_sync', 'coop/session/subsystems')) {
            $name = $file -replace '/', '_'
            & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/$name.obj" "src/votv-coop/src/$file.cpp"
            if ($LASTEXITCODE -ne 0) { throw "Source compile failed: $file" }
        }
    }
} finally { Pop-Location }
