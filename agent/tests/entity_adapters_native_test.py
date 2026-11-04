"""Verify the cooked schemas and mutation order used by the creature adapters."""
from pathlib import Path
import sys

checkout = Path(__file__).resolve().parents[2]
research = checkout.parent / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

def rows(asset, function):
    pkg = Package(str(research / 'current-assets' / ('VotV__Content__' + asset + '.uasset')))
    return dict(body(pkg, next(e for e in pkg.exports if e['name'] == function)))

add = rows('objects__cockroachMaster', 'addRoach')
assert 'Local(location)' in add[76] and '10000' in add[122]
assert 'Inst(count), Inst(maxAmount)' in add[170]
assert 'Local(bypassCheck)' in add[222]
assert 'Local(size)' in add[1184] and 'Actor.AddComponent' in add[1468]
assert 'Array_Add(Inst(roaches)' in add[1513]
assert 'K2_SetWorldLocation(Local(location)' in add[1581]
assert 'Inst(count) = ' in add[1702]
assert not any('Delay(' in row for row in add.values())
delete = rows('objects__cockroachMaster', 'deleteRoach')
assert 'Local(crush)' in delete[0]
assert 'Inst(roaches), Local(IndexToRemove)' in delete[191]
assert 'K2_DestroyComponent(self)' in delete[250]
assert 'Array_Set(Inst(roaches), Local(IndexToRemove)' in delete[283]
assert 'Subtract_IntInt(Inst(count), 1)' in delete[343]
assert not any('Array_Remove' in row or 'Delay(' in row for row in delete.values())
gather = rows('objects__killerwisp', 'gather')
assert gather[31] == 'Inst(gathered) = true'
assert 'Inst(piram2), Local(piram2)' in gather[61]
assert 'SetMovementMode(byte:0' in gather[157]
assert 'StopMovementImmediately' in gather[197]
assert gather[247] == 'releasePlayer()'
pyramid = rows('objects__piramid2', 'ExecuteUbergraph_piramid2')
assert any('Inst(wispTarget)->gather(noobj, self)' in row for row in pyramid.values())
assert '(1,1,0)' in pyramid[6408] and '(1,1,0)' in pyramid[6458]
assert 'Vector_Distance(' in pyramid[6508] and ', 10000)' in pyramid[6554]
seen = rows('objects__piramid2', 'scanWisps')
assert any('killerWisps' in row for row in seen.values())
assert any('cast</Game/objects/killerwisp.killerwisp_C>' in row for row in seen.values())
print('PASS cooked entity adapters: roach synchronous add/delete schemas and null holes, native proximity/capacity gates, killerwisp gather and pyramid target choreography')
