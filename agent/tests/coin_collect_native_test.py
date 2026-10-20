"""Pin native pickup convergence and credit-before-destroy ordering in 0.9.0n."""
from pathlib import Path
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

path = research / 'assets/VotV__Content__objects__misc__baocoin.uasset'
if not path.exists():
    raise SystemExit('UNAVAILABLE: extracted native coin asset required: ' + str(path))
p = Package(str(path))


def graph(name):
    export = next(e for e in p.exports if e['name'] == name and p.class_of_export(e) == 'Function')
    result = dict(body(p, export))
    assert result and not any('NO VALID SCRIPT' in text for text in result.values())
    return result


press = graph('actionOptionIndex')
assert press[72] == 'ExecuteUbergraph_baocoin(441)'
overlap = graph('BndEvt__baocoin_collect_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature')
assert overlap[108] == 'ExecuteUbergraph_baocoin(1871)'
coin = graph('ExecuteUbergraph_baocoin')
assert 'mainPlayer.mainPlayer_C' in coin[1871]
assert coin[1936] == 'pop_flow_if_not(Local(K2Node_DynamicCast_bSuccess))'
assert coin[1946] == 'jump @441'
assert 'addPoints(Inst(points), self)' in coin[441]
assert 'addHint(' in coin[780]
assert coin[829] == 'K2_DestroyActor()'
assert not any('K2Node_Event_player' in text for pos, text in coin.items() if 441 <= pos <= 843)
print('PASS native coin E/overlap converge on one player-independent credit/hint/destroy block')
