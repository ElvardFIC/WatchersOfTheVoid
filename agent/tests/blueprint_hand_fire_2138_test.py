"""Native destruction cleanup and source wiring of the hand-fire extension."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'agent/tests'))
from blueprint_fire_2137_test import body
f=body('objects__components__comp_flammable','ExecuteUbergraph_comp_flammable')
assert 'bind_delegate(des,' in f[422] and 'OnDestroyed' in f[445]
assert 'extinguish()' in f[7893]
# Verify the native des event leads to that cleanup, not just a matching name in a dump.
d=body('objects__components__comp_flammable','des')
assert any('ExecuteUbergraph_comp_flammable(7908)' in text for text in d.values())
assert 'Deactivate()' in f[7952] and 'Deactivate()' in f[8028]
assert not any('allBurningObjects' in text for offset,text in f.items() if 7908 <= offset <= 8064)
# Therefore mirror teardown explicitly extinguishes BEFORE destruction.
s=root/'src/votv-coop'
h=(s/'src/coop/player/hand_item.cpp').read_text()
destroy=h[h.index('void DestroyMirror('):h.index('void NoteSpawnFailed(')]
assert destroy.index('prop_fire::Extinguish(actor)') < destroy.index('E::DestroyActor(actor)')
sub=(s/'src/coop/session/subsystems.cpp').read_text() + (s/'src/coop/session/connect_replay.cpp').read_text()
for call in ('Install(&session)','Tick()','QueueConnect(slot)','OnPeerLeft(slot)','OnDisconnect()'):
 assert 'coop::hand_fire_sync::'+call in sub
dispatch=(s/'src/coop/dispatch/event_dispatch_world.cpp').read_text()
assert 'sizeof(net::HandFirePayload)' in dispatch and 'coop::hand_fire_sync::Receive' in dispatch
lanes=(s/'src/coop/net/session_lanes.h').read_text()
assert 'case ReliableKind::HandFire:' not in lanes # default Normal, explicit receiver, no auto-relay
assert 'src/coop/player/hand_fire_sync.cpp' in (s/'sources.cmake').read_text()
print('PASS native owner-destroy fire cleanup and hand fire lifecycle/routing integration')
