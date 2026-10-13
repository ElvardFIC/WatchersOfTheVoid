"""Check the gun interference diagnosis against this installed game's bytecode."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / '_research'))
from kismet_dis import Package, Dis, find_script

def body(asset, function):
    p = Package(str(root / '_research/assets' / ('VotV__Content__' + asset + '.uasset')))
    e = next(e for e in p.exports if e['name'] == function and p.class_of_export(e) == 'Function')
    code, _ = find_script(p.export_data(e))
    rows = '\n'.join(s for _, s in Dis(p, code).run())
    assert '!! ' not in rows
    return rows

gun = 'objects__components__comp_gravitygun'
assert 'ExecuteUbergraph_comp_gravitygun(2678)' in body(gun, 'ReceiveTick')
tick = body(gun, 'ExecuteUbergraph_comp_gravitygun')
hold = body(gun, 'hold')
assert 'getMainPlayer(self' in tick and 'getMainPlayer(self' in hold
assert 'Inst(input_fire) = false' in tick and 'Inst(input_rotate) = false' in hold
assert 'Inst(obj), Local(obj)' in hold
assert 'GameplayStatics.GetPlayerCharacter(Local(__WorldContext), 0)' in body('main__lib', 'getMainPlayer')
print('PASS: orphan gun ticks address player 0, consume local input and store target on their own component')
