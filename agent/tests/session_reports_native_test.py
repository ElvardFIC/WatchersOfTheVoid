"""Check the native contracts behind the new hole/console/can adapters."""
from pathlib import Path
import sys

workspace = Path(__file__).resolve().parents[3]
research = workspace / "_research"
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

assets = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else research / "current-assets"


def functions(asset):
    package = Package(str(assets / f"VotV__Content__{asset}.uasset"))
    result = {}
    for export in package.exports:
        if package.class_of_export(export) == "Function":
            result[export["name"]] = "\n".join(line for _, line in body(package, export))
    return result


player = functions("main__mainPlayer")["ExecuteUbergraph_mainPlayer"]
assert "obj:/Game/objects/dirthole.dirthole_C" in player
assert "name:matType" in player and "GameplayStatics.FinishSpawningActor" in player
hole = functions("objects__dirthole")
assert "getData" not in hole and "loadData" not in hole
assert "Inst(matType)" in hole["UserConstructionScript"]
for material in ("inst_dirthole_S_sand", "inst_dirthole_S_gravel", "inst_dirthole_S"):
    assert material in hole["UserConstructionScript"]
assert "Inst(hole)->SetMaterial" in hole["UserConstructionScript"]
assert "SphereOverlapActors" in hole["ExecuteUbergraph_dirthole"]
assert "dirthole_item" in hole["ExecuteUbergraph_dirthole"]
assert "GameplayStatics.BeginDeferredActorSpawnFromClass" in hole["ExecuteUbergraph_dirthole"]
console = functions("objects__panel_SATconsole")["ExecuteUbergraph_panel_SATconsole"]
assert console.index("Inst(widget)->Inst(used), self") < console.index("->Enter Interface(Inst(widget)")
can = functions("objects__prop_cannedFood")
assert "[Inst(type), Inst(foods)]" in can["getData"]
assert "[Inst(opened)]" in can["getData"]
assert "Inst(foods) =" not in can["open"]
assert "Subtract_IntInt(Inst(foods), 1)" in can["eat"]
print("PASS native hole spawn/material/treasure, physical console owner and non-refillable can contracts")
