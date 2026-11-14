"""Derived UE4.27 native signatures; live VOTV census acceptance is still pending.

The known captured SpawnSoundAttached signature controls the encoding. Native
bodies are empty. Parameter declarations also match the supplied Lua calls.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc'
SOURCES = (
    'https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Animation/UAnimInstance/PlaySlotAnimationAsDynamicMontag-?application_version=4.27',
    'https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/SpawnSound2D',
)
SIGNATURES = {
    'animinstance.playslotanimationasdynamicmontage': 'asset:P:objectproperty:animsequencebase,slotnodename:P:nameproperty,blendintime:P:floatproperty,blendouttime:P:floatproperty,inplayrate:P:floatproperty,loopcount:P:intproperty,blendouttriggertime:P:floatproperty,intimetostartmontageat:P:floatproperty,returnvalue:POR:objectproperty:animmontage',
    'gameplaystatics.spawnsound2d': 'worldcontextobject:CP:objectproperty:object,sound:P:objectproperty:soundbase,volumemultiplier:P:floatproperty,pitchmultiplier:P:floatproperty,starttime:P:floatproperty,concurrencysettings:P:objectproperty:soundconcurrency,bpersistacrossleveltransition:P:boolproperty,bautodestroy:P:boolproperty,returnvalue:POR:objectproperty:audiocomponent',
}

def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value

def rows(text):
    pattern = r'\{L"[^"]+", L"[^"]+", L"([^"]+)", 0x([0-9a-f]+)ull, 0x([0-9a-f]+)ull\}'
    return {key: (int(signature, 16), int(body, 16)) for key, signature, body in re.findall(pattern, text)}

text = TABLE.read_text(encoding='utf-8')
actual = rows(text)
capture = (ROOT / 'agent/tests/ci_reference_capture_0.9.0n.txt').read_text(encoding='utf-8')
control = re.search(r'### gameplaystatics.spawnsoundattached[^\n]+\nsig\(([^\n]+)\)', capture).group(1)
assert actual['gameplaystatics.spawnsoundattached'] == (fnv('sig(' + control + ')\n'), fnv(''))
expected = {key: (fnv('sig(' + signature + ')\n'), fnv('')) for key, signature in SIGNATURES.items()}
if '--write' in sys.argv:
    marker = '    {nullptr, nullptr, nullptr, 0, 0}'
    assert marker in text
    additions = []
    for key, (signature, body) in expected.items():
        if key in actual:
            assert actual[key] == (signature, body)
            continue
        cls, verb = key.split('.')
        additions.append(f'    {{L"{cls}", L"{verb}", L"{key}", 0x{signature:016x}ull, 0x{body:016x}ull}},\n')
    TABLE.write_text(text.replace(marker, ''.join(additions) + marker, 1), encoding='utf-8', newline='\n')
actual = rows(TABLE.read_text(encoding='utf-8'))
for key, hashes in expected.items():
    assert actual.get(key) == hashes, (key, actual.get(key), hashes)
print('PASS EmoteWheel native fingerprints: captured encoding control and two documented derived signatures; live census pending')
