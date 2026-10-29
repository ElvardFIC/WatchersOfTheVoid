"""Native bytecode evidence for floor-bed identity; not an in-game simulation."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root.parent / '_research'))
from kismet_dis import Package, Dis, find_script
def functions(asset, selected=None):
    p = Package(str(root.parent / '_research/assets' / ('VotV__Content__' + asset + '.uasset')))
    result = {}
    for e in p.exports:
        if p.class_of_export(e) == 'Function':
            if selected is not None and e['name'] not in selected:
                continue
            code, _ = find_script(p.export_data(e))
            rows = '\n'.join(s for _, s in Dis(p, code).run())
            assert '!! ' not in rows
            result[e['name']] = rows
    return result
flat = functions('objects__misc__sleepingbagWrap')
assert 'gen(false)' in flat['UserConstructionScript']
assert 'prop_C.UserConstructionScript' not in flat['UserConstructionScript']
for fn in ('UserConstructionScript', 'gen', 'ExecuteUbergraph_sleepingbagWrap'):
    assert 'init(' not in flat[fn].lower() and 'getKey(' not in flat[fn]
assert 'prop_C.ReceiveBeginPlay()' in flat['ExecuteUbergraph_sleepingbagWrap']
bed = functions('objects__bed')
assert 'gamemodeValid' not in bed and 'UserConstructionScript' not in bed
prop = functions('objects__prop', {'gamemodeValid', 'getKey'})
assert prop['gamemodeValid'] == 'return nothing\nend_of_script'
assert '->assignKey(Inst(key)' in prop['getKey']
lib = functions('main__lib', {'assignKey'})
assert 'generateRandomKey(' in lib['assignKey'] and 'Out(keyIn) =' in lib['assignKey']
source = (root / 'src/votv-coop/src/ue_wrap/actors/prop_keys.cpp').read_text()
assert 'return ReadField<R::FName>(obj, P::off::Aprop_Key);' in source
print('PASS evidence: floor gen does not run prop init/getKey; native getKey assigns identity, while mod reads the field directly')
