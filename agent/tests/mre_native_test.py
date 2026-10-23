"""Current cooked MRE action/spawn: authority and direct birth provenance."""
from pathlib import Path
import sys

research=Path(__file__).resolve().parents[3]/'_research'
sys.path.insert(0,str(research))
from kismet_dis import Package
from dump_2135 import body

p=Package(str(research/'current-assets/VotV__Content__objects__prop_food_mre.uasset'))
fs={e['name']:body(p,e) for e in p.exports if p.class_of_export(e)=='Function'}
assert any('ExecuteUbergraph_prop_food_mre(157)' in line for _,line in fs['actionOptionIndex'])
graph=dict(fs['ExecuteUbergraph_prop_food_mre'])
assert graph[10]=='spawn()'
assert 'mreopen_Cue' in graph[70] and 'att_small' in graph[70]
assert graph[138]=='K2_DestroyActor()'
assert graph[157].startswith('prop_C.actionOptionIndex(')
assert 'EqualEqual_ByteByte(Local(K2Node_Event_action), byte:4)' in graph[203]
assert 'jump @253' in graph[234] and graph[248]=='jump @10'

spawn=dict(fs['spawn']);text='\n'.join(spawn.values())
assert text.count('GameplayStatics.BeginDeferredActorSpawnFromClass(')==4
assert text.count('GameplayStatics.FinishSpawningActor(')==4
for at in (1335,2697,4553,5353):assert 'GameplayStatics.FinishSpawningActor(' in spawn[at]
assert 'name:foodData' in spawn[4199] and 'SetStructurePropertyByName' in spawn[4199]
assert 'name:name' in spawn[4321] and 'name:name' in spawn[5121]
assert 'Local(CallFunc_Prop_To_Object_object)' in spawn[1102]
assert 'prop_food.prop_food_C' in spawn[4157] and 'prop.prop_C' in spawn[5020]
assert 'Inst(name)' in spawn[1667] and '->init()' in spawn[1716]
assert 'mrefood_4, name:mrefood_4' in spawn[5]  # preserve native duplicate/weighted choices
assert 'progressAchievement(name:jackpot' in text
assert 'Inst(uses)' in text and 'cockroachMaster)->addRoach(' in text
assert 'prop_sponge_bucketPour' in spawn[2464] and 'poison_strength' in spawn[3021]
assert 'K2_DestroyActor' not in text
print('PASS cooked MRE: action-4 spawn/cue/destroy order, three loot paths plus poison-pour output, class/food/name initialization, weighted loot and native reward/roach/portion side effects')
