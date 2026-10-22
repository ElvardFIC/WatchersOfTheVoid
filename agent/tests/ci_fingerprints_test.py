"""Check added references against a real native capture, including absent declarations."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc'
CAPTURE = Path(__file__).with_name('ci_reference_capture_0.9.0n.txt')
NEW = {
    'analogdscreentest_c.receiveactorbeginoverlap',
    'primitivecomponent.issimulatingphysics',
    'mainplayer_c.adddamage', 'mainplayer_c.attack', 'mainplayer_c.swingmelee',
    'mainplayer_c.inpactevt_fire_k2node_inputactionevent',
    'gameplaystatics.playsoundatlocation', 'gameplaystatics.spawnemitteratlocation',
    'gameplaystatics.spawnsoundattached', 'gameplaystatics.openlevel',
    'prop_food_c.getdata', 'prop_food_c.loaddata',
    'kismetsystemlibrary.boxoverlapactors', 'kavotiapatrolcontroller_c.receivebeginplay',
}
CONTROLS = {'gameplaystatics.finishspawningactor', 'mainplayer_c.impactdamage'}

def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value

capture = CAPTURE.read_text(encoding='utf-8')
rows = dict(re.findall(r'^#row \S+ \S+ (\S+) -> (\S+)$', capture, re.M))
blocks = {}
for match in re.finditer(r'^### ([^\n]+?) ([0-9a-f]{16}) (ok|native)\n(.*?)(?=^### |\Z)', capture, re.M | re.S):
    path, whole, state, text = match.groups()
    blocks[path] = (match.group(0), int(whole, 16), state, text)
table = TABLE.read_text(encoding='utf-8')
pattern = r'\{L"([^"]+)",\s*L"([^"]+)",\s*(nullptr|L"[^"]+"),\s*(0x[0-9a-f]+ull|0),\s*(0x[0-9a-f]+ull|0)\}'
existing = {f'{cls}.{name}': (path[2:-1] if path != 'nullptr' else None, int(sig.removesuffix('ull'), 0), int(body.removesuffix('ull'), 0))
            for cls, name, path, sig, body in re.findall(pattern, table)}
calculated = {}
fixture_rows = []
fixture_blocks = {}
for key in sorted(NEW | CONTROLS):
    resolved = rows.get(key)
    assert resolved is not None, ('No captured row', key)
    fixture_rows.extend(line for line in capture.splitlines()[:550] if line.startswith('#row ') and f' {key} -> ' in line)
    if resolved == '-':
        assert key in {'mainplayer_c.swingmelee', 'mainplayer_c.inpactevt_fire_k2node_inputactionevent'}
        assert 'mainplayer_c.adddamage' in blocks, 'The declaring class must have been loaded'
        calculated[key] = (None, 0, 0)
        continue
    block, whole, state, text = blocks[resolved]
    assert fnv(text) == whole, ('Capture hash mismatch', key)
    signature, body = text.split('\n', 1)
    calculated[key] = (resolved, fnv(signature + '\n'), fnv(body))
    fixture_blocks[resolved] = block
    print(key, state, len(block.splitlines()), calculated[key])
for key in CONTROLS:
    assert calculated[key] == existing[key], ('Existing reference format control', key, calculated[key], existing[key])
for key in NEW:
    assert existing.get(key) == calculated[key], ('Captured reference differs', key)
print('PASS captured 0.9.0n references: fourteen rows, two existing native/Blueprint controls.')
