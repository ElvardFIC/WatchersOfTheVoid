"""Pin the native call and entry offsets used by the Ramiel decision adapter."""
from pathlib import Path
import sys

checkout = Path(__file__).resolve().parents[2]
research = checkout.parent / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

asset = research / 'current-assets/VotV__Content__objects__misc__geomOcta.uasset'
pkg = Package(str(asset))
graph = dict(body(pkg, next(e for e in pkg.exports if e['name'] == 'ExecuteUbergraph_geomOcta')))
assert 'K2Node_CustomEvent_NotifyName_3' in graph[1092]
assert 'name:finish' in graph[1119]
assert graph[1176].endswith('Inst(minSphere)->PrimitiveComponent.IsOverlappingActor(Inst(targetActor))')
assert next(i for i in graph if i > 1176) == 1227
assert 'IsOverlappingActor_ReturnValue' in graph[1227] and 'jump @1314' in graph[1227]
assert graph[1299] == 'fire()'
assert 'name:close' in graph[1375]
assert 'Inst(targetActor), noobj' in graph[1799]
for name, entry in [
    ('aggro', 3592),
    ('BndEvt__geomOcta_minSphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature', 3766),
    ('OnNotifyBegin_1DFC28904024A8FB71C3ACA9DBD440F9', 1092),
]:
    rows = dict(body(pkg, next(e for e in pkg.exports if e['name'] == name)))
    assert any(f'ExecuteUbergraph_geomOcta({entry})' in row for row in rows.values()), name
print('PASS native Ramiel graph: finish gate, exact overlap call boundary, refire/close branches and replay entries')
