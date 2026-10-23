"""Verify the dream collection contract in the freshly extracted current pak.

This is cooked Blueprint evidence, not a claim of rendered or real-peer testing.
The decoder and extracted game assets are local research inputs.
"""
from pathlib import Path
import hashlib
import re
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

assets = Path(sys.argv[1]) if len(sys.argv) > 1 else research / 'current-assets'
used = {}


def graph(asset, function):
    path = assets / ('VotV__Content__objects__dreams__' + asset + '.uasset')
    package = used.setdefault(asset, Package(str(path)))
    export = next(e for e in package.exports
                  if package.class_of_export(e) == 'Function' and e['name'] == function)
    rows = dict(body(package, export))
    assert rows and all('NO VALID SCRIPT' not in row for row in rows.values())
    return rows


handler = graph('dreamBurger_coll',
    'BndEvt__Capsule_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature')
assert handler[18] == 'persistent.K2Node_ComponentBoundEvent_OtherActor = Local(OtherActor)'
assert handler[108] == 'ExecuteUbergraph_dreamBurger_coll(10)'
collection = graph('dreamBurger_coll', 'ExecuteUbergraph_dreamBurger_coll')
assert collection[10] == 'op5F(Local(Temp_object_Variable), self)'
assert collection[21] == 'Inst(dream)->collec(Local(Temp_object_Variable))'
assert collection[66] == 'K2_DestroyActor()' and collection[80] == 'jump @546'
# The entry has no local-player or class check; do not widen this repair into
# suppressing nonplayer overlaps that the native handler intentionally accepts.
assert not any('OtherActor' in row or 'cast<' in row
               for pc, row in collection.items() if 10 <= pc < 80)
remove = graph('dream_burger', 'collec')
assert 'Array_RemoveItem(Inst(burgeres), Out(Item))' in remove[0]
assert graph('dream_burger', 'awoken')[0] == 'ExecuteUbergraph_dream_burger(1876)'
reward = graph('dream_burger', 'ExecuteUbergraph_dream_burger')
assert 'Array_Length(Inst(burgeres))' in reward[1876]
assert 'LessEqual_IntInt(Local(CallFunc_Array_Length_ReturnValue_1), 0)' in reward[1935]
assert reward[1969] == 'pop_flow_if_not(Local(CallFunc_LessEqual_IntInt_ReturnValue))'
assert 'addFood(100, 100, 0, noobj, true, byte:0' in reward[2025]
assert 'progressAchievement(name:dream_burger, true, false, self)' in reward[2089]
assert 'beaten_dreams_' in reward[1651] and 'beaten_dreams_' in reward[1756]
print('PASS fresh native dream burger: unfiltered overlap consumes local entry/actor; awoken alone applies personal food, beaten-dream stat and achievement')
for asset in sorted(used):
    path = assets / ('VotV__Content__objects__dreams__' + asset + '.uasset')
    for suffix in ('.uasset', '.uexp'):
        item = path.with_suffix(suffix)
        print(item.name, 'SHA256', hashlib.sha256(item.read_bytes()).hexdigest())

# The watch's reference comes from a real game capture. Existing overlap guards
# control the canonical signature and transitive event-graph encoding.
def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value

capture = Path(__file__).with_name('dream_burger_capture_0.9.0n.txt').read_text(encoding='utf-8')
table = (Path(__file__).resolve().parents[2] /
         'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc').read_text(encoding='utf-8')
captured = set()
for key, whole, text in re.findall(r'^### (\S+) ([0-9a-f]{16}) ok\n(.*?)(?=^### |\Z)', capture, re.M | re.S):
    assert fnv(text) == int(whole, 16), ('capture integrity', key)
    signature, canonical_body = text.split('\n', 1)
    cls, name = key.split('.')
    row = re.search(r'\{L"' + re.escape(cls) + r'", L"' + re.escape(name) + r'", L"' + re.escape(key) +
                    r'", 0x([0-9a-f]+)ull, 0x([0-9a-f]+)ull\}', table)
    assert row and tuple(int(v, 16) for v in row.groups()) == (fnv(signature + '\n'), fnv(canonical_body))
    captured.add(key)
assert captured == {
    'dreamburger_coll_c.bndevt__capsule_k2node_componentboundevent_1_componentbeginoverlapsignature__delegatesignature',
    'dream_climb_c.bndevt__dream_climb_box_k2node_componentboundevent_0_componentbeginoverlapsignature__delegatesignature',
    'dreamfilldoor_c.bndevt__box_k2node_componentboundevent_0_componentbeginoverlapsignature__delegatesignature',
}
print('PASS captured dream-burger reference and two existing native overlap controls')
