param([string]$ReuseQualification='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$release=Get-Content (Join-Path $PSScriptRoot 'release.json') -Raw | ConvertFrom-Json
. (Join-Path $root '../_toolchain/env.ps1')
$evidence=Split-Path (Join-Path $root $release.qualification) -Parent
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
function ProductionStamp {
 $production=Join-Path $root 'src/votv-coop'
 $files=@(Get-ChildItem -LiteralPath (Join-Path $production 'include'),(Join-Path $production 'src') -Recurse -File | Where-Object {$_.Extension -in @('.h','.hpp','.cpp','.inc','.rc')})
 $files+=Get-Item -LiteralPath (Join-Path $production 'CMakeLists.txt'),(Join-Path $production 'sources.cmake')
 $rows=foreach($file in ($files | Sort-Object FullName)){$file.FullName.Substring($production.Length)+':'+(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
 $sha=[Security.Cryptography.SHA256]::Create()
 try{([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows -join "`n"))))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
}
$files=@(Get-ChildItem -LiteralPath (Join-Path $root 'src/votv-coop/src'),(Join-Path $root 'src/votv-coop/include') -Recurse -File)
$files+=Get-Item -LiteralPath (Join-Path $root 'src/votv-coop/CMakeLists.txt'),(Join-Path $root 'src/votv-coop/sources.cmake'),(Join-Path $PSScriptRoot 'release.json')
$snapshot=foreach($file in ($files | Sort-Object FullName)) {
 [pscustomobject]@{path=$file.FullName.Substring($root.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$snapshot | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $root $release.source_snapshot) -Encoding UTF8
$before=ProductionStamp
Push-Location $root
try {
 if($ReuseQualification){
  $referencePath=if([IO.Path]::IsPathRooted($ReuseQualification)){$ReuseQualification}else{Join-Path $root $ReuseQualification}
  $reference=Get-Content -LiteralPath $referencePath -Raw | ConvertFrom-Json
  if(-not $reference.FullRegression -or -not $reference.BuildUpToDate -or $reference.Protocol -ne $release.protocol -or
     $reference.SourceSnapshotSHA256 -ne $before -or $reference.PassingGroups -lt 68 -or
     $reference.PassingGroups -ne $reference.Results.Count -or @($reference.Results | Where-Object ExitCode -ne 0).Count){
   throw 'Full regression reuse requires the identical qualified production snapshot and protocol'
  }
  $reference | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $evidence 'reference-qualification.json') -Encoding UTF8
  $build=Split-Path (Join-Path $root $release.dll) -Parent
  $ErrorActionPreference='Continue'
  & cmake --build $build --target votv-coop --parallel 2 *> (Join-Path $evidence 'build.log')
  $code=$LASTEXITCODE
  $ErrorActionPreference='Stop'
  if($code){throw 'Release compilation failed; see build.log'}
  if((ProductionStamp) -ne $before){throw 'Production changed during Release compilation'}
  & (Join-Path $PSScriptRoot 'verify_current_build.ps1') -BuildDirectory $build -EvidenceDirectory $evidence
  $identity=Get-Content (Join-Path $evidence 'identity.json') -Raw | ConvertFrom-Json
  foreach($name in @('Results','PassingGroups','FullRegression','SourceSHA256','SourceSnapshotSHA256')){
   $identity | Add-Member -NotePropertyName $name -NotePropertyValue $reference.$name
  }
  $identity | Add-Member -NotePropertyName BuildUpToDate -NotePropertyValue $true
  $identity | Add-Member -NotePropertyName FreshFullRegression -NotePropertyValue $false
  $identity | Add-Member -NotePropertyName RegressionReuse -NotePropertyValue ([pscustomobject]@{
   Reference=$ReuseQualification;ReferenceSHA256=(Get-FileHash -LiteralPath $referencePath -Algorithm SHA256).Hash.ToLowerInvariant()
   ProductionSnapshotMatched=$true;OriginalDllSHA256=$reference.DllSHA256
   Scope='Existing complete regression results for identical production sources; fresh Release, package label and archive validation for the public-version change.'
  })
  $identity | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $root $release.qualification) -Encoding UTF8
 }else{
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'check_current.ps1') -EvidenceDirectory $evidence
  if($LASTEXITCODE){throw 'Full release qualification failed'}
 }
 foreach($entry in $snapshot){if((Get-FileHash -LiteralPath (Join-Path $root $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256){throw "Release input changed: $($entry.path)"}}
 & "$env:USERPROFILE/.local/bin/python3.15.exe" (Join-Path $PSScriptRoot 'verify_package_label.py')
 if($LASTEXITCODE){throw 'Production package display formatter failed'}
 & (Join-Path $PSScriptRoot 'package_current.ps1')
 & "$env:USERPROFILE/.local/bin/python3.15.exe" (Join-Path $PSScriptRoot 'verify_test_package.py')
 if($LASTEXITCODE){throw 'Package rejection controls failed'}
}finally{Pop-Location}
