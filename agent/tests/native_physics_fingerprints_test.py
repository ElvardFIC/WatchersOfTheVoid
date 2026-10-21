"""Regenerate native physics signature rows from UE4.27 declarations.

New signatures are derived, not live-captured. Three existing captured signatures
control the parameter/type/flag encoding and the empty native body hash.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc'
SOURCES = (
    'https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/UPrimitiveComponent/AddForce?application_version=4.27',
    'https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/UPrimitiveComponent/SetEnableGravity?application_version=4.27',
)
SIGNATURES = {
    'addimpulse': 'impulse:P:structproperty:vector,bonename:P:nameproperty,bvelchange:P:boolproperty',
    'setphysicslinearvelocity': 'newvel:P:structproperty:vector,baddtocurrent:P:boolproperty,bonename:P:nameproperty',
    'setphysicsangularvelocityindegrees': 'newangvel:P:structproperty:vector,baddtocurrent:P:boolproperty,bonename:P:nameproperty',
    'addforce': 'force:P:structproperty:vector,bonename:P:nameproperty,baccelchange:P:boolproperty',
    'setenablegravity': 'bgravityenabled:P:boolproperty',
}
DERIVED = {'addforce', 'setenablegravity'}

def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value

pattern = re.compile(r'^    \{L"primitivecomponent", L"([^"]+)", L"primitivecomponent\.[^"]+", 0x([0-9a-f]+)ull, 0x([0-9a-f]+)ull\},$', re.M)
text = TABLE.read_text(encoding='utf-8')
rows = {name: (int(signature, 16), int(body, 16)) for name, signature, body in pattern.findall(text)}
expected = {name: (fnv('sig(' + parameters + ')\n'), fnv('')) for name, parameters in SIGNATURES.items()}
for name in SIGNATURES.keys() - DERIVED:
    assert rows[name] == expected[name], ('captured format control', name, rows[name], expected[name])
if '--write' in sys.argv:
    for name in sorted(DERIVED):
        if name in rows:
            assert rows[name] == expected[name]
            continue
        signature, body = expected[name]
        line = f'    {{L"primitivecomponent", L"{name}", L"primitivecomponent.{name}", 0x{signature:016x}ull, 0x{body:016x}ull}},\n'
        candidates = list(pattern.finditer(text))
        later = next((match for match in candidates if match.group(1) > name), None)
        position = later.start() if later else candidates[-1].end() + 1
        text = text[:position] + line + text[position:]
    TABLE.write_text(text, encoding='utf-8', newline='\n')
rows = {name: (int(signature, 16), int(body, 16)) for name, signature, body in pattern.findall(text)}
for name, value in expected.items():
    assert rows[name] == value, (name, rows.get(name), value)
print('PASS native physics fingerprints: three captured controls, two documented derived signatures and empty native bodies')
