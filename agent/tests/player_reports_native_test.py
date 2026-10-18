"""Check report-specific assumptions against the installed 0.9.0n bytecode."""
from pathlib import Path
import sys
research=Path(__file__).resolve().parents[3]/'_research'
sys.path.insert(0,str(research))
from kismet_dis import Package
from dump_2135 import body

def graph(asset,function):
    path=research/'assets'/('VotV__Content__'+asset+'.uasset')
    if not path.exists(): raise SystemExit('UNAVAILABLE: extracted report assets required: '+str(path))
    p=Package(str(path))
    e=next(e for e in p.exports if e['name']==function and p.class_of_export(e)=='Function')
    rows=dict(body(p,e))
    assert not any('NO VALID SCRIPT' in v for v in rows.values())
    return rows

wisp=graph('objects__killerwisp','ExecuteUbergraph_killerwisp')
assert 'Inst(sittingOn), self)' in wisp[3409]
assert 'Inst(held) = true' in wisp[3150]
assert 'name:playerGrab' in wisp[2941]
assert any('name:fatality)' in v for v in wisp.values())
cremator=graph('objects__cremator','ExecuteUbergraph_cremator')
assert graph('objects__cremator','useLever')[0]=='ExecuteUbergraph_cremator(9600)'
assert cremator[9600]=='jump @2209' and cremator[2209]=='Inst(anim) = true'
assert 'PlayFromStart()' in cremator[2309]
assert cremator[2716]=='collectObjects(true)' and cremator[2746]=='Inst(isActive) = true'
assert 'handle)->SceneComponent.K2_SetRelativeRotation' in cremator[8855]
gun=graph('objects__prop_physgun','ExecuteUbergraph_prop_physgun')
assert gun[759]=='Inst(hold) = true' and gun[6010]=='Inst(hold) = false'
assert gun[5162]=='Inst(beam)->Activate(true)'
assert 'Inst(beam)->SetBeamEndPoint(0,' in gun[6968]
gravity=graph('objects__components__comp_gravitygun','hold')
assert 'Inst(gravigun)->Inst(object_hold_loop)->Activate(true)' in gravity[2506]
assert 'Inst(gravigun)->Inst(object_grab)->Activate(true)' in gravity[2565]
assert 'Inst(gravigun)->Inst(object_release)->Activate(true)' in gravity[3047]
print('PASS native wisp capture is not a chair; cremator delayed lever/content mutation; native gun FX components')
