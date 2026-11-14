param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/vulkan-context-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
. (Join-Path $root '../_toolchain/env.ps1')
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$root/src/votv-coop/include" "/I$root/src/votv-coop/third_party/Vulkan-Headers/include" "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/vulkan_context_test.cpp"
if($LASTEXITCODE){throw 'Vulkan context compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Vulkan context fixture failed'}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/vulkan_present_extract.py" "$output/vulkan_present_lifetime.inc"
if($LASTEXITCODE){throw 'Vulkan present extraction failed'}
& cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/I$root/src/votv-coop/third_party/Vulkan-Headers/include" "/Fo$output/present.obj" "/Fe$output/present.exe" "$PSScriptRoot/vulkan_present_lifetime_test.cpp"
if($LASTEXITCODE){throw 'Vulkan present lifetime compile failed'}
& "$output/present.exe"
if($LASTEXITCODE){throw 'Vulkan present lifetime fixture failed'}
if($FaultControls){
 $path=Join-Path $output 'vulkan_present_lifetime.inc'
 $source=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
 try{
  foreach($case in @(
   @{Name='present_lifetime';Before="std::lock_guard<std::mutex> lock(g_mu);`n    const FrameResult";After='const FrameResult'},
   @{Name='destroy_lifetime';Before="std::lock_guard<std::mutex> lock(g_mu);`n    if (chain";After='if (chain'}
  )){
   if(-not $source.Contains($case.Before)){throw "Missing Vulkan lifetime control: $($case.Name)"}
   [IO.File]::WriteAllText($path,$source.Replace($case.Before,$case.After))
   & cl /nologo /std:c++20 /EHsc /utf-8 "/I$output" "/I$root/src/votv-coop/third_party/Vulkan-Headers/include" "/Fo$output/$($case.Name).obj" "/Fe$output/$($case.Name).exe" "$PSScriptRoot/vulkan_present_lifetime_test.cpp"
   if($LASTEXITCODE){throw "Vulkan lifetime control compile failed: $($case.Name)"}
   $saved=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& "$output/$($case.Name).exe" 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
   [IO.File]::WriteAllText((Join-Path $output ($case.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Vulkan lifetime mutation did not reach assertion: $($case.Name)"}
   Write-Output "PASS Vulkan lifetime fault $($case.Name)"
  }
 }finally{[IO.File]::WriteAllText($path,$source)}
}
$global:LASTEXITCODE=0
