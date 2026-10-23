"""Verify ATV steering dependencies against the real 0.9.0n capture."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
capture = Path(__file__).with_name('atv_steering_capture_0.9.0n.txt').read_text(encoding='utf-8')
table = (ROOT / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc').read_text(encoding='utf-8')


def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value


functions = {}
for path, whole, text in re.findall(r'^### (\S+) ([0-9a-f]{16}) ok\n(.*?)(?=^### |\Z)', capture, re.M | re.S):
    assert fnv(text) == int(whole, 16), ('capture integrity', path)
    signature, body = text.split('\n', 1)
    cls, name = path.split('.')
    row = re.search(r'\{L"' + cls + r'", L"' + name + r'", L"' + path + r'", 0x([0-9a-f]+)ull, 0x([0-9a-f]+)ull\}', table)
    assert row and tuple(int(v, 16) for v in row.groups()) == (fnv(signature + '\n'), fnv(body))
    functions[name] = text
assert set(functions) == {'receivetick', 'setfrontwheelsorientation'}
tick = functions['receivetick']
assert 'p:atv_c:handleaxis' in tick and 'o:scenecomponent.k2_setrelativerotation' in tick
assert 'n:setfrontwheelsorientation' in tick and 'p:atv_c:rotalpha' in tick
assert 'p:atv_c:input_left' in tick and 'p:atv_c:input_right' in tick
wheel = functions['setfrontwheelsorientation']
assert wheel.startswith('sig(a:P:floatproperty)\n')
assert 'f:3f000000' in wheel and 'o:physicsconstraintcomponent.setconstraintreferenceorientation' in wheel
assert 'o:physicsconstraintcomponent.setconstraintreferenceposition' in wheel
print('PASS native ATV steering: live tick/constraint references, handle yaw and native half-angle wheel reducer')
