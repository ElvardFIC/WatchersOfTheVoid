$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$workspace = Split-Path -Parent $root
. (Join-Path $workspace '_toolchain/env.ps1')
$build = Join-Path $root 'build/votv-coop-2154'
$logs = Join-Path $root 'build/release-2154-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$ErrorActionPreference = 'Continue'
& cmake -S (Join-Path $root 'src/votv-coop') -B $build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_NINJA_CMCLDEPS_RC=OFF `
    "-DCMAKE_TOOLCHAIN_FILE=$workspace/_toolchain/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_INSTALLED_DIR=$root/build/votv-coop/vcpkg_installed" `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static -DVCPKG_MANIFEST_INSTALL=OFF `
    *> (Join-Path $logs 'configure.log')
if ($LASTEXITCODE -ne 0) { throw "Configure failed: $logs/configure.log" }
& cmake --build $build --config Release --target votv-coop --parallel 6 *> (Join-Path $logs 'build.log')
if ($LASTEXITCODE -ne 0) { throw "Build failed: $logs/build.log" }
$dll = Join-Path $build 'main.dll'
$version = [Diagnostics.FileVersionInfo]::GetVersionInfo($dll).ProductVersion
if ($version -ne '0.9.0n b2156') { throw "Expected b2156, got $version" }
Write-Output "PASS: Release built: $version ($dll)"
