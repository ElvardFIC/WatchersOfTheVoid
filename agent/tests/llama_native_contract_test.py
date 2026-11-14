"""Validate the native llama graph boundaries used by the host state/cue adapter."""
from pathlib import Path
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package, Reader
from dump_2135 import body
from props_dump import read_tag

assets = Path(sys.argv[1]) if len(sys.argv) > 1 else research / 'current-assets'
def package(name):
    return Package(str(assets / ('VotV__Content__' + name + '.uasset')))
def graph(p, name):
    export = next(e for e in p.exports if e['name'] == name and p.class_of_export(e) == 'Function')
    rows = dict(body(p, export))
    assert not any('NO VALID SCRIPT' in text for text in rows.values())
    return rows
def contains(rows, *parts):
    return any(all(part in text for part in parts) for text in rows.values())

p = package('objects__prop_llama')
cls = next(e for e in p.exports if e['name'] == 'prop_llama_C')
assert p.obj(cls['sup']) == '/Game/objects/prop.prop_C'
for name, entry in (('ReceiveBeginPlay',1146), ('jump',1264), ('checkJump',1269), ('ReceiveDestroyed',1214)):
    assert contains(graph(p,name), f'ExecuteUbergraph_prop_llama({entry})')
rows = graph(p,'ExecuteUbergraph_prop_llama')
assert rows[1146] == 'prop_C.ReceiveBeginPlay()'
assert 'Inst(yumpskare)' in rows[1156] and 'jump @1175' in rows[1156]
assert rows[1175] == 'Inst(Sphere)->SetCollisionEnabled(byte:0)'
assert contains(rows,'K2_SetTimerDelegate','0.5, true')
assert rows[108] == 'bind_delegate(checkJump, Local(K2Node_CreateDelegate_OutputDelegate), self)'
assert 'K2_SetTimerDelegate' in rows[131] and ', 0.5, true, 0, 0)' in rows[131]
assert rows[184] == 'Inst(timer) = Local(CallFunc_K2_SetTimerDelegate_ReturnValue)'
assert rows[211] == 'pop_flow' and rows[1213] == 'pop_flow'
assert contains(rows,'GameplayStatics.GetPlayerCameraManager(self, 0)')
assert contains(rows,'Inst(StaticMesh)->SetPhysicsLinearVelocity(')
assert 'Cross_VectorVector' in rows[940] and '(0,0,1)' in rows[940]
assert 'Cross_VectorVector' in rows[990]
assert 'MakeRotationFromAxes' in rows[1036]
assert 'K2_SetActorRotation' in rows[1091] and ', false)' in rows[1091]
assert rows[1121] == 'KismetSystemLibrary.K2_ClearAndInvalidateTimerHandle(self, Inst(timer))'
assert 'Add_VectorVector' in rows[538] and '(0,0,0)' in rows[538]
assert 'Multiply_VectorFloat' in rows[634] and ', 3)' in rows[634]
assert 'Add_VectorVector' in rows[676] and '(0,0,100)' in rows[676]
assert 'false, name:None)' in rows[726] and rows[785] == 'jump @212'
assert 'SphereOverlapActors' in rows[1376] and 'Inst(SphereRadius)' in rows[1376]
assert 'mainPlayer.mainPlayer_C' in rows[1376]
assert 'LineTraceSingleForObjects' in rows[1655] and 'CallFunc_obj_statDyn_obj' in rows[1655]
assert contains(graph(package('main__lib_obj'),'obj_pawn'), '= [byte:2]')
assert rows[212] == 'Inst(Audio)->Activate(false)'
assert rows[287] == 'Inst(h) = true' and rows[49] == 'Inst(h) = false'
assert contains(rows,'Inst(PointLight)->SceneComponent.SetVisibility(true, false)')
assert contains(rows,'Inst(PointLight)->SceneComponent.SetVisibility(false, false)')
assert contains(rows,'KismetSystemLibrary.Delay(self, 3,')
assert contains(rows,'setEvent(Inst(h), false, self)')
assert contains(rows,'setEvent(false, false, self)')
assert any(p.class_of_export(e) == 'AudioComponent' and e['name'] == 'Audio_GEN_VARIABLE' for e in p.exports)
assert any(p.class_of_export(e) == 'PointLightComponent' and e['name'] == 'PointLight_GEN_VARIABLE' for e in p.exports)
def template_bool(name, field):
    export = next(e for e in p.exports if e['name'] == name)
    data = p.export_data(export)
    reader = Reader(data)
    while (tag := read_tag(p, reader)) is not None:
        prop, typ, size, index, extra = tag
        if prop == field:
            assert typ == 'BoolProperty'
            return bool(extra['bool'])
        reader.p += size
    raise AssertionError((name, field, 'missing explicit default'))
assert template_bool('Audio_GEN_VARIABLE', 'bAutoActivate') is False
assert template_bool('PointLight_GEN_VARIABLE', 'bVisible') is False

console = graph(package('umg__ui_console'),'enterCommand')
assert contains(console,'BeginDeferredActorSpawnFromClass(self,','prop_llama.prop_llama_C')
assert contains(console,'SetBoolPropertyByName','name:yumpskare, true')
assert contains(console,'BeginDeferredActorSpawnFromClass(self,','llamaSoul.llamaSoul_C')
soul = package('objects__llamaSoul')
assert not any(soul.class_of_export(e) == 'Function' for e in soul.exports)
marker = next(e for e in soul.exports if e['name'] == 'llamaSoul_C')
assert soul.obj(marker['sup']) == '/Game/objects/actor_save.actor_save_C'
saved = package('objects__actor_save')
data, load, key = (graph(saved, name) for name in ('getData', 'loadData', 'getKey'))
assert contains(data, 'Actor.GetTransform()') and contains(data, 'GameplayStatics.GetObjectClass(self)')
assert contains(data, '.key_', '= Inst(key)')
assert contains(load, 'Inst(key) = Local(data).key_')
assert not contains(load, 'Transform')
assert contains(key, 'assignKey(Inst(key)')
assert contains(graph(saved, 'ignoreSave'), 'Out(ignoreSave) = Inst(skipSave)')
events = graph(package('main__lib'), 'setEvent')
assert contains(events, 'Add_IntInt(', 'Inst(activeEvents), 1)')
assert contains(events, 'Array_Add(', 'Inst(activeEvents_senders)', 'Local(__WorldContext)')
assert contains(events, 'Subtract_IntInt(', 'Inst(activeEvents), 1)')
assert contains(events, 'Array_RemoveItem(', 'Inst(activeEvents_senders)', 'Local(__WorldContext)')
assert contains(events, 'Inst(activeEvents) = 0')
save = graph(package('main__mainGamemode'), 'saveObjects')
assert contains(save, 'GetAllActorsWithInterface', 'int_save.int_save_C')
assert contains(save, '->ignoreSave(') and contains(save, '->getData(')
print('PASS native llama base/isolated rearm, jump/latents, native event accounting and console/actor_save/int_save marker contracts')
