"""Check the cord assumptions against locally extracted game bytecode, when available."""
from pathlib import Path
import sys

checkout = Path(__file__).resolve().parents[2]
research = next((p for p in (checkout / "_research", checkout.parent / "_research")
                 if (p / "kismet_dis.py").is_file()
                 and (p / "assets/VotV__Content__objects__cord.uasset").is_file()), None)
if research is None:
    raise SystemExit("UNAVAILABLE: extracted VotV 0.9.0n cord asset and local disassembler required")
sys.path.insert(0, str(research))
from kismet_dis import Package, Dis, find_script

pkg = Package(str(research / "assets/VotV__Content__objects__cord.uasset"))
def body(name):
    export = next(e for e in pkg.exports if e["name"] == name and pkg.class_of_export(e) == "Function")
    code, _ = find_script(pkg.export_data(export))
    assert code is not None, name
    rows = dict(Dis(pkg, code).run())
    assert not any("!! " in line for line in rows.values()), name
    return rows

graph = body("ExecuteUbergraph_cord")
assert "Inst(root)->SceneComponent.K2_GetComponentLocation" in graph[1698]
assert "Inst(head)->SceneComponent.K2_GetComponentLocation" in graph[1748]
assert "Multiply_FloatFloat(Inst(cordMaxLength), 1.2)" in graph[1844]
assert "Add_FloatFloat(Local(CallFunc_Multiply_FloatFloat_ReturnValue), 100)" in graph[1886]
assert "GreaterEqual_FloatFloat" in graph[1928]
assert graph[1987] == "disconnect()"
assert "tryToConnect(false, 5, false" in graph[2333]
assert graph[2432] == "Inst(drag) = false" and graph[2443] == "breakInput()"
attempt = body("tryToConnect")
assert "SphereOverlapActors" in attempt[313]
assert "cordSocket_C" in attempt[313]
assert attempt[574].startswith("connect(")
assert "Out(AsCord Socket)" in attempt[606] and attempt[625] == "jump @641"
connect = "\n".join(body("connect").values())
assert "Inst(cord)" in connect and "disconnect()" in connect and "Local(load)" in connect
print("PASS native cord bytecode: socket selection, cancelled-connect return and cable disconnect boundary")
