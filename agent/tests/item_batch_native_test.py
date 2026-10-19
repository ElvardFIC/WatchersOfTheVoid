"""Verify the cooked boundaries used by the item-state fixes (VOTV 0.9.0n)."""
from pathlib import Path
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package, Dis, find_script


def rows(asset, function):
    p = Package(str(research / 'assets' / f'VotV__Content__objects__{asset}.uasset'))
    e = next(e for e in p.exports if e['name'] == function and p.class_of_export(e) == 'Function')
    code, _ = find_script(p.export_data(e))
    result = dict(Dis(p, code).run())
    assert not any('!! ' in row for row in result.values())
    return result


food = rows('prop_food', 'ExecuteUbergraph_prop_food')
assert 'Inst(uses)' in food[98]
assert 'bitten' in food[173] and 'eaten' in food[196]
assert 'K2_DestroyActor' in food[469]
record = '\n'.join(rows('prop_food', 'getData').values())
assert 'Inst(uses)' in record and 'Inst(temperature)' in record and 'Inst(ripeness)' in record
box = '\n'.join(rows('prop_foodBox', 'ExecuteUbergraph_prop_foodBox').values())
assert 'BeginDeferredActorSpawnFromClass' in box and 'FinishSpawningActor' in box and 'g_foodbox' in box
assert 'player' not in box.lower()  # safe host packaging output, no eater/vitals replay
gun = rows('prop_physgun', 'ExecuteUbergraph_prop_physgun')
assert 'SetSimulatePhysics(false)' in gun[5086]
assert 'K2_SetActorTransform' in gun[8591]
assert 'SetSimulatePhysics(Inst(compBodyInst).bSimulatePhysics)' in gun[9106]
release = '\n'.join(rows('prop_physgun', 'playerHandRelease_LMB').values())
assert 'ExecuteUbergraph_prop_physgun(9165)' in release
assert gun[9165] == 'jump @6010' and gun[6010] == 'Inst(hold) = false'
assert '\n'.join(rows('prop_physgun', 'release').values()) == 'return nothing\nend_of_script'
print('PASS native final-bite order, live food record, empty-box output and physgun transform/physics restore')
