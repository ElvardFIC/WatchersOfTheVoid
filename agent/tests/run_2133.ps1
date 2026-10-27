param([string]$Python=(Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root '_research/tests_2133'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
 . (Join-Path $root '../_toolchain/env.ps1')
 function Compile-AndRun([string]$name,[string]$source,[bool]$negative) {
  & cl /nologo /std:c++20 /EHsc /Gy /I agent/tests/desk_stubs /I src/votv-coop/include "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source /link /OPT:REF
  if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
  & "$output/$name.exe"
  $rc=$LASTEXITCODE
  if($negative){if($rc -ne 1){throw "Negative assertion expected1, got $rc for $name"};Write-Host "PASS negative control: $name"}
  elseif($rc -ne 0){throw "Test failed: $name"}
 }
 Compile-AndRun 'controller' 'agent/tests/desk_visual_2133_test.cpp' $false
 Compile-AndRun 'wrapper' 'agent/tests/coords_wrapper_2133_test.cpp' $false
 & $Python agent/tests/blueprint_coords_2133_test.py
 if($LASTEXITCODE -ne 0){throw 'Native evidence/integration failed'}
 $cases=@(
 @{Name='authority';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='if (holder!=sender && !tail) return;';After=''},
 @{Name='canonical_sender';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='sender!=0 ||';After=''},
 @{Name='stale_revision';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='if (g_have && static_cast<int32_t>(p.revision-g_state.revision)<=0) return;';After=''},
 @{Name='unready_loss';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='g_state=p; g_have=true;';After='g_state=p; g_have=CP::PresentationTarget()!=nullptr;'},
 @{Name='widget_reuse';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='!g_widget.Is(widget) || !g_material.Is(material)';After='false'},
 @{Name='no_timeout';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='now-g_lastInput>3000';After='false'},
 @{Name='no_departure';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='g_state.owner==slot';After='false'},
 @{Name='no_callback_epoch';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='g_epoch==epoch && g_world==ue_wrap::world_identity::Generation() &&';After=''},
 @{Name='no_reentry';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='g_ticking ||';After=''},
 @{Name='construct_overwrite';Suite='desk_visual';File='coop/interactables/desk_visual_sync.cpp';Before='const bool changed = drift ||';After='const bool changed ='},
 @{Name='active_machine';Suite='coords_wrapper';File='ue_wrap/desk/coords_visual.cpp';Before='simulating ||';After=''},
 @{Name='partial_materials';Suite='coords_wrapper';File='ue_wrap/desk/coords_visual.cpp';Before='a.num != 3';After='false'},
 @{Name='failed_repaint';Suite='coords_wrapper';File='ue_wrap/desk/coords_panel.cpp';Before='!ue_wrap::Call(w, f) ||';After='(ue_wrap::Call(w, f), false) ||'},
 @{Name='callback_widget';Suite='coords_wrapper';File='ue_wrap/desk/coords_panel.cpp';Before='identity.Get() == w && Instance() == w';After='true'},
 @{Name='no_idle_values';Suite='coords_wrapper';File='ue_wrap/desk/coords_visual.cpp';Before='for (int i = 0; i < 3; ++i) std::memcpy(static_cast<uint8_t*>(d) + off[i], &values[i], sizeof(float));';After=''},
 @{Name='unchecked_param';Suite='coords_wrapper';File='ue_wrap/desk/coords_visual.cpp';Before='!frame.Set<R::FName>(L"ParameterName", alpha)';After='(frame.Set<R::FName>(L"ParameterName", alpha), false)'}
 )
 foreach($case in $cases){
  $source=[IO.File]::ReadAllText((Join-Path $root ('src/votv-coop/src/'+$case.File)))
  if(-not $source.Contains($case.Before)){throw "Mutation target changed: $($case.Name)"}
  $mutant=Join-Path $output ($case.Name+'_source.cpp')
  [IO.File]::WriteAllText($mutant,$source.Replace($case.Before,$case.After))
  $harness=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ($case.Suite+'_2133_test.cpp')))
  $harness=$harness.Replace('../../src/votv-coop/src/'+$case.File,($mutant -replace '\\','/'))
  $harness=$harness.Replace('../../src/votv-coop/src/',(($root -replace '\\','/')+'/src/votv-coop/src/'))
  $copy=Join-Path $output ($case.Name+'_test.cpp');[IO.File]::WriteAllText($copy,$harness)
  Compile-AndRun $case.Name $copy $true
 }
 $global:LASTEXITCODE=0
} finally {Pop-Location}
