"""Native evidence for entity spawn routing and layout, using extracted 0.9.0n assets."""
from pathlib import Path
import re
import sys

checkout = Path(__file__).resolve().parents[2]
research = checkout.parent / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body


def package(asset):
    path = research / 'assets' / ('VotV__Content__' + asset + '.uasset')
    if not path.exists():
        raise SystemExit('UNAVAILABLE: extract native entity evidence: ' + str(path))
    return Package(str(path))


def parent(asset, cls):
    p = package(asset)
    e = next(e for e in p.exports if e['name'] == cls)
    return p.obj(e['sup'])


boar = package('objects__misc__grayboar')
boar_graph = dict(body(boar, next(e for e in boar.exports if e['name'] == 'ExecuteUbergraph_grayboar')))
boar_tick = dict(body(boar, next(e for e in boar.exports if e['name'] == 'ReceiveTick')))
assert 'ExecuteUbergraph_grayboar(17479)' in boar_tick[18]
assert 'Inst(StaticMesh)->AddForce(Inst(movementForce)' in boar_graph[18287]
assert 'grayboarPawn.grayboarPawn_C' in boar_graph[16486]
assert 'Inst(pawn)->Actor.K2_AttachToActor(self' in boar_graph[16652]
boar_sense = dict(body(boar, next(e for e in boar.exports if e['name'] == 'sensePawn')))
assert 'GetOverlappingActors' in boar_sense[31] and 'obj:/Script/Engine.Pawn' in boar_sense[31]
assert 'Array_RemoveItem(Local(pawns), Inst(pawn))' in boar_sense[108]
assert 'Inst(target), Local(K2Node_DynamicCast_AsPawn)' in boar_sense[1836]
assert 'move()' in boar_sense[1855]
assert 'detected()' in boar_sense[1869]
assert 'return nothing' in boar_sense[1884], 'cancelled sense body must have no return value'
detected = dict(body(boar, next(e for e in boar.exports if e['name'] == 'detected')))
assert 'ExecuteUbergraph_grayboar(22579)' in detected[0]
assert 'Inst(roaring) = true' in boar_graph[22594]
assert 'name:roar' in boar_graph[22620]
assert 'Inst(audio_roar)->Play' in boar_graph[23048]
assert 'Inst(target)->Inst(OnDestroyed)' in boar_graph[23112]
assert not any('GetPlayerPawn' in row or 'getMainPlayer' in row for row in boar_sense.values())
assert 'IsOverlappingActor(Inst(biteActor))' in boar_graph[1758]
assert 'LineTraceSingleForObjects' in boar_graph[1963]
assert '->addDamage(self, Inst(damage)' in boar_graph[2200]
player = package('main__mainPlayer')
player_graph = dict(body(player, next(e for e in player.exports if e['name'] == 'ExecuteUbergraph_mainPlayer')))
assert 'Add Player Damage(Local(K2Node_Event_damage_7)' in player_graph[15102]
assert 'Local(K2Node_Event_actor_2))' in player_graph[15102]
damage_graph = dict(body(player, next(e for e in player.exports if e['name'] == 'Add Player Damage')))
assert 'saveSlot)->Inst(health)' in damage_graph[972]
anim = package('meshes__grayboar__grayboar_Skeleton_AnimBlueprint')
anim_graph = dict(body(anim, next(e for e in anim.exports if e['name'] == 'ExecuteUbergraph_grayboar_Skeleton_AnimBlueprint')))
assert 'Inst(grayBoar)->GetVelocity()' in anim_graph[4551]
assert 'Inst(speed)' in anim_graph[4949]
assert 'Inst(audio_loop_velocity)->AudioComponent.SetPitchMultiplier' in anim_graph[5060]
for name, entry in [
    ('BndEvt__grayboar_biteZone_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature', 21883),
    ('OnNotifyBegin_FEF3E3834EA127D483B3218D367A2A58', 14897),
]:
    rows = dict(body(boar, next(e for e in boar.exports if e['name'] == name)))
    assert any(f'ExecuteUbergraph_grayboar({entry})' in row for row in rows.values())
assert 'name:atttack' in boar_graph[21958]
assert 'Inst(audio_attack)->Play' in boar_graph[22386]
assert 'Inst(StaticMesh)->AddImpulse' in boar_graph[22519]
assert 'Inst(animationSpeed)' in boar_graph[21958]
assert 'Inst(animationSpeed)' in boar_graph[22620]
assert not any(e['name'].startswith('AnimNotify_') for e in anim.exports), 'review new AnimBP notify gameplay before cosmetic replay'
assert 'K2Node_CustomEvent_NotifyName_13' in boar_graph[14897]
assert 'jump @1701' in boar_graph[14924]
assert 'name:bite' in boar_graph[1701]
damaged = dict(body(boar, next(e for e in boar.exports if e['name'] == 'damaged')))
assert 'ExecuteUbergraph_grayboar(24581)' in damaged[36]
assert 'VictoryFloatMinusEquals(Inst(health)' in boar_graph[24634]
assert 'killed()' in boar_graph[24730] and 'jump @23558' in boar_graph[24744]
assert 'Inst(dying) = true' in boar_graph[23558]
assert 'Divide_FloatFloat(Inst(animationSpeed), 2)' in boar_graph[23593]
assert 'name:death' in boar_graph[23635]
assert 'grayboar_idle4' in boar_graph[24063] and '1.5, 1.35' in boar_graph[24063]
for name, entry in [('OnNotifyBegin_F2D44BFA4340ED84016CA19643B74736', 2921),
                    ('OnCompleted_F2D44BFA4340ED84016CA19643B74736', 13600)]:
    rows = dict(body(boar, next(e for e in boar.exports if e['name'] == name)))
    assert f'ExecuteUbergraph_grayboar({entry})' in rows[18]
assert 'name:death' in boar_graph[2948]
assert 'GameplayStatics.BeginDeferredActorSpawnFromClass' in boar_graph[4306]
assert 'K2_DestroyActor()' in boar_graph[13627]
assert not any(e['name'] == 'GetVelocity' and boar.class_of_export(e) == 'Function' for e in boar.exports)

if '--boar-only' in sys.argv:
    print('PASS native boar targeting, bite/health and AnimBP velocity -> locomotion/audio path')
    raise SystemExit(0)

source = (checkout / 'src/votv-coop/src/coop/creatures/npc_world_enum.cpp').read_text()
table = source.split('kExSpawnSourceClasses[] = {', 1)[1].split('};', 1)[0]
sources = set(re.findall(r'L"([^"]+)"', table))
for ticker, creature in [('ticker_fossilhoundSpawner', 'fossilhound'),
                         ('ticker_yellowWispSpawner', 'killerwisp')]:
    p = package('objects__tickers__' + ticker)
    graph = next(e for e in p.exports if e['name'] == 'ExecuteUbergraph_' + ticker)
    rows = body(p, graph)
    assert not any('NO VALID SCRIPT' in row for _, row in rows)
    assert any('GameplayStatics.BeginDeferredActorSpawnFromClass' in row and
               f'/objects/{creature}.{creature}_C' in row for _, row in rows)
    assert ticker + '_C' in sources, f'{ticker}: native spawn missing from catch source table'

assert parent('objects__misc__grayboar', 'grayboar_C').endswith('/prop.prop_C')
assert parent('objects__misc__grayboarPawn', 'grayboarPawn_C') == '/Script/Engine.Character'
profile = (checkout / 'src/votv-coop/include/ue_wrap/core/sdk_profile_names.h').read_text()
npc_section = profile.split('inline constexpr const wchar_t* kWorldActorAllowlist', 1)[0]
assert 'L"grayboar_C"' not in npc_section, 'prop body must not use ACharacter offsets'
constants = dict(re.findall(r'(NpcClass_\w+)\s*=\s*L"([^"]+)"', profile))
allowlist = profile.split('kNpcAllowlist[] = {', 1)[1].split('};', 1)[0]
classes = [constants[name] for name in re.findall(r'^\s*(NpcClass_\w+),', allowlist, re.M)]
parents = {}
for cls in classes:
    for path in (research / 'assets').glob('*__' + cls[:-2] + '.uasset'):
        p = Package(str(path))
        e = next((e for e in p.exports if e['name'] == cls and e['sup']), None)
        if e:
            parents[cls] = p.obj(e['sup'])
            break
    assert cls in parents, 'UNAVAILABLE: native class evidence for ' + cls
for cls in classes:
    current = cls
    seen = set()
    while current in parents and current not in seen:
        seen.add(current)
        base = parents[current]
        if base == '/Script/Engine.Character':
            break
        current = base.rsplit('.', 1)[-1]
    else:
        raise AssertionError(f'{cls}: NPC lane requires native Character ancestry')
for name in ['npc_zombie_deer', 'npc_zombie_erie', 'npc_zombie_skerfuro', 'npc_zombie_skele']:
    assert parent('objects__npc__' + name, name + '_C').endswith('/npc_zombie.npc_zombie_C')
assert parent('objects__npc__npc_zombie_skeleBloody', 'npc_zombie_skeleBloody_C').endswith('/npc_zombie_skele.npc_zombie_skele_C')
assert parent('objects__npc__npc_goreSlither_cheese', 'npc_goreSlither_cheese_C').endswith('/npc_goreSlither.npc_goreSlither_C')
print(f'PASS native spawn catches, {len(classes)} Character-compatible NPC roots, boar separation and zombie/cheese descendant coverage')
