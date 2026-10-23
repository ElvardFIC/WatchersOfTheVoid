"""Measured native damage, saw-eligibility getter and wall effects contracts."""
from pathlib import Path
import sys

research=Path(__file__).resolve().parents[3]/'_research'
sys.path.insert(0,str(research))
from kismet_dis import Package
from dump_2135 import body

def functions(name):
    p=Package(str(research/'current-assets'/f'VotV__Content__objects__{name}.uasset'))
    return {e['name']:body(p,e) for e in p.exports if p.class_of_export(e)=='Function'}

prop=functions('prop')
assert prop['isNotSawable']==[(0,'Out(return) = false'),(11,'return nothing'),(13,'end_of_script')]
assert any('ExecuteUbergraph_prop(1046)' in line for _,line in prop['addDamage'])
assert any('ExecuteUbergraph_prop(440)' in line for _,line in prop['slice'])
graph=dict(prop['ExecuteUbergraph_prop'])
assert 'Divide_VectorFloat(Local(K2Node_Event_impact_1), 100)' in graph[1046]
assert 'iface(Inst(physicsImpact))->impactDamage(' in graph[1088]
assert 'Local(K2Node_Event_hit_1)' in graph[1088] and 'Local(K2Node_Event_actor_1)' in graph[1088]
assert graph[440]=='pop_flow'  # the base slice is a no-op, overrides remain separate

hacksaw=dict(functions('prop_hacksaw')['ExecuteUbergraph_prop_hacksaw'])
assert 'Inst(sawingProp)->isNotSawable(' in hacksaw[5901]
assert 'Local(CallFunc_isNotSawable_return)' in hacksaw[5946] and 'jump @6466' in hacksaw[5946]
assert 'Local(CallFunc_safeAsProp_prop)' in hacksaw[5705]
assert hacksaw[4243]=='Inst(sawingProp)->broken()' and hacksaw[4279]=='Inst(sawingProp)->K2_DestroyActor()'

wall=dict(functions('customWall')['damage'])
assert 'VictoryFloatMinusEquals(Inst(health), Local(damage)' in wall[5]
assert 'SetScalarParameterValueOnMaterials(name:crack' in wall[362]
assert 'SpawnEmitterAtLocation' in wall[416] and 'Local(Location), Local(Rotation)' in wall[416]
assert wall[502]=='K2_DestroyActor()'
print('PASS cooked action contracts: actor getter rather than component property, inherited damage/hit forwarding, base slice no-op, distinct hacksaw break path and native wall damage/effects/destruction')
