$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/fork-watch-cache-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source=Get-Content -LiteralPath (Join-Path $root 'src/votv-coop/src/ue_wrap/core/script_gate_names.cpp') -Raw
$start=$source.IndexOf('inline bool SameLiteral(')
$end=$source.IndexOf('// The whole table, not the probe chain:')
if($start -lt 0 -or $end -le $start){throw 'Production name-watch extraction missing'}
$body="namespace ue_wrap::script_gate { using namespace detail;`n"+$source.Substring($start,$end-$start)+"`n}"
[IO.File]::WriteAllText((Join-Path $output 'fork_watch_cache.inc'),$body,[Text.UTF8Encoding]::new($false))
& cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$root/src/votv-coop/include" "/I$output" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/fork_watch_cache_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Fork watch-cache compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Fork watch-cache regression failed'}
$reflection=Get-Content -LiteralPath (Join-Path $root 'src/votv-coop/src/ue_wrap/core/reflection.cpp') -Raw
$hash=[regex]::Match($reflection,'(?ms)^uint64_t Fnv1a\(.*?^\}').Value
$cache=[regex]::Match($reflection,'(?ms)^struct CachedCdo \{.*?^std::unordered_map<uint64_t, CachedCdo> g_cdoCache;').Value
$lookup=[regex]::Match($reflection,'(?ms)^void\* FindClassDefaultObject\(.*?^\}').Value
if(-not $hash -or -not $cache -or -not $lookup){throw 'Production CDO cache extraction missing'}
[IO.File]::WriteAllText((Join-Path $output 'fork_cdo_cache.inc'),($hash+"`n"+$cache+"`n"+$lookup),[Text.UTF8Encoding]::new($false))
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/Fo$output/cdo.obj" "/Fe$output/cdo.exe" "$PSScriptRoot/fork_cdo_cache_test.cpp"
if($LASTEXITCODE){throw 'Fork CDO cache compile failed'}
& "$output/cdo.exe"
if($LASTEXITCODE){throw 'Fork CDO cache regression failed'}
