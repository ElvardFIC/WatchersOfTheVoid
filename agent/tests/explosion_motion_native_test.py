"""Verify overlap selection and delayed impulse in the existing native corpus."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
research = root.parent / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package, Dis, find_script

# Engine native FireImpulse() has no reflected arguments or return value:
# https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/URadialForceComponent
# Use a live 0.9.0n capture, with two existing engine functions as format controls.
def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value

table_path = root / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc'
table = table_path.read_text(encoding='utf-8')
signature, native_body = fnv('sig()\n'), fnv('')
capture = Path(__file__).with_name('explosion_impulse_capture_0.9.0n.txt').read_text(encoding='utf-8')
records = re.findall(r'^### (\S+) ([0-9a-f]{16}) native\n(.*?)(?=^### |\Z)', capture, re.M | re.S)
assert {name for name, _, _ in records} == {'actor.k2_destroyactor', 'actorcomponent.deactivate', 'radialforcecomponent.fireimpulse'}
for name, whole, body in records:
    assert body == 'sig()\n' and fnv(body) == int(whole, 16), ('capture integrity', name)
for cls, name in [('actor', 'k2_destroyactor'), ('actorcomponent', 'deactivate')]:
    control = re.search(r'\{L"' + cls + r'", L"' + name + r'", L"[^"\n]+", 0x([0-9a-f]+)ull, 0x([0-9a-f]+)ull\}', table)
    assert control and tuple(int(x, 16) for x in control.groups()) == (signature, native_body)
row = f'    {{L"radialforcecomponent", L"fireimpulse", L"radialforcecomponent.fireimpulse", 0x{signature:016x}ull, 0x{native_body:016x}ull}},\n'
if '--write' in sys.argv and row not in table:
    pos = table.index('    {L"scenecomponent",')
    table = table[:pos] + row + table[pos:]
    table_path.write_text(table, encoding='utf-8', newline='\n')
assert row in table

p = Package(str(research / 'assets/VotV__Content__objects__misc__explosion.uasset'))
e = next(e for e in p.exports if e['name'] == 'ExecuteUbergraph_explosion')
code, _ = find_script(p.export_data(e))
rows = dict(Dis(p, code).run())
assert 'Greater_FloatFloat(Inst(damage), 0)' in rows[290]
assert 'jump @4728' in rows[324]  # Force-only blasts do not visit the damage overlap.
assert 'SphereOverlapActors(' in rows[497]
assert '->reachedByExplosion(' in rows[1626]
assert 'Delay(self, 0.1' in rows[4728]
assert rows[4783] == 'Inst(RadialForce)->FireImpulse()'
assert 497 < 1626 < 4728 < 4783
salt = Package(str(research / 'assets/VotV__Content__objects__misc__explosion_saltrocket.uasset'))
assert not any(salt.class_of_export(e) == 'Function' for e in salt.exports)

sub = (root / 'src/votv-coop/src/coop/session/subsystems.cpp').read_text()
assert sub.index('explosion_prop_motion::Install(&session)') < sub.index('native_explosion_sync::Install(&session)')
assert sub.index('explosion_prop_motion::Tick()') < sub.index('prop_drive_host::Pump(session, isHost)')
assert 'explosion_prop_motion::OnDisconnect()' in sub
wrapper = (root / 'src/votv-coop/src/ue_wrap/actors/radial_impulse.cpp').read_text()
assert 'L"Radius"' in wrapper and 'L"ImpulseStrength"' in wrapper and 'L"ObjectTypesToAffect"' in wrapper
assert 'L"SphereOverlapActors"' in wrapper and 'L"K2_GetComponentLocation"' in wrapper
assert 'CollectTracked' not in wrapper and 'NumObjects' not in wrapper
print('PASS existing native corpus: actual overlap targets precede latent radial impulse; salt inheritance and publisher ordering')

capture=(root/'src/votv-coop/src/coop/world/explosion_sync.cpp').read_text()
assert 'explosion_prop_motion::ObserveBlast(c.object)' in capture
assert capture.index('explosion_prop_motion::ObserveBlast(c.object)') < capture.index('if(!s->AnyWorldReadyPeer())')
assert 'coop::explosion_sync::Install(&session)' not in sub
assert 'coop::explosion_events::Install(&session)' not in sub
print('PASS merged blast pipeline: one native capture, retained native-filter discovery and both coast feeds before the shared drive tick')
