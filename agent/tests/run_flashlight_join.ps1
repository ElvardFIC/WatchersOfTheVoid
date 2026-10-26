param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $checkout 'build/flashlight-join-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $checkout
try {
 if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $checkout '../_toolchain/env.ps1')}
 function Compile-Join($exe,$forcedHeader=''){
  $extra=@();if($forcedHeader){$extra+="/FI$forcedHeader"}
  & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /DTEST_STATE_MAX_PEERS=32 /I agent/tests/state_2145_stubs `
   /I agent/tests/text_drone_stubs /I agent/tests/stubs /I src/votv-coop/include @extra "/Fo$exe.obj" "/Fe$exe" agent/tests/flashlight_join_test.cpp /link /OPT:REF
  if($LASTEXITCODE){throw 'Flashlight join fixture compile failed'}
 }
 $exe=Join-Path $output 'join.exe';Compile-Join $exe
 & $exe
 if($LASTEXITCODE){throw 'Flashlight join regression failed'}
 & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/item_activate.obj" src/votv-coop/src/coop/player/item_activate.cpp
 if($LASTEXITCODE){throw 'Flashlight join production compile failed'}
 if($FaultControls){
  $source=[IO.File]::ReadAllText((Join-Path $checkout 'src/votv-coop/src/coop/player/item_activate.cpp')).Replace("`r`n","`n")
  $controls=@(
   @{Name='local-retry';Before='if(owns() && sent && SameReplaySeat(target,targetSeat) && sourceCurrent())';After='if(owns() && (sent || !source) && SameReplaySeat(target,targetSeat) && sourceCurrent())'},
   @{Name='relay-retry';Before='if(owns() && sent && SameReplaySeat(target,targetSeat) && sourceCurrent())';After='if(owns() && (sent || source) && SameReplaySeat(target,targetSeat) && sourceCurrent())'},
   @{Name='fresh';Before='if(!BuildPayloadFromLocal(local,payload,session))continue;';After='if(!BuildPayloadFromLocal(local,payload,session)){ClearJoinDebt(target,source);continue;}'},
   @{Name='off';Before='--budget;';After='if(!payload.state){ClearJoinDebt(target,source);continue;} --budget;'},
   @{Name='recipient';Before='ClearJoinDebt(peerSlot,other);';After='/* retained departed recipient debt */'},
   @{Name='author';Before='if(peerSlot>0)ClearJoinDebt(other,peerSlot);';After='/* retained departed author debt */'},
   @{Name='packet';Before='&payload,sizeof(payload),static_cast<uint8_t>(source)';After='(source?&g_peerActivateCache[source]:&payload),sizeof(payload),static_cast<uint8_t>(source)'},
   @{Name='completion';Before='if(owns() && sent && SameReplaySeat(target,targetSeat) && sourceCurrent())';After='if(Current(epoch) && sent)'},
   @{Name='budget';Before='constexpr size_t kJoinSendBudget=64;';After='constexpr size_t kJoinSendBudget=1024;'},
   @{Name='admission';Before='!session->IsSlotReady(target)';After='false';ExtraBefore='!session->IsSlotWorldReady(target)';ExtraAfter='false'}
  )
  foreach($control in $controls){
   if(-not $source.Contains($control.Before)){throw "Missing join fault anchor: $($control.Name)"}
   $copy=$source.Replace($control.Before,$control.After)
   if($control.ExtraBefore){
    if(-not $copy.Contains($control.ExtraBefore)){throw "Missing secondary join anchor: $($control.Name)"}
    $copy=$copy.Replace($control.ExtraBefore,$control.ExtraAfter)
   }
   if($copy -eq $source){throw "Join control does not change production source: $($control.Name)"}
   $path=Join-Path $output ($control.Name+'.cpp');[IO.File]::WriteAllText($path,$copy)
   $header=Join-Path $output ($control.Name+'.h')
   [IO.File]::WriteAllText($header,('#define FLASHLIGHT_STATE_SOURCE "'+$path.Replace('\','/')+'"'+[Environment]::NewLine))
   $exe=Join-Path $output ($control.Name+'.exe');Compile-Join $exe $header
   $savedPreference=$ErrorActionPreference
   try{$ErrorActionPreference='Continue';$report=& $exe $control.Name 2>&1 | Out-String;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
   [IO.File]::WriteAllText((Join-Path $output ($control.Name+'.log')),$report)
   if($code -eq 0 -or $report -notmatch 'FAIL \d+:'){throw "Join control did not reach its assertion: $($control.Name)"}
   Write-Output ('PASS join fault '+$control.Name+': '+[regex]::Match($report,'FAIL [^\r\n]+').Value)
  }
 }
}finally{Pop-Location}
