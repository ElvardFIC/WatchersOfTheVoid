"""Verify the native takeObj contract before relying on its successful result."""
from pathlib import Path
import re
import sys

research = Path(__file__).resolve().parents[3] / "_research"
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body


def function(asset, name):
    package = Package(str(research / "current-assets" / f"VotV__Content__objects__{asset}.uasset"))
    export = next(e for e in package.exports if e["name"] == name and package.class_of_export(e) == "Function")
    return "\n".join(line for _, line in body(package, export))


take = function("propInventory", "takeObj")
assert "pop_flow_if_not(Local(removeVol))" in take
assert "GameplayStatics.FinishSpawningActor" in take
assert "->loadData(" in take
assert take.index("->loadData(") < take.index("Out(object), Local(CallFunc_FinishSpawningActor_ReturnValue)")
assert "Out(return) = true" in take and "Out(return) = false" in take
assert "op5F(Out(object), noobj)" in take
assert "Array_Remove" in take
extract = function("prop_container", "extract")
assert "->takeObj(Local(index), false," in extract
assert extract.index("->putObjectInventory2(") < extract.index("->loadData(")
print("PASS native extraction: successful restored object out, failed null out, saved-row removal and separate record-only container path")

root = Path(__file__).resolve().parents[2]
subsystems = (root / 'src/votv-coop/src/coop/session/subsystems.cpp').read_text(encoding='utf-8-sig')
subsystems = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/', '', subsystems)
install = re.search(r'^void Install\(coop::net::Session& session\) \{(.*?)^\}', subsystems, re.M | re.S)
assert install and re.search(r'\bcoop::prop_lifecycle::InstallInventory\s*\(\s*&session\s*\)\s*;', install[1]), \
    'Retained native extraction publisher must be installed and drained by the active session pump'
print('PASS retained extraction publisher is wired into the production session install pump')
