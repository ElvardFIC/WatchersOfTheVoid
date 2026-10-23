"""Keep the precise arirShipAppear hooks tied to the fresh cooked 0.9.0n graph."""
from pathlib import Path
import sys
research=Path(__file__).resolve().parents[3]/"_research"
sys.path.insert(0,str(research))
from kismet_dis import Package
from dump_2135 import body
p=Package(str(research/"current-assets/VotV__Content__objects__arirShipAppear.uasset"))
functions={e["name"]:body(p,e) for e in p.exports if p.class_of_export(e)=="Function"}
graph=dict(functions["ExecuteUbergraph_arirShipAppear"])
for name,entry in {"appear":908,"runTrigger":1398,"ReceiveBeginPlay":1197,"ReceiveTick":1546,"photoTaken":2686,"a__UpdateFunc":604,"a__FinishedFunc":3075}.items():
    assert any(f"ExecuteUbergraph_arirShipAppear({entry})" in line for _,line in functions[name]),(name,functions[name])
assert graph[15]=="appear(false)"
assert graph[383]=="jump @31"
assert "text:'blank'" in graph[192] and "->addEmail(" in graph[192]
assert "skipconst:383" in graph[324] and "skipconst:31" in graph[2631]
assert graph[3075]=="jump @2585"
assert "SetScalarParameterValueOnMaterials(name:alpha, Inst(a_a_2C6617ED4975EC563C73D5B0DBE0638B))" in graph[604]
assert "MultiplyMultiply_FloatFloat" in graph[658] and ", 10)" in graph[658]
assert "Lerp(0, 100000," in graph[700] and "SetIntensity" in graph[747]
assert "->setEvent(Local(K2Node_CustomEvent_dir), false, self)" in graph[908]
assert graph[955]=="Inst(visible) = Local(K2Node_CustomEvent_dir)"
assert graph[974]=="Inst(radarPoint)->Inst(active) = Inst(visible)"
assert "TimelineComponent.Play()" in graph[1034] and "TimelineComponent.Reverse()" in graph[1067]
assert "eff_arirShipAir" in graph[1100] and "Inst(air), name:None, (0,0,0), (0,0,0), (1,1,1), byte:0, true, byte:0, true" in graph[1100]
assert graph[1495]=="appear(false)" and graph[1510]=="jump @474"
assert graph[1515]=="appear(true)" and graph[1530]=="jump @539"
assert "op62(Inst(gamemode)->Inst(photoTaken)" in graph[497]
assert "op5C(Inst(gamemode)->Inst(photoTaken)" in graph[562]
assert "addAriralRep(15, self)" in graph[815]
text="\n".join(graph.values())
assert "GetPlayerCameraManager" in text and "sun_height" in text and "WasRecentlyRendered" in text
assert "SetActorLocation" not in text and "Teleport" not in text
print("PASS cooked arirShipAppear: exact hook/latent entries, blank-email loop, alpha/light/radar/timeline, photo binding/reward, camera/day eligibility, native air cue and no player relocation")
