"""Verify the cooked console/family/AI/presentation boundaries used by Rufus routing."""
from pathlib import Path
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

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

rufus = package('objects__rufus')
thicc = package('objects__thiccfus')
plush = package('objects__prop_thiccfus')
for p, cls, parent in ((rufus,'rufus_C','/Script/Engine.Character'),
                       (thicc,'thiccfus_C','/Game/objects/rufus.rufus_C'),
                       (plush,'prop_thiccfus_C','/Game/objects/prop.prop_C')):
    export = next(e for e in p.exports if e['name'] == cls)
    assert p.obj(export['sup']) == parent
assert not any(thicc.class_of_export(e) == 'Function' for e in thicc.exports)

console = graph(package('umg__ui_console'), 'enterCommand')
for name in ('rufus', 'thiccfus'):
    assert contains(console, 'BeginDeferredActorSpawnFromClass(self,', f'/Game/objects/{name}.{name}_C')

rows = graph(rufus, 'ExecuteUbergraph_rufus')
for name, entry in (('ReceiveBeginPlay',4128), ('ReceiveTick',4566), ('ReceiveDestroyed',4430), ('addDamage',5586)):
    assert contains(graph(rufus,name), f'ExecuteUbergraph_rufus({entry})')
assert contains(rows, 'GameplayStatics.GetPlayerCameraManager(self, 0)')
assert contains(rows, 'AIBlueprintHelperLibrary.CreateMoveToProxyObject')
assert contains(rows, 'WidgetBlueprintLibrary.Create', 'umg_rufusHide_C')
assert contains(rows, 'Inst(Mesh)->SceneComponent.K2_AttachToComponent(Inst(SpringArm), name:None, byte:0, byte:0, byte:0, true)')
assert contains(rows, '->Add Player Damage(2,', 'self)')
assert contains(rows, '->Actor.K2_SetActorLocation(')
for output in ('rufusCorpse.rufusCorpse_C','prop_thiccfus.prop_thiccfus_C','misc/explosion.explosion_C'):
    assert contains(rows, 'BeginDeferredActorSpawnFromClass', output)
assert contains(rows, 'Inst(hide)->RemoveFromParent()')

anim = package('meshes__rufus__rufus_Skeleton_AnimBlueprint')
anim_rows = graph(anim,'ExecuteUbergraph_rufus_Skeleton_AnimBlueprint')
assert contains(anim_rows, 'Inst(pawn)', 'Local(CallFunc_TryGetPawnOwner_ReturnValue)')
assert contains(anim_rows, 'Inst(pawn)->GetVelocity()')
assert contains(anim_rows, 'GameplayStatics.GetPlayerCameraManager(self, 0)')

root = Path(__file__).resolve().parents[2]
mirror = (root/'src/votv-coop/src/coop/creatures/npc_mirror.cpp').read_text(encoding='utf-8-sig')
fresh = mirror[mirror.index('bool SpawnFreshNpcMirror('):]
assert fresh.index('rufus_birth::ReadyForClass(actorClass)') < fresh.index('ParamFrame begin(')
assert fresh.index('if(!coop::npc_mirror_brain::PrepareBirth(spawned))') < fresh.index('ParamFrame finish(')
adopt = (root/'src/votv-coop/src/coop/creatures/npc_adoption.cpp').read_text(encoding='utf-8-sig')
bind = adopt[adopt.index('bool BindAsMirror('):]
assert bind.index('if(rufus&&!coop::npc_mirror_brain::PrepareBirth(obj))') < bind.index('CreateOrAdoptNpcMirror(')
print('PASS cooked Rufus/thiccfus console births, inherited graph, unsafe AI/HUD/damage/children, mesh attach and AnimBP owner/velocity; admission ordering')
