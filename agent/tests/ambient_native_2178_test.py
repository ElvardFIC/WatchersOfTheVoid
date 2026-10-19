"""Check ambient presentation independently of persistent root/actor settings."""
from pathlib import Path
import struct
import sys

checkout = Path(__file__).resolve().parents[2]
research = next((p for p in (checkout / "_research", checkout.parent / "_research")
                 if (p / "kismet_dis.py").is_file()), None)
if research is None:
    raise SystemExit("UNAVAILABLE: local disassembler and extracted ambient/root assets required")
sys.path.insert(0, str(research))
from kismet_dis import Package, Dis

def body(asset, name):
    pkg = Package(str(research / f"assets/VotV__Content__objects__{asset}.uasset"))
    export = next(e for e in pkg.exports if e["name"] == name and pkg.class_of_export(e) == "Function")
    data = pkg.export_data(export)
    candidates = []
    # The older find_script heuristic picks a false header in setActive's property
    # metadata. Require a complete parse and the serialized VM byte count as well.
    for tail in (12, 14):
        for pos in range(len(data) - 8 - tail + 1):
            vm_size, disk_size = struct.unpack_from("<ii", data, pos)
            end = pos + 8 + disk_size
            if vm_size <= 0 or disk_size <= 0 or end + tail != len(data) or data[end - 1] != 0x53:
                continue
            dis = Dis(pkg, data[pos + 8:end])
            rows = dict(dis.run())
            if dis.mem == vm_size and not any("!! " in v for v in rows.values()):
                candidates.append(rows)
    assert len(candidates) == 1, (asset, name, len(candidates))
    return candidates[0]

active = body("ambientLight", "setActive")
assert active[0] == "Inst(bottomLight)->SceneComponent.SetVisibility(Local(active), false)"
assert "BooleanAND(Local(active), Inst(enableTopLight))" in active[42]
assert "Inst(topLight)->SceneComponent.SetVisibility(" in active[80]
assert not any("Inst(isActive)" in v or "SetActorHiddenInGame" in v for v in active.values())
settings = "\n".join(body("ambientLight", "updVisible").values())
assert "enableAmbientLight_" in settings and "SetActorHiddenInGame(" in settings
assert "SetVisibility(" not in settings
root = body("triggers__trigger_lightRoot", "ExecuteUbergraph_trigger_lightRoot")
assert "setActive(Inst(isActive))" in root[593] and "setActive(false)" in root[1589]
print("PASS ambient native bottom/top visibility, independent user setting, and root flicker calls")
