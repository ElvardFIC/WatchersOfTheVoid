"""Validate the blood contract and reference against a real 0.9.0n capture."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
capture = Path(__file__).with_name('blood_capture_0.9.0n.txt').read_text(encoding='utf-8')
table = (ROOT / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc').read_text(encoding='utf-8')


def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value


functions = {}
for path, whole, text in re.findall(r'^### (\S+) ([0-9a-f]{16}) ok\n(.*?)(?=^### |\Z)', capture, re.M | re.S):
    assert fnv(text) == int(whole, 16), ('capture integrity', path)
    functions[path] = text
drop = functions['blooddecalmaster_c.addparticle']
sig, body = drop.split('\n', 1)
assert sig == 'sig(location:P:structproperty:vector,velocity:P:structproperty:vector,size:P:floatproperty,type:CP:intproperty)'
row = re.search(r'\{L"blooddecalmaster_c", L"addparticle", L"blooddecalmaster_c.addparticle", 0x([0-9a-f]+)ull, 0x([0-9a-f]+)ull\}', table)
assert row and tuple(int(v, 16) for v in row.groups()) == (fnv(sig + '\n'), fnv(body))
assert 'o:gameplaystatics.spawnemitterattached' in body and 'o:eff_blooddripdrop' in body
assert 'n:setphysicslinearvelocity' in body
assert 'add player damage' not in body and 'addtoviewport' not in body
timer = functions['effect_bloodloss_c.droptimer']
assert 'VirtualFunction(n:addparticle,' in timer
assert 'p:maingamemode_c:masteblooddecals' in timer
assert 'n:add player damage' in timer
assert 'LocalVariable(l0:callfunc_getplayercameramanager_returnvalue)' in timer
print('PASS native blood: captured addParticle reference and blood-loss timer separate world drops from local damage')
