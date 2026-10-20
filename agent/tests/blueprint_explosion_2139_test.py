"""Cooked gascan/explosion control flow and release integration, not a game test."""
from blueprint_fire_2137_test import body, root, research, Package
from kismet_dis import Reader
from props_dump import read_tag

g=body('objects__prop_gascan','ExecuteUbergraph_prop_gascan')
assert 'GreaterEqual_FloatFloat(Inst(amount), 25)' in g[10]
assert 'explosion.explosion_C' in g[372] and g[372].endswith('byte:0, self))')
for pc,name in [(550,'force'),(727,'radius'),(904,'damage'),(1295,'fireStrength'),(1472,'fireFuel')]:
 assert 'SetFloatPropertyByName(' in g[pc] and 'name:'+name in g[pc]
assert 'SetIntPropertyByName(' in g[945] and 'name:debris, 6)' in g[945]
assert 'FinishSpawningActor(' in g[1909] # all parameters precede BeginPlay
assert '1952' in body('objects__prop_gascan','broken')[0]
assert '1957' in body('objects__prop_gascan','broken_fire')[0]
d=body('objects__components__comp_physicsImpact','ExecuteUbergraph_comp_physicsImpact')
assert '->broken_fire()' in d[9241] and '->broken()' in d[9404]
assert 'push_flow @9288' in d[9278] and 'K2_DestroyActor()' in d[9288]
e=body('objects__misc__explosion','ExecuteUbergraph_explosion')
assert '7661' in body('objects__misc__explosion','ReceiveBeginPlay')[0]
assert 'Delay(self, 0.1' in e[9124] and 'skipconst:15' in e[9124]
for pc,name in [(1626,'reachedByExplosion'),(2595,'exploded'),(3498,'addDamage'),(4681,'ignite')]:
 assert '->'+name+'(' in e[pc]
assert 'Inst(Skip Setting)' in e[3498]
assert 'FireImpulse()' in e[4783] and e[4819].endswith(' = 1')
assert 'LessEqual_IntInt(' in e[4842] and 'Inst(debris)' in e[4842]
assert 'jump @5783' in e[4880] and 'burningDebris_C' in e[5160] # debris=0 really skips all fragments
assert 'runeCheck()' in e[9179]
deaf=body('objects__misc__explosion','processDeaf')
assert 'GetPlayerCharacter(self, 0)' in deaf[576] and 'GetPlayerCameraManager(self, 0)' in deaf[137]
water=body('objects__misc__explosion_underwater','ExecuteUbergraph_explosion_underwater')
assert 'K2_SetWorldLocation(' in water[299] and len(water)==10 # splash only, no second damage pass

p=Package(str(research/'assets/VotV__Content__objects__misc__explosion.uasset'))
data=p.export_data(next(x for x in p.exports if x['name']=='Default__explosion_C'))
r=Reader(data);tags={}
while True:
 tag=read_tag(p,r)
 if tag is None:break
 name,typ,size,idx,extra=tag;tags[name]=(typ,size);r.p+=size
for name in ('force','radius','damage','innerShake','outerShake'):
 assert tags[name]==('FloatProperty',4)
assert tags['shake']==('ObjectProperty',4)

s=root/'src/votv-coop'
sub=(s/'src/coop/session/subsystems.cpp').read_text()
for call in ('Install(&session)','Tick()','OnPeerLeft(slot)','OnDisconnect()'):
 assert 'coop::explosion_sync::'+call in sub
assert 'explosion_sync::QueueConnect' not in sub
lanes=(s/'src/coop/net/session_lanes.h').read_text()
assert 'case ReliableKind::Explosion:' not in lanes # Normal fallback, no relay whitelist entry
dispatch=(s/'src/coop/dispatch/event_dispatch_world.cpp').read_text()
assert 'sizeof(net::ExplosionPayload)' in dispatch and 'coop::explosion_sync::Receive' in dispatch
destroy=(s/'src/coop/props/prop_destroy_seam.cpp').read_text()
assert destroy.index('explosion_sync::ConsumesClientDestroy(self)')<destroy.index('s->SendPropDestroy(dp)')
sync=(s/'src/coop/props/prop_fire_sync.cpp').read_text()
assert sync.index('explosion_sync::IsReplica(c.callerObject)')<sync.index('QueueLocal(p,key)')
print('PASS cooked gascan deferred setters, destroy ordering, delayed damage, debris/rune/deaf/water routes, field types and wire/lifecycle integration')
