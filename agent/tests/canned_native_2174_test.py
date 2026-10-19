"""Validate the can-record assumptions against locally extracted game bytecode."""
from pathlib import Path
import sys

checkout = Path(__file__).resolve().parents[2]
research = next((p for p in (checkout / "_research", checkout.parent / "_research")
                 if (p / "kismet_dis.py").is_file()
                 and (p / "assets/VotV__Content__objects__prop_cannedFood.uasset").is_file()), None)
if research is None:
    raise SystemExit("UNAVAILABLE: extracted canned-food and prop assets plus local disassembler required")
sys.path.insert(0, str(research))
from kismet_dis import Package, Dis, find_script

can = Package(str(research / "assets/VotV__Content__objects__prop_cannedFood.uasset"))
base = Package(str(research / "assets/VotV__Content__objects__prop.uasset"))

def body(pkg, name):
    export = next(e for e in pkg.exports if e["name"] == name and pkg.class_of_export(e) == "Function")
    code, _ = find_script(pkg.export_data(export))
    assert code is not None, name
    rows = dict(Dis(pkg, code).run())
    assert not any("!! " in line for line in rows.values()), name
    return rows

cls = next(e for e in can.exports if e["name"] == "prop_cannedFood_C")
assert can.obj(cls["sup"]).endswith(".prop_C")
opened = body(can, "open")
eaten = body(can, "eat")
assert opened[19] == "Inst(opened) = true" and opened[90] == "init()"
assert "Subtract_IntInt(Inst(foods), 1)" in eaten[331]
assert eaten[400].startswith("Inst(foods) = ") and eaten[487] == "init()"
assert not any("upd(" in line for line in (*opened.values(), *eaten.values()))
capture = body(can, "getData")
assert "prop_C.getData(" in capture[0]
assert "[Inst(type), Inst(foods)]" in capture[19]
assert "[Inst(opened)]" in capture[48]
# The base writes no ints, and the can preserves bool group0 while adding opened as group1.
assert not any(".ints_" in line for line in body(base, "getData").values())
assert ".bools_" in capture[160] and ", 0," in capture[160]
assert "[Local(CallFunc_Array_Get_Item), Local(K2Node_MakeStruct_struct_mBool)]" in capture[224]
restore = body(can, "loadData")
assert restore[138].startswith("Inst(opened) = ")
assert restore[285].startswith("Inst(type) = ")
assert restore[440].startswith("Inst(foods) = ")
assert "prop_C.loadData(" in restore[468]
assert body(base, "loadData")[1159] == "init()"
look = body(can, "init")
assert "Inst(foods)" in look[372] and "Inst(opened)" in look[406]
assert look[525].startswith("Inst(name) = switch(") and look[904] == "prop_C.init()"
print("PASS native can open/eat, non-overlapping save fields and loadData -> init mesh refresh")
