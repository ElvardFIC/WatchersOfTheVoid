"""Native evidence and wiring assertions; not a two-peer UE runtime test."""
from pathlib import Path
import struct,sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root.parent/'_research'))
from kismet_dis import Package,Dis
def body(asset,name):
 p=Package(str(root.parent/'_research/assets'/('VotV__Content__'+asset+'.uasset')))
 e=next(e for e in p.exports if e['name']==name and p.class_of_export(e)=='Function')
 data=p.export_data(e)
 for tail in (12,14):
  for off in range(len(data)-8-tail):
   bsz,ssz=struct.unpack_from('<ii',data,off)
   if ssz>0 and bsz>0 and off+8+ssz+tail==len(data) and data[off+8+ssz-1]==0x53:
    rows=Dis(p,data[off+8:off+8+ssz]).run()
    if rows and rows[-1]==(bsz-1,'end_of_script') and all('!! ' not in s for _,s in rows):return dict(rows)
 raise AssertionError((asset,name,'no fully decoded script'))
widget=body('umg__windows__ui_coordinates','ExecuteUbergraph_ui_coordinates')
assert 'Inst(dynmat_triangle)' in widget[546] and widget[565]=='updCursorLocations()'
assert 'Inst(coordinate_2)' in widget[956]
cursor=body('umg__windows__ui_coordinates','changeCursor')
assert "'view'" in cursor[27] and '3:Local(Temp_string_Variable)' in cursor[141]
desk=body('test__analogDScreenTest','ExecuteUbergraph_analogDScreenTest')
assert desk[82980]=='pop_flow_if_not(Inst(coord_isPing))'
assert desk[82990]=='jump @80105'
for off,name in ((5453,'outer'),(5862,'inner'),(5990,'ping'),(8758,'inner'),(8817,'ping')):
 assert 'SetScalarParameterValue(name:alpha, Inst(coord_ping_'+name+'))' in desk[off]
assert desk[83274]=='push_flow @11341' # native idle alpha painter is independent of the FSM gate
source=root/'src/votv-coop'
lanes=(source/'src/coop/net/session_lanes.h').read_text()
relay=lanes[lanes.index('inline bool IsHostRelayable'): ] if 'inline bool IsHostRelayable' in lanes else lanes[lanes.index('// The host relay:'):]
assert 'case ReliableKind::DishAimState:' not in relay
dispatch=(source/'src/coop/dispatch/event_dispatch_signal.cpp').read_text()
section=dispatch[dispatch.index('case net::ReliableKind::DishAimState:'):dispatch.index('case net::ReliableKind::SavedSignalAppend:')]
assert 'net::DeskVisualPayload ap' in section and 'sizeof(net::DeskVisualPayload)' in section
console=(source/'src/coop/interactables/console_state_sync.cpp').read_text()
for call in ('Install(session)','Tick()','QueueConnect(peerSlot)','OnState(p, senderSlot)','OnDisconnect()'):
 assert 'coop::desk_visual_sync::'+call in console
assert 'SendReliable(coop::net::ReliableKind::DishAimState' not in console
sub=(source/'src/coop/session/subsystems.cpp').read_text()
assert 'coop::desk_visual_sync::OnPeerLeft(slot)' in sub
verdict=(source/'src/coop/interactables/desk_ping_sync.cpp').read_text()
assert '!CP::WriteCursorOnly(aim.viewX, aim.viewY) || !CP::WriteDishCommitted(aim)' in verdict
input_source=(source/'src/coop/interactables/desk_input_sync.cpp').read_text()
start=input_source.index('if (static_cast<DeskInputField>(p.field) == DeskInputField::CoordIsPing)')
assert 'return;' in input_source[start:input_source.index('if (!CD::EnsureResolved()',start)]
print('PASS: native coordinate materials/idle repaint/FSM gate and wire lifecycle integration')
