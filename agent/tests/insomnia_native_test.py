"""Verify why Insomniac must keep native, peer-local behavior (VotV 0.9.0n)."""
from pathlib import Path
import sys

checkout = Path(__file__).resolve().parents[2]
research = checkout.parent / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

def graph(asset, function):
    path = research / 'assets' / ('VotV__Content__' + asset + '.uasset')
    if not path.is_file():
        raise SystemExit('UNAVAILABLE: extracted Insomniac, ticker and mainGamemode assets required')
    pkg = Package(str(path))
    export = next(e for e in pkg.exports
                  if e['name'] == function and pkg.class_of_export(e) == 'Function')
    rows = dict(body(pkg, export))
    assert not any('NO VALID SCRIPT' in row for row in rows.values())
    return rows

spawn = graph('main__mainGamemode', 'trySpawnInsomniac')
assert spawn[10] == 'if not (Inst(isSleep)) jump @72'
assert 'Subtract_FloatFloat(20, Inst(saveSlot)->Inst(sleep))' in spawn[72]
assert 'GameplayStatics.GetPlayerCameraManager(self, 0)' in spawn[450]
ticker = graph('objects__tickers__ticker_insomniacSpawner', 'ExecuteUbergraph_ticker_insomniacSpawner')
assert 'trySpawnInsomniac(' in ticker[887]
assert 'insomniac_Child_C' in ticker[112] and 'insomniac_C' in ticker[323]
assert 'Inst(gamemode)->Inst(insomniac)' in ticker[466]
demon = graph('objects__insomniac', 'ExecuteUbergraph_insomniac')
assert 'GameplayStatics.GetPlayerPawn(self, 0)' in demon[5759]
assert 'CreateMoveToProxyObject(self, self,' in demon[5785]
assert 'Local(CallFunc_GetPlayerPawn_ReturnValue), 225, false)' in demon[5785]
assert 'Inst(gamemode)->Inst(saveSlot)->Inst(sleep), 20)' in demon[503]
assert '->punch(' in demon[2567]
assert demon[2826] == 'Inst(gamemode)->createDream(noobj)'
assert '->stopMovement()' in demon[6199]
assert '->SetControlRotation(' in demon[6589]
print('PASS: local sleep/camera spawn, both variants, local pursuit, punch, grab and dream entry')

night = graph('main__mainGamemode', 'ExecuteUbergraph_mainGamemode')
assert night[773] == 'pop_flow_if_not(Inst(isSleep))'
assert night[788] == 'pop_flow_if_not(Inst(nightmaresEnabled))'
assert night[798].endswith('= bedSleepProb()')
assert 'RandomBoolWithWeight(Local(CallFunc_bedSleepProb_ReturnValue))' in night[830]
assert night[859] == 'pop_flow_if_not(Local(CallFunc_RandomBoolWithWeight_ReturnValue_4))'
assert night[869] == 'wakeup()' and night[883] == 'createDream(noobj)'
bed = graph('main__mainGamemode', 'bedSleepProb')
assert bed[76] == 'Local(sl) = Inst(bed)->Inst(dreamProb)'
assert bed[450] == 'Local(sl) = 0.15'
assert 'Inst(dreamProbability), 0)' in bed[321]
assert 'false:Local(sl); true:Inst(dreamProbability)' in bed[374]
print('PASS: personal sleeping/enabled/bed chance/native roll gates, wake-before-dream order')
