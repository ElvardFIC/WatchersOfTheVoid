"""Verify/generate the native camera rows from a real 0.9.0n function capture.

The fixture is a four-function excerpt of fingerprint_capture from the isolated
game: two new SceneComponent dependencies and two existing format controls.
Use --capture <full capture> --write to regenerate only the two new rows.
"""
from pathlib import Path
import argparse
import re

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc'
FIXTURE = Path(__file__).with_name('ragdoll_transition_capture_0.9.0n.txt')
NEW = {'scenecomponent.k2_attachtocomponent', 'scenecomponent.k2_setrelativerotation'}
CONTROLS = {'gameplaystatics.finishspawningactor', 'primitivecomponent.setphysicslinearvelocity'}


def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value


def rows(text):
    pattern = r'\{L"([^"]+)",\s*L"([^"]+)",\s*L"([^"]+)",\s*0x([0-9a-f]+)ull,\s*0x([0-9a-f]+)ull\}'
    return {f'{cls}.{name}': (path, int(sig, 16), int(body, 16))
            for cls, name, path, sig, body in re.findall(pattern, text)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, default=FIXTURE)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    capture = args.capture.read_text(encoding='utf-8')
    native = {}
    pattern = r'^### ([^\n ]+) ([0-9a-f]{16}) native\n(sig\([^\n]*\)\n)'
    for path, whole, signature in re.findall(pattern, capture, re.M):
        if path in NEW | CONTROLS:
            assert fnv(signature) == int(whole, 16), ('capture integrity', path)
            native[path] = (path, fnv(signature), fnv(''))
    assert set(native) == NEW | CONTROLS, 'Missing captured native dependencies/controls'
    text = TABLE.read_text(encoding='utf-8')
    existing = rows(text)
    for path in CONTROLS:
        assert existing[path] == native[path], ('existing captured format control', path)
    if args.write:
        marker = '    {nullptr, nullptr, nullptr, 0, 0}'
        assert marker in text, 'Missing reference-table terminator'
        additions = []
        for path in sorted(NEW):
            if path in existing:
                assert existing[path] == native[path], ('existing row differs', path)
                continue
            cls, name = path.split('.')
            _, signature, body = native[path]
            additions.append(f'    {{L"{cls}", L"{name}", L"{path}", '
                             f'0x{signature:016x}ull, 0x{body:016x}ull}},\n')
        text = text.replace(marker, ''.join(additions) + marker, 1)
        TABLE.write_text(text, encoding='utf-8', newline='\n')
    actual = rows(text)
    for path in NEW:
        assert actual.get(path) == native[path], ('native camera reference mismatch', path)
    print('PASS native ragdoll camera fingerprints: two live-captured rows and two existing controls')


if __name__ == '__main__':
    main()
