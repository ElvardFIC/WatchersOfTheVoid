"""Check chair attachment assumptions against the current cooked local game."""
from pathlib import Path
import sys

research = Path(__file__).resolve().parents[3] / "_research"
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

assets = Path(sys.argv[1]) if len(sys.argv) > 1 else research / "current-assets"

def graph(asset, function):
    path = assets / ("VotV__Content__" + asset + ".uasset")
    package = Package(str(path))
    export = next(e for e in package.exports
                  if e["name"] == function and package.class_of_export(e) == "Function")
    rows = [text for _, text in body(package, export)]
    assert not any("NO VALID SCRIPT" in text for text in rows)
    return rows

box = graph("objects__sitBox", "ExecuteUbergraph_sitBox")
assert any("GetParentActor" in text for text in box)
assert any("->sit(Local(CallFunc_GetParentActor_ReturnValue_1), Inst(sitting), self)" in text for text in box)
sit = graph("main__mainPlayer", "sit")
assert any("op5F(Inst(sittingOn), Local(actor))" in text for text in sit)
main = graph("main__mainPlayer", "ExecuteUbergraph_mainPlayer")
assert any("Actor.K2_AttachToActor(Inst(sittingOn)" in text for text in main)
unsit = graph("main__mainPlayer", "unsit")
assert any("op5F(Inst(sittingOn), noobj)" in text for text in unsit)
print("PASS native sitBox passes its parent chair; mainPlayer retains/attaches to sittingOn and clears it on unsit")
