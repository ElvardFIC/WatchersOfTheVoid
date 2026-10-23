param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root '_research/tests_2147'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/container.obj" "/Fe$output/container.exe" agent/tests/container_extract_2147_test.cpp /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw 'Container test compilation failed'}
 & "$output/container.exe"
 if($LASTEXITCODE -ne 0){throw 'Container test failed'}
 if($FaultControls){
  $source=Get-Content src/votv-coop/src/coop/items/container_extract_restore.h -Raw
  foreach($name in @('unguarded-restore','missing-owner','missing-carrier-frame')){
   $mutant=switch($name){
    'unguarded-restore' {$source.Replace('SR::ApplyRecordViaWhile(actor,load,record,current)','SR::ApplyRecordVia(actor,load,record)')}
    'missing-owner' {$source.Replace('caller.Get()!=call.callerObject || ','')}
    'missing-carrier-frame' {$source.Replace('return selected==actor;','return true;')}
   }
   if($mutant -eq $source){throw "Missing container mutation site: $name"}
   $header=Join-Path $output "$name.h"
   [IO.File]::WriteAllText($header,$mutant,[Text.UTF8Encoding]::new($false))
   $fixture=Join-Path $output "$name.cpp"
   $lines=@(('#define CONTAINER_RESTORE_SOURCE "'+$header.Replace('\','/')+'"'),('#include "'+(Join-Path $PSScriptRoot 'container_extract_2147_test.cpp').Replace('\','/')+'"'))
   [IO.File]::WriteAllText($fixture,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
   & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" $fixture /link /OPT:REF
   if($LASTEXITCODE -ne 0){throw "Container control compile failed: $name"}
   & "$output/$name.exe"
   if($LASTEXITCODE -ne 1){throw "Container control did not fire: $name"}
   Write-Output "PASS expected-failure control: $name"
  }
 }
 # Compile only the changed translation unit to catch integration errors, without linking a mod.
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/inventory_pickup_sync.obj" src/votv-coop/src/coop/items/inventory_pickup_sync.cpp
 if($LASTEXITCODE -ne 0){throw 'Inventory pickup source compilation failed'}
}finally{Pop-Location}
