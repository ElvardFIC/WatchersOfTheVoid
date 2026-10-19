"""Validate the separate lamp phase against locally extracted game bytecode."""
from pathlib import Path
import sys

checkout = Path(__file__).resolve().parents[2]
research = next((p for p in (checkout / "_research", checkout.parent / "_research")
                 if (p / "kismet_dis.py").is_file()), None)
if research is None:
    raise SystemExit("UNAVAILABLE: local disassembler and extracted lamp/root/ticker assets required")
sys.path.insert(0, str(research))
from kismet_dis import Package, Dis, find_script

def body(asset, name):
    pkg = Package(str(research / f"assets/VotV__Content__objects__{asset}.uasset"))
    export = next(e for e in pkg.exports if e["name"] == name and pkg.class_of_export(e) == "Function")
    code, _ = find_script(pkg.export_data(export))
    assert code is not None, name
    rows = dict(Dis(pkg, code).run())
    assert not any("!! " in line for line in rows.values()), name
    return rows

lamp = body("ceilingLamp", "setActive")
assert lamp[0] == "Inst(isActive) = Local(isActive)" and lamp[19] == "setVis()"
root = "triggers__trigger_lightRoot"
assert body(root, "flick")[0] == "ExecuteUbergraph_trigger_lightRoot(2178)"
graph = body(root, "ExecuteUbergraph_trigger_lightRoot")
phases = "\n".join(v for k, v in graph.items() if 88 <= k < 1853)
assert "setActive(false)" in phases and "setActive(Inst(isActive))" in phases
assert "Delay(" in phases and "RandomFloatInRange(" in phases
assert "Inst(isActive) =" not in phases
assert any("setActive(Inst(isActive))" in v for v in body(root, "updLig").values())
ticker = "\n".join(body("tickers__ticker_flickerer", "ExecuteUbergraph_ticker_flickerer").values())
assert "flick()" in ticker and "RandomFloatInRange(" in ticker
print("PASS native random flicker changes lamp presentation without changing the root active flag")
