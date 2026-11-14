$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $root ('build/build-order-check/fixture-'+[guid]::NewGuid().ToString('N'))
$repo=Join-Path $fixture 'repo'
foreach($dir in @('package','agent/tests','.github/ci','src/votv-coop/src','src/votv-coop/include','build/release')){
 New-Item -ItemType Directory -Force -Path (Join-Path $repo $dir) | Out-Null
}
New-Item -ItemType Directory -Force -Path (Join-Path $fixture '_toolchain') | Out-Null
foreach($name in @('check_current','check_prebuild','build_current')){
 Copy-Item -LiteralPath (Join-Path $root "package/$name.ps1") -Destination (Join-Path $repo "package/$name.ps1")
}
$scriptText=Get-Content (Join-Path $repo 'package/check_current.ps1') -Raw
$paths=@('CMakeLists.txt','sources.cmake','sources_ue_wrap.cmake','sources_dev.cmake')
$paths+= [regex]::Matches($scriptText,"'((?:include/|src/)[^']+\.(?:h|cpp|inc))'") | ForEach-Object {$_.Groups[1].Value}
foreach($path in $paths){
 $file=Join-Path $repo "src/votv-coop/$path"
 New-Item -ItemType Directory -Force -Path (Split-Path $file) | Out-Null
 [IO.File]::WriteAllText($file,'fixture')
}
$pythonDouble=@'
import sys
from pathlib import Path
root=Path.cwd()
name=Path(__file__).stem
with (root/'events.txt').open('a') as f:f.write('check:'+name+'\n')
sys.exit(1 if (root/'failure.txt').read_text()==name else 0)
'@
$names=@('public_prose_include_test','disk_utf8_test','reliablekind_gate','fact_rows_gate','public_prose_gate')
$names+= [regex]::Matches($scriptText,"'([a-z_]+_test)'") | ForEach-Object {$_.Groups[1].Value}
foreach($name in $names | Sort-Object -Unique){
 foreach($dir in @('agent/tests','.github/ci')){[IO.File]::WriteAllText((Join-Path $repo "$dir/$name.py"),$pythonDouble)}
}
$environmentDouble=@'
function global:powershell {
 $argv=@($args)
 $file=$argv[[Array]::IndexOf($argv,'-File')+1]
 $name=[IO.Path]::GetFileNameWithoutExtension($file)
 Add-Content -LiteralPath (Join-Path $global:orderRepo 'events.txt') -Value "check:$name"
 $global:LASTEXITCODE=if((Get-Content (Join-Path $global:orderRepo 'failure.txt') -Raw) -eq $name){1}else{0}
}
function global:cmake {
 Add-Content -LiteralPath (Join-Path $global:orderRepo 'events.txt') -Value 'build'
 $global:LASTEXITCODE=0
}
'@
[IO.File]::WriteAllText((Join-Path $fixture '_toolchain/env.ps1'),$environmentDouble)
[IO.File]::WriteAllText((Join-Path $repo 'package/verify_current_build.ps1'),@'
param([string]$BuildDirectory,[string]$EvidenceDirectory)
Add-Content -LiteralPath (Join-Path $global:orderRepo 'events.txt') -Value 'inspect'
[pscustomobject]@{Protocol=2220;DllSHA256='fixture';ProductVersion='fixture'} |
 ConvertTo-Json | Set-Content (Join-Path $EvidenceDirectory 'identity.json')
'@)
[pscustomobject]@{protocol=2220;dll='build/mock/main.dll';qualification='build/release/verification.json';source_snapshot='build/release/source-before.json'} |
 ConvertTo-Json | Set-Content (Join-Path $repo 'package/release.json')
$global:orderRepo=$repo
. (Join-Path $fixture '_toolchain/env.ps1')
function Check([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function RunCase([string]$Name,[string]$Failure,[hashtable]$Options,[bool]$ExpectFailure,[bool]$ExpectBuild,[switch]$Reuse){
 [IO.File]::WriteAllText((Join-Path $repo 'failure.txt'),$Failure)
 [IO.File]::WriteAllText((Join-Path $repo 'events.txt'),'')
 $evidence=Join-Path $repo "build/$Name"
 $thrown=$false
 try {
  if($Reuse){& (Join-Path $repo 'package/build_current.ps1') -ReuseQualification (Join-Path $repo 'build/passing/verification.json') | Out-Null}
  else {& (Join-Path $repo 'package/check_current.ps1') -EvidenceDirectory $evidence @Options | Out-Null}
 } catch {$thrown=$true; $errorText=$_.Exception.Message}
 Check ($thrown -eq $ExpectFailure) "$Name unexpected exception state: $errorText"
 if($thrown){
  $expected=if($Failure -eq 'run_emote'){'Current checks failed'}elseif($Name -eq 'unknown-group'){'Unknown check group'}else{"Pre-build gate failed: $Failure"}
  Check ($errorText -like "*$expected*") "$Name failed for the wrong reason: $errorText"
 }
 $events=@(Get-Content (Join-Path $repo 'events.txt'))
 Check (($events -contains 'build') -eq $ExpectBuild) "$Name wrong build admission"
 Check (($events -contains 'inspect') -eq $ExpectBuild) "$Name wrong artifact-inspection admission"
 if($ExpectBuild){
  $buildAt=[Array]::IndexOf($events,'build')
  Check ($buildAt -gt [Array]::IndexOf($events,'check:public_prose_gate')) "$Name built before prose gate"
  Check ($buildAt -gt [Array]::IndexOf($events,'check:fact_rows_gate')) "$Name built before fact gate"
  Check ($events[-1] -eq 'inspect') "$Name did not inspect after build"
  Check (@($events[($buildAt+1)..($events.Count-1)] | Where-Object {$_ -like 'check:*'}).Count -eq 0) "$Name ran a source check after build"
 } elseif(-not $Reuse){
  Check (-not(Test-Path (Join-Path $evidence 'verification.json'))) "$Name emitted successful build evidence without building"
 }
 Write-Output "PASS $Name"
}
RunCase 'failed-test' 'run_emote' @{CheckGroups=@('run_emote')} $true $false
RunCase 'scoped-pass' '' @{CheckGroups=@('run_emote')} $false $false
RunCase 'unknown-group' '' @{CheckGroups=@('unknown')} $true $false
RunCase 'failed-prose' 'public_prose_gate' @{} $true $false
RunCase 'failed-facts' 'fact_rows_gate' @{} $true $false
RunCase 'checks-only' '' @{ChecksOnly=$true} $false $false
RunCase 'passing' '' @{} $false $true
RunCase 'reuse-failed-prose' 'public_prose_gate' @{} $true $false -Reuse
RunCase 'reuse-failed-facts' 'fact_rows_gate' @{} $true $false -Reuse
Write-Output 'PASS build ordering: actual workflow scripts, simulated external commands; no release DLL compiled'
