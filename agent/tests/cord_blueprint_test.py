"""Validate compiled-game assumptions against the extracted VotV 0.9.0n asset, not a handwritten fixture."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root.parent / "_research"))
from kismet_dis import Package, Dis, find_script

pkg = Package(str(root.parent / "_research/assets/VotV__Content__objects__cord.uasset"))
def body(name):
    export = next(e for e in pkg.exports if e["name"] == name and pkg.class_of_export(e) == "Function")
    code, _ = find_script(pkg.export_data(export))
    assert code is not None, name
    rows = dict(Dis(pkg, code).run())
    assert not any("!! " in line for line in rows.values()), name
    return rows

header = (root / "src/votv-coop/include/ue_wrap/devices/cord_blueprint.h").read_text()
entry = {key: int(value) for key, value in re.findall(r"int32_t (k\w+) = (\d+);", header)}
graph = body("ExecuteUbergraph_cord")
assert "Inst(head)->SceneComponent.K2_AttachToComponent(Inst(DefaultSceneRoot)" in graph[entry["kReattachResume"]]
assert "K2_SetRelativeTransform" in graph[153] and graph[205] == "pop_flow"
assert f"skipconst:{entry['kReattachResume']}," in graph[2002] and "Delay(self, 0," in graph[2002]
assert "BreakConstraint" in graph[2057] and graph[2089] == "jump @2002"
assert "Inst(player)" in graph[entry["kAction"]] and graph[281] == "disconnect()"
assert "K2Node_CustomEvent_pressed" in graph[entry["kUsed"]] and graph[2469] == "disconnect()"
assert graph[entry["kDestroyed"]] == "disconnect()"
assert "tryToConnect" in graph[2333] and graph[2432] == "Inst(drag) = false" and graph[2443] == "breakInput()"
connect = body("connect")
assert any("disconnect()" in line for line in connect.values())
disconnect = body("disconnect")
assert disconnect[254] == "reattach()"
attempt = body("tryToConnect")
assert attempt[574].startswith("connect(")
assert "Out(AsCord Socket)" in attempt[606] and "K2Node_DynamicCast_AsCord_Socket" in attempt[606]
assert attempt[625] == "jump @641"  # no post-connect socket test: cancel still ends local drag
print("PASS: actual cord Blueprint entries, delayed reattach, user disconnects and cancelled-connect return path")
