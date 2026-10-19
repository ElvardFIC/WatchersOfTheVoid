$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/player-reports-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$units=@('coop/creatures/npc_sync','coop/creatures/npc_sync_install','coop/creatures/wisp_tear_mirror',
 'coop/player/sit_pose','coop/player/skin_registry','coop/player/skin_preview',
 'coop/player/local_streams','coop/player/remote_player','coop/player/hand_item','coop/net/session_streams',
 'coop/interactables/kitchen_sync','coop/interactables/interactable_sync','coop/interactables/door_verb_intent',
 'coop/world/spawn_authority',
 'ue_wrap/devices/kitchen_machine','ue_wrap/devices/cremator','ue_wrap/actors/physics_gun')
foreach($unit in $units){
 $name=$unit.Replace('/','_')
 & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
  "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" `
  "/Fo$output/$name.obj" "$checkout/src/votv-coop/src/$unit.cpp"
 if($LASTEXITCODE -ne 0){throw "Production compile failed: $unit"}
}
