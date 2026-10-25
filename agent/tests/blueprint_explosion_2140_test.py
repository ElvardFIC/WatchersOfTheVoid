"""Game 0.9.0n cooked spawn census and integration; not a multiplayer test."""
import json
from collections import Counter
from blueprint_fire_2137_test import root,research,body,Package
from kismet_dis import find_script,Dis
sites=json.loads((research/'explosion_inventory_2140.json').read_text())
counts=Counter()
for s in sites:
 p=Package(str(research/'assets'/s['asset'].replace('/','__')))
 e=next(e for e in p.exports if e['name']==s['function'] and p.class_of_export(e)=='Function')
 code,_=find_script(p.export_data(e));rows=dict(Dis(p,code).run())
 assert rows[s['pc']]==s['call']
 counts[s['call'].split('obj:')[1].split(',')[0].rsplit('.',1)[1]]+=1
assert counts=={'explosion_C':72,'explosion_saltrocket_C':1,'explosion_pc_C':1,'begoExplosion_C':4,'puffballExplosion_C':1},counts
for cls in ['prop_pipebomb','prop_grenade','prop_landmine','prop_stickybomb','prop_gascan']:
 assert any('/'+cls+'.uasset' in s['asset'] for s in sites),cls
p=Package(str(research/'assets/VotV__Content__objects__misc__explosion_saltrocket.uasset'))
assert not any(p.class_of_export(e)=='Function' for e in p.exports) # inherits the gated native graph
s=root/'src/votv-coop'
sub=(s/'src/coop/session/subsystems.cpp').read_text()
for call in ('Install(&session)','Tick()','OnDisconnect()'):
 assert 'coop::native_explosion_sync::'+call in sub
assert 'coop::explosion_events::Install(&session)' not in sub
assert 'coop::explosion_events::Tick()' not in sub
assert 'case ReliableKind::ExplosionEvent:' not in (s/'src/coop/net/session_lanes.h').read_text()
dispatch=(s/'src/coop/dispatch/event_dispatch_world.cpp').read_text()
assert 'sizeof(net::ExplosionEventPayload)' in dispatch and 'coop::explosion_events::Receive' in dispatch
destroy=(s/'src/coop/props/prop_destroy_seam.cpp').read_text()
assert destroy.index('explosion_events::ConsumesClientDestroy(self)')<destroy.index('s->SendPropDestroy(dp)')
fire=(s/'src/coop/props/prop_fire_sync.cpp').read_text()
assert fire.index('explosion_events::IsReplica(c.callerObject)')<fire.index('QueueLocal(p,key)')
print('PASS 79 decoded native spawn sites:73 gameplay blasts,1 private PC minigame,5 spore/story-object spawners; salt inheritance and wire/authority/lifecycle integration')
