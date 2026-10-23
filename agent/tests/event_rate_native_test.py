"""Check added rate coverage against the existing local 0.9.0n corpus."""
from pathlib import Path
import hashlib
import re
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

path = research / 'assets/VotV__Content__objects__tickers__ticker_eyers.uasset'
p = Package(str(path))
graph = next(e for e in p.exports if e['name'] == 'ExecuteUbergraph_ticker_eyers')
rows = dict(body(p, graph))
text = '\n'.join(rows.values())
assert 'RandomFloatInRange(1800, 2700)' in rows[214]
assert 'SetActorTickInterval' in rows[252]
assert 'RandomBoolWithWeight(0.05)' in rows[271]
assert 'timeZ).Z, 4)' in rows[296]
assert 'timeZ).X, 20)' in rows[383] and 'timeZ).X, 4)' in rows[470]
assert 'eyer.eyer_C' in text and 'FinishSpawningActor' in text
source = Path(__file__).resolve().parents[2] / 'src/votv-coop/src/coop/creatures/owner_entity_sync.cpp'
assert 'L"eyer_C"' in source.read_text(encoding='utf-8')
print('PASS existing native night-eye wait, day/night eligibility, 5% chance and owner-entity product')
print('uasset SHA256', hashlib.sha256(path.read_bytes()).hexdigest())
print('uexp SHA256', hashlib.sha256(path.with_suffix('.uexp').read_bytes()).hexdigest())

def graph(asset, function):
    package = Package(str(research / 'assets' / f'VotV__Content__objects__{asset}.uasset'))
    export = next(e for e in package.exports
                  if e['name'] == function and package.class_of_export(e) == 'Function')
    result = dict(body(package, export))
    assert not any('!! ' in row for row in result.values()), (asset, function)
    return result

# Only the eligible-roll wait is scaled. Generator condition, native probability
# and the root's separate .02-.1 s visual delays must stay in the native graphs.
flicker_asset = 'tickers__ticker_flickerer'
flicker = graph(flicker_asset, 'ExecuteUbergraph_ticker_flickerer')
assert graph(flicker_asset, 'ReceiveTick')[18] == 'ExecuteUbergraph_ticker_flickerer(1187)'
assert 'RandomFloatInRange(30, 60)' in flicker[1187]
assert 'SetActorTickInterval' in flicker[1225]
assert 'Inst(cycle)' in flicker[169]
assert 'Divide_FloatFloat(Local(CallFunc_Conv_IntToFloat_ReturnValue), 100)' in flicker[228]
assert 'Multiply_FloatFloat(Local(CallFunc_Divide_FloatFloat_ReturnValue), 2)' in flicker[270]
assert 'FClamp(Local(CallFunc_Multiply_FloatFloat_ReturnValue), 0, 1)' in flicker[312]
assert 'FMin(Local(CallFunc_FClamp_ReturnValue), Inst(min))' in flicker[359]
assert 'Subtract_FloatFloat(1, Inst(min))' in flicker[667]
assert 'MultiplyMultiply_FloatFloat(Local(CallFunc_Subtract_FloatFloat_ReturnValue), 3)' in flicker[709]
assert 'Add_FloatFloat(Local(CallFunc_MultiplyMultiply_FloatFloat_ReturnValue), 0.02)' in flicker[751]
assert 'RandomBoolWithWeight(Local(CallFunc_Add_FloatFloat_ReturnValue))' in flicker[793]
assert 'flick()' in flicker[1021]
assert not any('Delay(' in row or 'Spawn' in row or 'SetTimer' in row
               for row in flicker.values())

root_asset = 'triggers__trigger_lightRoot'
assert graph(root_asset, 'flick')[0] == 'ExecuteUbergraph_trigger_lightRoot(2178)'
root = graph(root_asset, 'ExecuteUbergraph_trigger_lightRoot')
assert root[2178] == 'jump @1092'
assert 'setActive(Inst(isActive))' in root[281] and 'setActive(Inst(isActive))' in root[593]
assert 'setActive(false)' in root[1285] and 'setActive(false)' in root[1589]
assert 'RandomIntegerInRange(1, 4)' in root[712]
assert 'RandomFloatInRange(0.02, 0.1)' in root[995]
assert 'RandomFloatInRange(0.02, 0.1)' in root[1700]
phases = [row for pos, row in root.items() if 88 <= pos < 1853]
assert not any('Inst(isActive) =' in row or 'Spawn' in row or 'QuitGame' in row
               for row in phases)
print('PASS existing native flickerer: 30-60 s eligible roll, generator probability and separate lamp/ambient phase delays')
for asset in (flicker_asset, root_asset):
    asset_path = research / 'assets' / f'VotV__Content__objects__{asset}.uasset'
    for suffix in ('.uasset', '.uexp'):
        item = asset_path.with_suffix(suffix)
        print(item.name, 'SHA256', hashlib.sha256(item.read_bytes()).hexdigest())

# The added authority watch is fingerprinted from an existing live game capture.
# Two already-qualified ticker bodies control its canonical encoding.
def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value

fixture = Path(__file__).with_name('event_flicker_capture_0.9.0n.txt').read_text(encoding='utf-8')
table = (Path(__file__).resolve().parents[2] /
         'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc').read_text(encoding='utf-8')
captured = set()
for key, whole, text in re.findall(r'^### (\S+) ([0-9a-f]{16}) ok\n(.*?)(?=^### |\Z)', fixture, re.M | re.S):
    assert fnv(text) == int(whole, 16), ('capture integrity', key)
    signature, canonical_body = text.split('\n', 1)
    cls, name = key.split('.')
    row = re.search(r'\{L"' + cls + r'", L"' + name + r'", L"' + key +
                    r'", 0x([0-9a-f]+)ull, 0x([0-9a-f]+)ull\}', table)
    assert row and tuple(int(v, 16) for v in row.groups()) == (fnv(signature + '\n'), fnv(canonical_body))
    captured.add(key)
assert captured == {'ticker_flickerer_c.receivetick', 'ticker_fossilhoundspawner_c.receivetick',
                    'ticker_wispspawner_c.receivetick'}
print('PASS captured flickerer authority reference and two existing Blueprint format controls')
