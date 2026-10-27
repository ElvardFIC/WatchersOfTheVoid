"""Native game evidence and routing; this does not simulate UE physics or two peers."""
from pathlib import Path
import struct,sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root.parent/'_research'))
from kismet_dis import Package,Dis
p=Package(str(root.parent/'_research/assets/VotV__Content__objects__cargoLift.uasset'))
def body(name):
 e=next(e for e in p.exports if e['name']==name and p.class_of_export(e)=='Function')
 data=p.export_data(e)
 for tail in (12,14):
  for off in range(len(data)-8-tail):
   bsz,ssz=struct.unpack_from('<ii',data,off)
   if bsz>0 and ssz>0 and off+8+ssz+tail==len(data) and data[off+8+ssz-1]==0x53:
    rows=Dis(p,data[off+8:off+8+ssz]).run()
    if rows and rows[-1]==(bsz-1,'end_of_script') and all('!! ' not in s for _,s in rows):return dict(rows)
 raise AssertionError((name,'no complete script'))
graph=body('ExecuteUbergraph_cargoLift')
assert graph[1289]=='pop_flow' # player_use does NOT start motion
assert graph[1649]=='if not (Inst(moving)) jump @1291'
assert graph[1291]=='Inst(moving) = true'
assert graph[1331]=='Inst(dir) = Local(CallFunc_Not_PreBool_ReturnValue)'
assert graph[1774].endswith('NotEqual_ByteByte(Local(K2Node_Event_action), byte:4)')
assert 'Inst(lift)->Inst(ChildActor)->Actor.K2_GetRootComponent()' in graph[1355]
assert 'Inst(buttonsElev)->SceneComponent.K2_AttachToComponent' in graph[1558]
assert 'Inst(comp)' in graph[1629]
assert 'KismetSystemLibrary.Delay' in graph[853] and 'skipconst:15' in graph[853]
assert graph[779]=='Inst(moving) = false'
assert '(0,0,500)' in graph[1041] and 'Inst(attach)' in graph[1104]
assert graph[820]=='Inst(dir) = Local(CallFunc_Not_PreBool_ReturnValue_1)'
assert body('ReceiveBeginPlay')[0]=='ExecuteUbergraph_cargoLift(1355)'
assert body('actionOptionIndex')[72]=='ExecuteUbergraph_cargoLift(1774)'
source=root/'src/votv-coop'
tracker=(source/'src/coop/props/prop_element_tracker.cpp').read_text()
assert 'if (ue_wrap::engine::IsChildActor(actor))' in tracker
sub=(source/'src/coop/session/subsystems.cpp').read_text()
for call in ('Install(&session)','Tick()','QueueConnect(slot)','OnPeerLeft(slot)','OnDisconnect()'):
 assert 'coop::cargo_lift_sync::'+call in sub
lanes=(source/'src/coop/net/session_lanes.h').read_text()
assert 'case ReliableKind::CargoLift:' not in lanes # default normal, not relay/pre-world
dispatch=(source/'src/coop/dispatch/event_dispatch_world.cpp').read_text()
assert 'sizeof(net::CargoLiftPayload)' in dispatch and 'coop::cargo_lift_sync::Receive' in dispatch
sync=(source/'src/coop/world/cargo_lift_sync.cpp').read_text()
wrapper=(source/'src/ue_wrap/devices/cargo_lift.cpp').read_text()
assert 'L::Replay(' in sync and 'L::Apply(' not in sync and 'OnGraph' not in sync
assert 'SetSimulatePhysics' not in wrapper and 'CallParamlessNamed' not in wrapper
pump=(source/'src/coop/session/net_pump.cpp').read_text()
assert 'if (!isHost && isConnected &&' in pump and 'g_worldReadyAnnounced.store(true' in pump
print('PASS: native button/empty use/BeginPlay/latent motion/obstruction/child-body evidence and complete network lifecycle wiring')
