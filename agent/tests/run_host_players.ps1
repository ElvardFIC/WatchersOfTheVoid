param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/host-players-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$includes=@("/I$checkout/src/votv-coop/include","/I$checkout/build/votv-coop/vcpkg_installed/x64-windows-static/include","/I$output")
function Compile-And-Run([string]$name,[string[]]$sources) {
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes "/Fo$output/" "/Fe$output/$name.exe" @sources /link /OPT:REF
 if($LASTEXITCODE){throw "Host players compile failed: $name"}
 & "$output/$name.exe"
 if($LASTEXITCODE){throw "Host players regression failed: $name"}
}
Compile-And-Run 'selector' @("$PSScriptRoot/host_players_test.cpp","$checkout/src/votv-coop/src/coop/config/config_registry.cpp")
$manager=Get-Content (Join-Path $checkout 'src/votv-coop/src/coop/session/session_manager.cpp') -Raw
$functions=foreach($name in @('HostLobby','AnnounceEnvHostHidden','HostWithSave')) {
 $match=[regex]::Match($manager,"(?ms)^(void|bool) $name\(.*?^\}")
 if(-not $match.Success){throw "Host controller missing: $name"}
 $match.Value.Replace('std::thread(','test::Thread(')
}
[IO.File]::WriteAllText((Join-Path $output 'host_players_flow.inc'),($functions -join "`n"),[Text.UTF8Encoding]::new($false))
Compile-And-Run 'flow' @("$PSScriptRoot/host_players_flow_test.cpp")
Compile-And-Run 'lobby' @("$PSScriptRoot/host_players_lobby_test.cpp")
if($FaultControls) {
 $source=Get-Content (Join-Path $checkout 'src/votv-coop/src/ui/host_player_limit.cpp') -Raw
 foreach($name in @('forgotten-choice','unsaved-choice','unbounded-choice','failed-widget','per-frame-write','legacy-four','env-four')) {
  $mutant=$source;$flow=$functions -join "`n"
  switch($name) {
   'forgotten-choice' {$mutant=$source.Replace('coop::config::ResolveInt(coop::config_registry::rows::net_player_limit)','coop::net::kDefaultPlayers')}
   'unsaved-choice' {$mutant=$source.Replace('if(remember && g_savePending)','if(false)')}
   'unbounded-choice' {$mutant=$source.Replace('const int selected=coop::net::ClampPlayerLimit(players);','const int selected=players;')}
   'failed-widget' {$mutant=$source.Replace('{ClearWidgets();return false;}','{return false;}')}
   'per-frame-write' {$mutant=$source.Replace('if(remember && g_savePending)','if(g_savePending)')}
   'legacy-four' {$flow=$flow.Replace('cfg.playerLimit=playersMax;','/* lost selected legacy-host limit */')}
   'env-four' {$flow=$flow.Replace('/*locked=*/false, playersMax, 8000','/*locked=*/false, 4, 8000')}
  }
  if($mutant -eq $source -and $flow -eq ($functions -join "`n")){throw "Missing host-player control site: $name"}
  $control=Join-Path $output $name
  New-Item -ItemType Directory -Force -Path $control | Out-Null
  if($mutant -ne $source) {
   $inc=(Join-Path $control 'selector.inc').Replace('\','/')
   [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
   $fixture=Join-Path $control 'test.cpp'
   $test=(Join-Path $PSScriptRoot 'host_players_test.cpp').Replace('\','/')
   [IO.File]::WriteAllText($fixture,('#define HOST_PLAYERS_SOURCE "'+$inc+'"'+"`n"+'#include "'+$test+'"'),[Text.UTF8Encoding]::new($false))
   $sources=@($fixture,"$checkout/src/votv-coop/src/coop/config/config_registry.cpp")
  } else {
   [IO.File]::WriteAllText((Join-Path $control 'host_players_flow.inc'),$flow,[Text.UTF8Encoding]::new($false))
   $sources=@("$PSScriptRoot/host_players_flow_test.cpp")
  }
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$control" @includes "/Fo$control/" "/Fe$control/test.exe" @sources /link /OPT:REF
  if($LASTEXITCODE){throw "Host-player control compile failed: $name"}
  & "$control/test.exe"
  if($LASTEXITCODE -ne 1){throw "Host-player control did not detect removed protection: $name"}
  Write-Output "PASS expected-failure control: $name"
 }
}
$cargo=Get-Command cargo -ErrorAction SilentlyContinue
if(-not $cargo) {
 $rust=Join-Path $checkout '../_toolchain/rust'
 $env:CARGO_HOME=Join-Path $rust 'cargo';$env:RUSTUP_HOME=Join-Path $rust 'rustup'
 $cargoPath=Join-Path $env:CARGO_HOME 'bin/cargo.exe'
 if(-not(Test-Path -LiteralPath $cargoPath)){throw 'Rust cargo is required for the master player-limit tests'}
} else {$cargoPath=$cargo.Source}
& $cargoPath test --locked --lib --manifest-path (Join-Path $checkout 'server/Cargo.toml')
if($LASTEXITCODE){throw 'Master player-limit tests failed'}
exit 0
