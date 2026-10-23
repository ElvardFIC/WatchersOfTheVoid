"""Check the baked personal address and native fixed-slot inventory semantics."""
from pathlib import Path
import struct
import sys
research=Path(__file__).resolve().parents[3]/"_research"
sys.path.insert(0,str(research))
from kismet_dis import Package,Reader
from dump_2135 import body

def package(asset):
    return Package(str(research/"current-assets"/f"VotV__Content__{asset}.uasset"))

def functions(p):
    return {e["name"]:"\n".join(line for _,line in body(p,e)) for e in p.exports if p.class_of_export(e)=="Function"}

p=package("objects__prop_inventoryContainer_player")
component=next(e for e in p.exports if e["name"]=="propInventory_GEN_VARIABLE")
reader=Reader(p.export_data(component))
props={}
while True:
    name=p.fname(reader)
    if name=="None":break
    typ=p.fname(reader);size=reader.i32();index=reader.i32()
    assert index==0 and typ in ("IntProperty","BoolProperty","FloatProperty"),(name,typ)
    value=reader.u8() if typ=="BoolProperty" else None
    guid=reader.u8()
    if guid:reader.p+=16
    if typ=="IntProperty":value=reader.i32();assert size==4
    elif typ=="FloatProperty":value=reader.f32();assert size==4
    else:assert size==0
    props[name]=(typ,value)
assert props["index"]==("IntProperty",0)
assert props["player"]==("BoolProperty",1)
assert props["customVolume"]==("FloatProperty",50000.0)
f=functions(p)
assert f["loadData"]=="Out(return) = true\nreturn nothing\nend_of_script"
inv=functions(package("objects__propInventory"))
assert "GreaterEqual_IntInt(Inst(index), 0)" in inv["init"]
assert "Inst(index) = Local(CallFunc_Array_Add_ReturnValue)" in inv["init"]
assert "Inst(GObjStack), Inst(index))" in inv["addObject"]
equipment=functions(package("main__mainGamemode"))["AddEquipment"]
assert "Array_Find" in equipment and "Array_Set" in equipment
assert "Not enough space" in equipment
print("PASS cooked personal component index=0/player=true/volume=50000, no loadData rebinding, initialized-index reuse and fixed equipment-slot lookup")
