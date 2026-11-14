"""Verify current Madness HUD paths without replaying enemy-spawning callbacks."""
from pathlib import Path
import sys

research = Path(__file__).resolve().parents[3] / "_research"
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

assets = Path(sys.argv[1]) if len(sys.argv) > 1 else research / "current-assets"

def graph(asset, name):
    package = Package(str(assets / ("VotV__Content__" + asset + ".uasset")))
    export = next(e for e in package.exports
                  if e["name"] == name and package.class_of_export(e) == "Function")
    rows = [text for _, text in body(package, export)]
    assert not any("NO VALID SCRIPT" in text for text in rows)
    return rows

master = graph("objects__misc__madnessCombatMaster", "ExecuteUbergraph_madnessCombatMaster")
assert any("WidgetBlueprintLibrary.Create" in text and "ui_madnessCombat_C" in text for text in master)
assert any("Inst(killcount)" in text and "AddToViewport(0)" in text for text in master)
assert any("Inst(killcount)->RemoveFromParent()" in text for text in master)
count = graph("umg__interfaces__ui_madnessCombat", "addCount")
assert any("Inst(count) =" in text for text in count)
assert any("Inst(text_count)->SetText" in text for text in count)
assert any("Inst(master)->killed()" in text for text in count)
grunt = graph("objects__misc__madnessCombatMaster", "spawnGrunt")
assert any("BeginDeferredActorSpawnFromClass" in text and "prop_grunt_C" in text for text in grunt)
assert any("name:killcount, Inst(killcount)" in text for text in grunt)
print("PASS native Madness HUD creation/count/text and master kill/spawnGrunt dependencies; addCount is not a cosmetic replay")
