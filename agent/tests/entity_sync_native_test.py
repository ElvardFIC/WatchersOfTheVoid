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
assert boar_graph[16486].endswith('byte:1, self))'), 'helper ownership must remain the body for scoped path abort'
assert 'GetAIController(Inst(pawn))' in boar_graph[173]
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
anim_update = dict(body(anim, next(e for e in anim.exports if e['name'] == 'BlueprintUpdateAnimation')))
assert 'ExecuteUbergraph_grayboar_Skeleton_AnimBlueprint(3948)' in anim_update[18]
assert 'Inst(grayBoar)->Inst(StaticMesh)->SceneComponent.GetForwardVector()' in anim_graph[3991]
assert 'Inst(grayBoar)->Inst(forward)->SceneComponent.GetRightVector()' in anim_graph[4063]
assert 'Dot_VectorVector' in anim_graph[4135]
assert 'Inst(turn) = Local(CallFunc_FInterpTo_ReturnValue)' in anim_graph[4474]
assert 'Multiply_FloatFloat(Inst(turn), -30)' in anim_graph[1138]
assert 'Multiply_FloatFloat(Inst(turn), 30)' in anim_graph[1268]
assert 'Lerp(0.5, 2, Local(CallFunc_Abs_ReturnValue_2))' in anim_graph[5160]
assert 'Inst(audio_loop_turn)->AudioComponent.SetPitchMultiplier' in anim_graph[5207]
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
assert any(i[1] == 'SoundWave' and i[3] == 'grayboar_idle4' for i in boar.imports)
assert any(i[1] == 'SoundAttenuation' and i[3] == 'att_default' for i in boar.imports)
assert 'Local(K2Node_CustomEvent_skipAnim)' in boar_graph[24749]
for offset,section in [(24764,'damage1'),(24795,'damage2'),(24826,'damage3'),(24857,'damage4')]:
    assert f'name:{section}' in boar_graph[offset]
assert 'RandomIntegerInRange(0, 3)' in boar_graph[24888]
assert 'CreateProxyObjectForPlayMontage' in boar_graph[24953] and 'Inst(animationSpeed)' in boar_graph[24953]
assert 'audio_idle)->AudioComponent.SetSound' in boar_graph[23431] and 'grayboar_damage_Cue' in boar_graph[23431]
assert 'Inst(audio_idle)->Play(0)' in boar_graph[23342] and 'Inst(audio_attack)->Stop()' in boar_graph[23394]
assert any(i[1]=='SoundCue' and i[3]=='grayboar_damage_Cue' for i in boar.imports)
for name, entry in [('OnNotifyBegin_F2D44BFA4340ED84016CA19643B74736', 2921),
                    ('OnCompleted_F2D44BFA4340ED84016CA19643B74736', 13600)]:
    rows = dict(body(boar, next(e for e in boar.exports if e['name'] == name)))
    assert f'ExecuteUbergraph_grayboar({entry})' in rows[18]
assert 'name:death' in boar_graph[2948]
assert 'GameplayStatics.BeginDeferredActorSpawnFromClass' in boar_graph[4306]
assert 'K2_DestroyActor()' in boar_graph[13627]
move = dict(body(boar, next(e for e in boar.exports if e['name'] == 'move')))
assert 'ExecuteUbergraph_grayboar(21139)' in move[0]
assert 'name:ExecuteUbergraph_grayboar' in boar_graph[21139] and 'skipconst:15' in boar_graph[21139]
assert 'Inst(speedMultiplier)' in boar_graph[91]
assert 'AITask_MoveTo.AIMoveTo' in boar_graph[202]
assert 'OnCharacterMovementUpdated' in boar_graph[489]
for name, entry in [('updMov',21330),('checkIfOnPath',26533),('nomoveMove',26197),
                    ('OnRequestFailed_EAF218CB4B42F6CCCF73E9B452B5AF9D',13852),
                    ('OnMoveFinished_EAF218CB4B42F6CCCF73E9B452B5AF9D',13786)]:
    rows=dict(body(boar,next(e for e in boar.exports if e['name']==name)))
    assert any(f'ExecuteUbergraph_grayboar({entry})' in row for row in rows.values())

# Delays scheduled before adoption bypass the public function gates on resume.
for schedule, resume in [(13797,543),(13924,558),(24526,966),(26478,1016),(26670,1381)]:
    assert 'Delay(' in boar_graph[schedule] and f'skipconst:{resume},' in boar_graph[schedule]
    assert 'name:ExecuteUbergraph_grayboar' in boar_graph[schedule]
assert 'move()' in boar_graph[543] and 'jump @543' in boar_graph[558]
assert 'detected()' in boar_graph[966]
assert 'StopMovementImmediately()' in boar_graph[1308] and 'move()' in boar_graph[1366]
assert 'StopMovementImmediately()' in boar_graph[1518] and 'move()' in boar_graph[1576]
for name, entry in [('senseUpdate',21799),('saw',21828),('setWalking',26118),
    ('BndEvt__grayboar_StaticMesh_K2Node_ComponentBoundEvent_1_ComponentHitSignature__DelegateSignature',25637),
    ('reachedByLightning',26734)]:
    rows=dict(body(boar,next(e for e in boar.exports if e['name']==name)))
    assert any(f'ExecuteUbergraph_grayboar({entry})' in row for row in rows.values())
assert 'unhook()' in boar_graph[21813]
assert 'SetMovementMode(byte:1, byte:0)' in boar_graph[26055]
assert 'jump @16399' in boar_graph[25637] and 'push_flow @14985' in boar_graph[16399]
assert 'AddImpulse' in boar_graph[15555]
assert '->addDamage(self, 10,' in boar_graph[16237]
assert 'grayboar_overcharged.grayboar_overcharged_C' in boar_graph[27181]
assert 'K2_DestroyActor()' in boar_graph[27289]

# Old OnDestroyed bindings only unbind and clear their target; preserve cleanup.
dest=dict(body(boar,next(e for e in boar.exports if e['name']=='dest')))
assert 'ExecuteUbergraph_grayboar(23553)' in dest[18]
assert 'jump @23477' in boar_graph[23553]
assert 'op62(Inst(target)->Inst(OnDestroyed)' in boar_graph[23500]
assert 'op5F(Inst(target), noobj)' in boar_graph[23541]

charged=package('objects__misc__grayboar_overcharged')
charged_graph=dict(body(charged,next(e for e in charged.exports if e['name']=='ExecuteUbergraph_grayboar_overcharged')))
for name, entry in [('killed',933),('deathplosion',928),('objectDamaged',938),('zapSfx',923)]:
    rows=dict(body(charged,next(e for e in charged.exports if e['name']==name)))
    assert any(f'ExecuteUbergraph_grayboar_overcharged({entry})' in row for row in rows.values())
assert 'bind_delegate(deathplosion' in charged_graph[10] and ', 0.25, true,' in charged_graph[33]
assert 'explosion.explosion_C' in charged_graph[199] and 'name:damage, 1)' in charged_graph[315]
assert '->punch(' in charged_graph[1049]
assert 'SetPhysicsAngularVelocityInDegrees' in charged_graph[863]
for p in (boar,charged):
    explosion=dict(body(p,next(e for e in p.exports if e['name']=='explosion')))
    assert 'explosion.explosion_C' in explosion[113]
    assert any('name:damage, 25)' in row for row in explosion.values())

# Script-gate class scopes match the declaring body, not the actor's ancestry.
# Every registered owner/name pair must actually be exported by that package.
sync_source=(checkout/'src/votv-coop/src/coop/creatures/grayboar_sync.cpp').read_text()
authority_table=sync_source.split('kAuthorityEntries[] = {',1)[1].split('};',1)[0]
for owner,name in re.findall(r'\{(kBoarClass|kChargedClass),\s*L"([^"]+)"\}',authority_table):
    p=boar if owner=='kBoarClass' else charged
    assert any(e['name']==name and p.class_of_export(e)=='Function' for e in p.exports), (owner,name)
assert not any(e['name'] == 'GetVelocity' and boar.class_of_export(e) == 'Function' for e in boar.exports)

if '--boar-only' in sys.argv:
    print('PASS native boar targeting, bite/health and AnimBP velocity -> locomotion/audio path')
    raise SystemExit(0)

source = (checkout / 'src/votv-coop/src/coop/creatures/npc_world_enum.cpp').read_text()
assert 'kExSpawnSourceClasses' not in source, 'dead/log-only source table can diverge from the executed gate'
table = source.split('kExSpawnSources[] = {', 1)[1].split('};', 1)[0]
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

kavotia_controller = package('objects__kavotiaPatrolController')
kavotia_controller_graph = next(e for e in kavotia_controller.exports
                                if e['name'] == 'ExecuteUbergraph_kavotiaPatrolController')
kavotia_controller_rows = [row for _, row in body(kavotia_controller, kavotia_controller_graph)]
assert any('BeginDeferredActorSpawnFromClass' in row and '/objects/kavotia.kavotia_C' in row
           for row in kavotia_controller_rows)
assert any('BeginDeferredActorSpawnFromClass' in row and '/objects/igetis.igetis_C' in row
           for row in kavotia_controller_rows)
assert 'kavotiaPatrolController_C' in sources
authority = (checkout / 'src/votv-coop/src/coop/world/spawn_authority.cpp').read_text()
assert 'kavotiaPatrolController.ReceiveBeginPlay' in authority

kavotia = package('objects__kavotia')
kavotia_functions = {e['name']: e for e in kavotia.exports if kavotia.class_of_export(e) == 'Function'}
kavotia_graph = dict(body(kavotia, kavotia_functions['ExecuteUbergraph_kavotia']))
assert 'ExecuteUbergraph_kavotia(3961)' in dict(body(kavotia, kavotia_functions['ReceiveBeginPlay']))[0]
assert 'Inst(Mesh)->SceneComponent.K2_AttachToComponent(Inst(SpringArm)' in kavotia_graph[4001]
assert 'Inst(anim), Local(K2Node_DynamicCast_AsKavotia_Skeleton_Anim_Blueprint)' in kavotia_graph[4322]
for fn in ('alert', 'updateTarget', 'sensePawns'):
    assert fn in kavotia_functions, f'Kavotia authority watch names a missing native function: {fn}'
kavotia_target = dict(body(kavotia, kavotia_functions['updateTarget']))
assert 'Inst(anim)->Inst(isShooting)' in kavotia_target[48]
assert 'SelectFloat(450, 200' in kavotia_target[89]
assert 'Inst(CharacterMovement)->Inst(MaxWalkSpeed)' in kavotia_target[158]
assert 'Inst(anim)->Inst(shootTarget)' in kavotia_graph[5039]
assert 'Inst(anim)->Inst(isLaying)' in kavotia_graph[4688]
assert 'Inst(eff_tentacleBallShootWindup)->Activate(true)' in kavotia_graph[5753]
assert 'Inst(charging)->Activate(true)' in kavotia_graph[5790]
assert 'Inst(charging)->Deactivate()' in kavotia_graph[5860]
assert 'Lerp(0.5, 2' in kavotia_graph[5954]
assert 'Inst(charging)->AudioComponent.SetPitchMultiplier' in kavotia_graph[6001]
assert 'BeginDeferredActorSpawnFromClass' in kavotia_graph[894] and 'kavotiaShot_C' in kavotia_graph[894]
assert 'MakeLiteralInt(7)' in kavotia_graph[418] and 'Delay(self, 0.3' in kavotia_graph[1907]
assert 'name:fire' in kavotia_graph[1366] and 'kavotia_Skeleton_Montage' in kavotia_graph[1366]
assert 'kavotia_Skeleton_laying_Montage' in kavotia_graph[6300]
kavotia_anim = package('meshes__kavotia__kavotia_Skeleton_AnimBlueprint')
kavotia_anim_graph = dict(body(kavotia_anim, next(e for e in kavotia_anim.exports
                            if e['name'] == 'ExecuteUbergraph_kavotia_Skeleton_AnimBlueprint')))
assert 'Inst(kavotia), Local(K2Node_DynamicCast_AsKavotia)' in kavotia_anim_graph[89]
assert 'Inst(mesh), Inst(kavotia)->Inst(Mesh)' in kavotia_anim_graph[108]
assert 'Inst(kavotia)->GetVelocity()' in kavotia_anim_graph[537]
assert not any(e['name'].startswith('AnimNotify_') for e in kavotia_anim.exports)
print('PASS native Kavotia setup/authority entries, aim/stance, charge/pitch, seven-shot burst and exact montage assets')

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
gun_stealer = package('objects__npc__npc_arirGunStealer')
gun_functions = {e['name'] for e in gun_stealer.exports if gun_stealer.class_of_export(e) == 'Function'}
assert 'punching' in gun_functions
assert 'StealGun' not in gun_functions and 'TakeWeapon' not in gun_functions
ariral_sync = (checkout / 'src/votv-coop/src/coop/creatures/ariral_sync.cpp').read_text()
assert 'L"punching"' in ariral_sync and 'L"StealGun"' not in ariral_sync and 'L"TakeWeapon"' not in ariral_sync
print(f'PASS native spawn catches, {len(classes)} Character-compatible NPC roots, boar separation and zombie/cheese descendant coverage')
