"""Current cooked native ordering behind LOCAL extraction recovery/refund."""
from pathlib import Path
import re
import sys

research = Path(__file__).resolve().parents[3] / "_research"
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body


def function(asset, name):
    package = Package(str(research / "current-assets" / f"VotV__Content__{asset}.uasset"))
    export = next(e for e in package.exports if e["name"] == name and package.class_of_export(e) == "Function")
    return list(body(package, export))


take = "\n".join(line for _, line in function("objects__propInventory", "takeObj"))
assert "pop_flow_if_not(Local(removeVol))" in take
assert "Local(r) = op6B(" in take and "Array_Remove(" in take
assert take.index("Local(r) = op6B(") < take.index("Array_Remove(") < take.index("Out(return) = true")
assert "Out(Output) = Local(r)" in take
assert "if not (Local(CallFunc_loadData_return))" not in take

extract = function("objects__prop_container", "extract")
text = "\n".join(line for _, line in extract)
assert "->takeObj(Local(index), false," in text
put = next(i for i, (_, line) in enumerate(extract) if "->putObjectInventory2(" in line)
assert extract[put + 1][1] == "pop_flow_if_not(Local(CallFunc_putObjectInventory2_return))"
resume = int(next(re.search(r"push_flow @(\d+)", line).group(1)
                  for _, line in reversed(extract[:put]) if "push_flow @" in line))
load = next(offset for offset, line in extract if "->loadData(" in line)
remove_volume = next(offset for offset, line in extract if "->removeVol(" in line)
assert resume > load and resume > remove_volume
assert dict(extract)[resume] == "dingus()"  # false pocket result skips all carrier restoration

names = "\n".join(line for _, line in function("objects__propInventory", "recalculateNames"))
volume = "\n".join(line for _, line in function("objects__prop_container", "updateVolumesAndMass"))
assert "nameData" in names and "Array_Clear" in names and "Array_Add" in names
assert "nameData" in volume and "Get Volume" in volume
print("PASS cooked LOCAL extraction: record-only debit, failed pocket skips loadData, unchecked take load result, names-before-volume dependency")

# Preserve the exact captured text behind the new dependency; the existing
# takeObj reference is an independent format/hash control.
def fnv(text):
    value = 0xcbf29ce484222325
    for byte in text.encode('utf-8'):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value

capture = Path(__file__).with_name('container_recovery_reference_0.9.0n.txt').read_text(encoding='utf-8')
table = (Path(__file__).resolve().parents[2] / 'src/votv-coop/include/ue_wrap/facts/fingerprints_0.9.0n.inc').read_text(encoding='utf-8')
for key in ('prop_container_c.extract', 'propinventory_c.takeobj'):
    block = re.search(r'^### ' + re.escape(key) + r' ([0-9a-f]{16}) (ok|native)\n(.*?)(?=^### |\Z)', capture, re.M | re.S)
    assert block and fnv(block[3]) == int(block[1], 16), key
    signature, body_text = block[3].split('\n', 1)
    cls, name = key.split('.')
    expected = f'{{L"{cls}", L"{name}", L"{key}", 0x{fnv(signature + chr(10)):016x}ull, 0x{fnv(body_text):016x}ull}}'
    assert expected in table, key
print('PASS exact saved extraction reference and existing takeObj fingerprint control')
