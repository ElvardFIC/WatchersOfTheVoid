"""Cooked native fire behavior and actual integration; no in-game simulation."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
research=root.parent/'_research'
sys.path.insert(0,str(research))
from kismet_dis import Package,Dis,find_script
def body(asset,name):
 p=Package(str(research/'assets'/('VotV__Content__'+asset+'.uasset')))
 e=next(e for e in p.exports if e['name']==name and p.class_of_export(e)=='Function')
 code,_=find_script(p.export_data(e));rows=Dis(p,code).run()
 assert rows and rows[-1][1]=='end_of_script' and all('!! ' not in s for _,s in rows)
 return dict(rows)
p=body('objects__prop','ExecuteUbergraph_prop')
assert 'name:NODE_Addflammable-1' in p[1779]
assert p[333]=='ignited()' and 'ignite_s(Local(K2Node_Event_fuel))' in p[347]
assert 'extinguish()' in p[1957]
f=body('objects__components__comp_flammable','ExecuteUbergraph_comp_flammable')
assert 'FMin(' in f[1118] and ', 60)' in f[1118]
assert 'allBurningObjects' in f[613] and 'allBurningObjects' in f[4147]
assert 'fireDamage(' in f[4367]
assert 'BoxTraceMulti(' in f[4902] and '->ignite(' in f[7134]
assert 'Inst(fuelSpeed)' in f[8107] and 'VictoryFloatMinusEquals' in f[8153]
assert 'Deactivate()' in f[4013] and 'Deactivate()' in f[4049]
assert 'K2_DestroyComponent(self)' in f[4001]
s=root/'src/votv-coop'
sub=(s/'src/coop/session/subsystems.cpp').read_text()
for call in ('Install(&session)','Tick()','QueueConnect(slot)','OnPeerLeft(slot)','OnDisconnect()'):
 assert 'coop::prop_fire_sync::'+call in sub
assert 'case ReliableKind::PropFire:' not in (s/'src/coop/net/session_lanes.h').read_text()
dispatch=(s/'src/coop/dispatch/event_dispatch_world.cpp').read_text()
assert 'sizeof(net::PropFirePayload)' in dispatch and 'coop::prop_fire_sync::Receive' in dispatch
sync=(s/'src/coop/props/prop_fire_sync.cpp').read_text()
assert '!hand_item::IsHandAxisActor(a)' in sync
assert 'WatchName(L"fireDamage"' in sync and 'WatchName(L"ignited"' in sync
print('PASS native component fuel, creation/removal, spread/damage, player paths and wire/lifecycle integration')
