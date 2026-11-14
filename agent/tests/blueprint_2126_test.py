"""The native routes used by the door and cover fixes, from this game's assets."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root.parent/'_research'))
from kismet_dis import Package,Dis,find_script
def body(asset,name):
    p=Package(str(root.parent/'_research/assets'/('VotV__Content__'+asset+'.uasset')))
    e=next(e for e in p.exports if e['name']==name and p.class_of_export(e)=='Function')
    code,_=find_script(p.export_data(e));assert code is not None
    rows=dict(Dis(p,code).run());assert not any('!! ' in row for row in rows.values())
    return rows
g=body('objects__generator','ExecuteUbergraph_generator')
assert 'Inst(moving)' in g[3785] and g[3800]=='Inst(moving) = true'
assert 'Local(K2Node_CustomEvent_open)' in g[3890]
assert 'TimelineComponent.Reverse()' in g[3262] and 'TimelineComponent.Play()' in g[3295]
assert 'Inst(opened) = true'==g[188] and 'Inst(opened) = false'==g[200]
assert 'SetActorEnableCollision(true)' in g[155] and 'SetActorEnableCollision(false)' in g[3909]
desk=body('test__analogDScreenTest','ExecuteUbergraph_analogDScreenTest')
for at in (47207,47907,48607): assert 'IsOverlappingActor' in desk[at]
for at in (47624,48324,49024): assert 'K2_DestroyActor' in desk[at]
assert 'panelTakeOff' in desk[47067] and 'panelsProcess' in desk[47067]
gun=body('objects__components__comp_gravitygun','hold')
assert gun[324]=='op5F(Inst(obj), Local(obj))' and gun[3227]=='op5F(Inst(obj), noobj)'
print('PASS: cooked transformer direction/collision, cover overlap/consume guards, gravity held actor')
