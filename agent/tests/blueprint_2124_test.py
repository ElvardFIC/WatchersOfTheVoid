"""Check the real cooked 0.9.0n Blueprints behind b2124's hooks and field layouts."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'_research'))
from kismet_dis import Package,Dis,find_script
def pkg(name): return Package(str(root/'_research/assets'/('VotV__Content__'+name+'.uasset')))
def body(p,name):
    e=next(e for e in p.exports if e['name']==name and p.class_of_export(e)=='Function')
    code,_=find_script(p.export_data(e)); assert code is not None,name
    rows=dict(Dis(p,code).run()); assert not any('!! ' in r for r in rows.values()),name
    return rows
def joined(p,name): return '\n'.join(body(p,name).values())
gun=pkg('objects__components__comp_gravitygun')
graph=body(gun,'ExecuteUbergraph_comp_gravitygun')
assert 'AddForce' in graph[4189] and 'HitComponent' in graph[4189]
assert 'AddImpulse' in graph[6604] and 'HitComponent' in graph[6604]
assert 'hold(noobj' in graph[5290] and 'AddImpulse' in graph[5231]
assert body(gun,'hold')[324]=='op5F(Inst(obj), Local(obj))'
swing=pkg('objects__prop_swinger')
graph=body(swing,'ExecuteUbergraph_prop_swinger')
assert 'AddTorqueInRadians' in graph[1418] and 'mouseXY' in graph[386]
assert 'SetConstraintReferenceOrientation' in joined(swing,'open')
micro=pkg('objects__prop_microwave')
assert 'Inst(opened) = true' in joined(micro,'openDoor')
assert 'AddTorque' not in joined(micro,'openDoor') # light/state callback is NOT hinge movement
desk=pkg('test__analogDScreenTest')
graph=body(desk,'ExecuteUbergraph_analogDScreenTest')
assert 'panelsState' in graph[18711] and 'false' in graph[18711]
assert 'panelsUnscrew' in graph[49298]
assert 'SetVisibility' in joined(desk,'updPanels') and 'SetCollision' in joined(desk,'updPanels')
assert 'ExecuteUbergraph_analogDScreenTest(47067)' in joined(desk,'ReceiveActorBeginOverlap')
assert 'panelTakeOff' in graph[47067] and 'lid1' in graph[47207]
radio=pkg('objects__radiotower')
graph=body(radio,'ExecuteUbergraph_radiotower')
assert graph[3393].endswith('.X = 5') and graph[3425].endswith('.Y = 5')
assert 'switchNeighbors(Inst(lookAtIndex), true)' in graph[4055]
assert 'Inst(TL_lever)->TimelineComponent.PlayFromStart()'==graph[2015]
assert 'Inst(TL_reset)->TimelineComponent.PlayFromStart()'==graph[2278]
assert graph[5500].endswith('= byte:1') and 'Inst(fuses)' in graph[5500]
assert 'K2_DestroyActor' in graph[5737] # insert consumes only after successful native write
assert 'fusesButtons' in joined(radio,'genFuses')
for name in ('scramble','breakdown','broken','broken_fire','tryToFix','completeMinigame','playerUsedOn','loadData'):
    body(radio,name)
print('PASS: real gravity force/shot routes, mouse-driven hinge, cover consumers, radio 2x2..5x5 and fuse spend')
