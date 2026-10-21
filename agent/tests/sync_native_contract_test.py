"""Validate the gameplay contracts against extracted 0.9.0n assets, not fixtures.

Usage: python agent/tests/sync_native_contract_test.py _research/current-assets _research
The second directory supplies the existing research-only Kismet decoder.
"""
from pathlib import Path
import hashlib
import json
import sys

asset_root, decoder_root = (Path(p).resolve() for p in sys.argv[1:3])
sys.path.insert(0, str(decoder_root))
from kismet_dis import Package
from dump_2135 import body
from kismet_dis import Reader
from props_dump import read_tag

evidence = {}

# Native window repaint has no arguments and reads the current scalar itself.

def package(name):
    path = asset_root / f"VotV__Content__{name}.uasset"
    p = Package(str(path))
    for file in (path, path.with_suffix('.uexp')):
        evidence[file.name] = hashlib.sha256(file.read_bytes()).hexdigest()
    return p

def function(p, name):
    e, = [e for e in p.exports if p.class_of_export(e) == 'Function' and e['name'] == name]
    return dict(body(p, e))

def text(p, name):
    return '\n'.join(function(p, name).values())

wall_outline=package('objects__customWall_unfinished')
builder=package('objects__prop_wallbuilder')
library=package('main__lib')
assert 'customContent_' in text(library,'isCustom')
assert 'isCustom_func(self,' in text(builder,'ExecuteUbergraph_prop_wallbuilder')
builder_wall=function(builder,'addWall')
assert builder_wall[585]=='if not (Inst(cheat)) jump @821'
assert '->buildWall(true)' in builder_wall[599]
finish=function(wall_outline,'buildWall')
assert finish[10]=='pop_flow_if_not(Local(isCheat))'
assert 'GetOverlappingActors' in finish[31] and 'findMat(' in finish[211]
assert 'FinishSpawningActor' in finish[483] and '->setHealth()' in finish[521]
assert '->particleEffect()' in finish[557] and finish[593]=='K2_DestroyActor()'
assert all('K2_DestroyActor' not in row for offset,row in finish.items() if offset<593)

bbq=package('objects__prop_bbq')
bbq_graph=function(bbq,'ExecuteUbergraph_prop_bbq')
assert 'Inst(hasLid) = Local(lid)' in text(bbq,'setLid') and 'updLid()' in text(bbq,'setLid')
lid_paint=text(bbq,'updLid')
assert 'SetVisibility(Inst(hasLid), false)' in lid_paint and 'SetCollisionEnabled' in lid_paint
assert all(word not in lid_paint for word in ('DestroyActor','Hold Object','Spawn','coals','temperature'))
assert 'Inst(cookingLoop)->SetActive(true, false)'==bbq_graph[4402]
assert 'Inst(cookingLoop)->Deactivate()'==bbq_graph[3452]
assert 'KismetMathLibrary.Lerp(0, 0.1,' in bbq_graph[3531]
assert 'Inst(cookingLoop)->AudioComponent.SetVolumeMultiplier(Inst(v))' in text(bbq,'setVol')
assert 'KismetMathLibrary.SelectFloat(2, 1, Inst(hasLid))' in bbq_graph[1744]
assert bbq_graph[4551]=='setLid(false)' and bbq_graph[4566]=='jump @2595'
assert 'Prop To Object(name:bbq_1, self,' in bbq_graph[2600] and '->Hold Object(false,' in bbq_graph[3280]
assert 'Inst(coals)' in text(bbq,'getData') and 'Inst(hasLid)' in text(bbq,'getData')
bbq_save=text(bbq,'getData');bbq_load=text(bbq,'loadData')
assert 'Inst(a)' not in bbq_save and 'Inst(active)' not in bbq_save
assert 'floats_' not in bbq_save and 'floats_' not in bbq_load # slot 1 is free after the base float group
prop_base=package('objects__prop')
assert function(prop_base,'getData')[88]=='Local(K2Node_MakeArray_Array_1) = [Local(K2Node_MakeStruct_struct_mFloat)]'
assert 'Inst(coals) =' in bbq_load and 'Inst(hasLid) =' in bbq_load
coal_add=function(bbq,'addCoal')
assert 'Less_IntInt(Inst(coals), 16)' in coal_add[0]
assert 'Add_IntInt(Inst(coals), 1)' in coal_add[48] and coal_add[117]=='Inst(coals) = Local(Temp_int_Variable)'
assert coal_add[144]=='Out(added) = true' and coal_add[257]=='Out(added) = false'
assert 'Local(data).ints_' in function(bbq,'loadData')[38] and ', 0,' in function(bbq,'loadData')[38]
assert 'Local(data).bools_' in function(bbq,'loadData')[193] and ', 1,' in function(bbq,'loadData')[193]
coal_piece=package('objects__prop_coal_s')
coal_use=function(coal_piece,'ExecuteUbergraph_prop_coal_s')
assert '->addCoal(' in coal_use[570] and coal_use[615]=='if not (Local(CallFunc_addCoal_added)) jump @634'
assert coal_use[70]=='K2_DestroyActor()' # personal consumption follows successful native addCoal

plant = package('objects__growingPlant')
coin = package('objects__misc__baocoin')
coin_body = function(coin, 'ExecuteUbergraph_baocoin')
assert 'addPoints(Inst(points), self)' in coin_body[441]
assert coin_body[829] == 'K2_DestroyActor()' # credit precedes native retirement
assert '[Inst(grow), Inst(fertilizer), Inst(water), Inst(fruitGrow),' in text(plant, 'getData')
assert 'Inst(dryness) = -1' in text(plant, 'turnDead')
assert 'SetLifeSpan(' in text(plant, 'loadData') and 'processGrown(Inst(grew))' in text(plant, 'loadData')
tick = text(plant, 'ExecuteUbergraph_growingPlant')
assert 'VictoryFloatMinusEquals(Inst(water), Inst(deltaSeconds),' in tick
assert 'VictoryFloatMinusEquals(Inst(fertilizer), Inst(deltaSeconds),' in tick
assert 'addFruit(' in tick and 'RandomBoolWithWeight' in tick
assert 'BeginDeferredActorSpawnFromClass' in text(plant, 'addFruit')
harvest = text(plant, 'harvestSound')
assert 'plantGather_Cue' in harvest and '(0,0,25)' in harvest
assert '(0,0,0), 1, 1, 0,' in harvest
assert 'Local(K2Node_Event_player)->putObjectInventory2(' in tick
assert 'Local(K2Node_Event_player)->Inst(input_alt)' in tick # discard-only player read
for row in function(plant, 'ExecuteUbergraph_growingPlant').values():
    if 'Local(K2Node_Event_player)' in row:
        assert 'putObjectInventory2(' in row or 'Inst(input_alt)' in row
stolas = package('objects__growingPlant_stolasHerb')
assert 'ExecuteUbergraph_growingPlant_stolasHerb(29)' in text(stolas, 'grown')
assert 'lightningStrike' in text(stolas, 'ExecuteUbergraph_growingPlant_stolasHerb')
assert 'K2_DestroyActor' in text(stolas, 'ExecuteUbergraph_growingPlant_stolasHerb')
# Every other allowlisted descendant was audited too: none overrides grown
# with an unguarded gameplay output during canonical loadData replay.
for suffix in ('bush','deadtree','grassPatch','lemontree','orangetree','pine',
               'shroom_0','shroom_2','shroom_3','shroom_4','shroom_5','shroom_6',
               'shroom_7','shroom_funny'):
    descendant=package(f'objects__growingPlant_{suffix}')
    assert not any(e['name']=='grown' and descendant.class_of_export(e)=='Function' for e in descendant.exports)
pot = package('objects__prop_farmPot')
assert 'plantObject_' in text(pot, 'ExecuteUbergraph_prop_farmPot') # table-selected subclass
hose = package('objects__groundHose')
assert 'gen()' in text(hose, 'finish') and 'finish()' in text(hose, 'processKeys')
assert 'ClearSplinePoints' in text(hose, 'gen')
assert 'DestroyComponent' in text(hose, 'gen') # replace old geometry on every replay
assert 'Inst(moving) = true' in text(hose, 'resume') and 'SetActorTickEnabled(true)' in text(hose, 'resume')
assert 'Inst(moving) = false' in text(hose, 'finish') and 'SetActorTickEnabled(false)' in text(hose, 'finish')
assert 'Inst(points) =' in text(hose, 'loadData') and 'gen()' not in text(hose, 'loadData')
assert 'Inst(sprinkler_key)' in text(hose, 'getData') and 'Inst(sprinkler_key) = None' not in text(hose, 'getData')
assert 'op5F(Inst(sprinkler), noobj)' in text(hose, 'detachSprinkler')
assert 'sprinkler_key' not in text(hose, 'detachSprinkler') # native detach leaves the saved key behind
assert 'op5F(Inst(sprinkler), noobj)' not in text(hose, 'processKeys')
faucet = package('objects__faucet')
assert 'Inst(hose_key)' in text(faucet, 'getData') and 'Inst(hose_key)' in text(faucet, 'loadData')
faucet_body=function(faucet,'ExecuteUbergraph_faucet')
assert 'Inst(active) = Local(CallFunc_Not_PreBool_ReturnValue)' in faucet_body[651]
assert 'door0_open.door0_open, 0.25, 1, 0,' in faucet_body[684]
assert sum('PlaySound2D' in row for row in faucet_body.values()) == 1
sprinkler = package('objects__prop_sprinkler')
assert 'Inst(hose_key)' in text(sprinkler, 'getData') and 'Inst(hose_key)' in text(sprinkler, 'loadData')
assert 'Inst(faucet)->Inst(active)' in text(sprinkler, 'upd')
bucket = package('objects__prop_bucket')
grime=package('objects__grime')
clean_body=function(grime,'clean');material_body=function(grime,'applyMaterial')
assert 'Divide_FloatFloat(Inst(process), 100)' in clean_body[139]
assert 'SetScalarParameterValue(Inst(cleanParameter)' in clean_body[181]
assert clean_body[419]=='K2_DestroyActor()'
assert 'Divide_FloatFloat(Inst(process), Inst(maxProcess))' in material_body[477]
assert 'SetScalarParameterValue(Inst(cleanParameter)' in material_body[523]
grime_classes={}
for path in sorted(asset_root.glob('VotV__Content__objects__grime*.uasset')):
    name=path.stem.removeprefix('VotV__Content__');p=package(name)
    grime_classes[name]={e['name'] for e in p.exports if p.class_of_export(e)=='Function'}
assert len(grime_classes)==24
assert {name for name,names in grime_classes.items() if 'clean' in names}=={'objects__grime'}
assert {name for name,names in grime_classes.items() if 'applyMaterial' in names}=={
    'objects__grime','objects__grime_arirGraffiti','objects__grime_blood2','objects__grime_bloodtest',
    'objects__grime_dyn','objects__grime_oil','objects__grime_uv'}
bucket_body=function(bucket,'ExecuteUbergraph_prop_bucket')
assert 'prop_sponge_bucketPour.prop_sponge_bucketPour_C' in bucket_body[902]
assert 'FinishSpawningActor' in bucket_body[1057] and 'SetPhysicsLinearVelocity' in bucket_body[1493]
bucket_throw=function(bucket,'getFromWater')
assert 'prop_sponge_bucketPour.prop_sponge_bucketPour_C' in bucket_throw[1133]
assert 'name:soapAmount' in bucket_throw[1212]
assert 'FinishSpawningActor' in bucket_throw[1388] and 'SetPhysicsLinearVelocity' in bucket_throw[1840]
assert 'wade_Cue.wade_Cue, 1, 1.5,' in bucket_throw[1922]
assert 'water_splash_Cue.water_splash_Cue, 0.5, 1,' in bucket_throw[1982]
assert bucket_throw[2042]=='Inst(height) = 0' and bucket_throw[2065]=='Inst(soap) = 0'
pour=package('objects__prop_sponge_bucketPour')
pour_body=function(pour,'ExecuteUbergraph_prop_sponge_bucketPour')
assert function(pour,'collided')[90]=='ExecuteUbergraph_prop_sponge_bucketPour(2901)'
assert function(pour,'ReceiveBeginPlay')[0]=='ExecuteUbergraph_prop_sponge_bucketPour(1654)'
assert 'footsteps/wade_Cue.wade_Cue' in pour_body[1843]
assert 'eff_waterThrowSplash.eff_waterThrowSplash' in pour_body[2120]
assert 'Local(CallFunc_BreakHitResult_Location)' in pour_body[2120]
assert 'Local(CallFunc_BreakHitResult_Normal)' in pour_body[2083]
assert '->extinguishFire()' in pour_body[169] and '->shortCircuit()' in pour_body[586]
assert '->cleanSponge(' in pour_body[1308] and pour_body[2575]=='K2_DestroyActor()'
assert 'skipconst:15' in pour_body[835] and pour_body[15]=='K2_DestroyActor()'
assert 'wade_Cue.wade_Cue' in bucket_body[435] and ', 0.25, 1, 0,' in bucket_body[435]
washer=package('objects__prop_waterShooter')
washer_body=function(washer,'ExecuteUbergraph_prop_waterShooter')
assert 'prop_sponge_p.prop_sponge_p_C' in washer_body[1817]
assert 'FinishSpawningActor' in washer_body[2099] and 'SetPhysicsLinearVelocity' in washer_body[2367]
assert washer_body[2448]=='Local(CallFunc_FinishSpawningActor_ReturnValue)->Inst(power) = 1'
assert 'SetLifeSpan(10)' in washer_body[1386]
assert 'wade_Cue.wade_Cue, 1, 1.5,' in washer_body[1326]
water_shot=package('objects__prop_sponge_p')
water_shot_body=function(water_shot,'ExecuteUbergraph_prop_sponge_p')
assert function(water_shot,'ReceiveBeginPlay')[0]=='ExecuteUbergraph_prop_sponge_p(982)'
assert 'eff_waterThrower.eff_waterThrower' in water_shot_body[1216]
assert 'eff_waterThrowSplash.eff_waterThrowSplash' in water_shot_body[1696]
assert water_shot_body[15]=='K2_DestroyActor()' and 'skipconst:15' in water_shot_body[422]
assert '->shortCircuit()' in water_shot_body[173] and '->cleanSponge(' in water_shot_body[868]
assert 'Normal)' in water_shot_body[1659] and 'Location)' in water_shot_body[1696]
assert 'K2_AddActorLocalOffset' in water_shot_body[1112] and 'K2_AddActorLocalOffset' in pour_body[1784]
assert 'ExecuteUbergraph_prop_sponge_p(1282)' in text(water_shot,'BndEvt__StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature')
parent_sponge=package('objects__prop_sponge')
parent_sponge_body=function(parent_sponge,'ExecuteUbergraph_prop_sponge')
assert 'Inst(washCollision)' in parent_sponge_body[4087]
assert '->clean(' in parent_sponge_body[3716] and '->cleanPhys(' in parent_sponge_body[4532]
assert 'collided(' in parent_sponge_body[4579] and parent_sponge_body[3875]=='prop_C.ReceiveBeginPlay()'
assert 'ExecuteUbergraph_prop_sponge(4087)' in text(parent_sponge,'BndEvt__prop_sponge_StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature')
assert 'ExecuteUbergraph_prop_bucket(4791)' in text(bucket,'playerHandUse_LMB')
assert 'Greater_FloatFloat(Inst(height), 5)' in bucket_body[5064]
assert 'Divide_FloatFloat(Inst(height), Inst(maxHeight))' in bucket_body[5112]
assert '600)' in bucket_body[5158] and 'Inst(water) =' in bucket_body[5200]
assert bucket_body[5309]=='Inst(height) = 0'
fertilizer = package('objects__prop_fertilizer')
fert_body=function(fertilizer,'ExecuteUbergraph_prop_fertilizer')
assert 'Greater_FloatFloat(' in fert_body[625] and 'Inst(fertilizer), 0)' in fert_body[625]
assert 'Inst(fertilizer) = 600' in fert_body[700] and fert_body[745]=='K2_DestroyActor()'
sky = package('objects__newsky')
assert 'ExecuteUbergraph_newsky(5062)' in text(sky, 'tp')
sky_body = function(sky, 'ExecuteUbergraph_newsky')
for delay, continuation in ((4645,3222),(15,70),(382,437),(497,552),(650,759),(968,1023)):
    assert 'Delay(' in sky_body[delay] and f'skipconst:{continuation},' in sky_body[delay]
for continuation in (70,759,1023):
    assert 'addEmail(' in sky_body[continuation]
assert 'setEvent(true, false, self)' in sky_body[4546]
assert 'setEvent(false, false, self)' in sky_body[611]
assert sky_body[5062] == 'jump @4546'
assert 'PlaySound2D(self, obj:/Game/audio/effects/earthTP_in.earthTP_in, 1.5, 0, 0,' in sky_body[4585]
assert 'PlaySound2D(self, obj:/Game/audio/effects/earthTP_out.earthTP_out, 1.5, 0, 0,' in sky_body[437]
assert 'skipconst:437' in sky_body[382] # actual latent continuation, not an estimated clock
for name in ('earthTP_in', 'earthTP_out'):
    wave = package(f'audio__effects__{name}')
    assert any(wave.class_of_export(e) == 'SoundWave' for e in wave.exports)
noiser = package('objects__noiser')
assert 'ExecuteUbergraph_noiser(1599)' in text(noiser, 'timer')
noise_body = function(noiser, 'ExecuteUbergraph_noiser')
assert 'SpawnSoundAttached(obj:/Game/audio/effects/outsideAlien_Cue.outsideAlien_Cue, Inst(CapsuleComponent)' in noise_body[1604]
assert 'false, 2, 1, 0,' in noise_body[1604]
assert sum('SpawnSoundAttached' in row for row in noise_body.values()) == 1
laptop = package('umg__interfaces__ui_laptop')
switch = text(laptop, 'switcherSwitched')
assert 'Inst(switcher)->SetActiveWidgetIndex(Local(Index))' in switch
assert 'createRC()' in switch and 'leaveRC()' in switch # never replay on mirror
cart_add = text(laptop, 'addStoreCart')
cart_remove = text(laptop, 'removeStoreCart')
cart_rebuild = text(laptop, 'genStoreCart')
assert 'list_store.list_store' in text(laptop, 'generateStore')
assert '.name_14_B3814BBE478D1FA0AB005BB6386C1541 = Local(CallFunc_Array_Get_Item_2)' in text(laptop, 'generateStore')
assert 'Array_Add(Inst(cart), Out(struct_store))' in cart_add
assert 'VictoryIntPlusEquals(Inst(size)' in cart_add and 'VictoryIntPlusEquals(Inst(storePrice)' in cart_add
assert 'Array_Remove(Inst(cart), Local(index))' in cart_remove
assert 'VictoryIntMinusEquals(Inst(size)' in cart_remove and 'VictoryIntMinusEquals(Inst(storePrice)' in cart_remove
assert 'Inst(storePrice) = 0' in cart_rebuild and 'Inst(size) = 0' not in cart_rebuild
assert 'SetIntPropertyByName(Local(CallFunc_Create_ReturnValue_1), name:index' in cart_rebuild
for name in ('addStoreCart', 'removeStoreCart', 'genStoreCart'):
    assert 'makeAnOrder(' not in text(laptop, name)
wall = package('objects__customWall')
wet = package('objects__customWall_wetConcrete')
assert '[Inst(health), Inst(maxHealth)]' in text(wall,'getData')
for name in ('getData','loadData'):
    assert not any(e['name']==name and wet.class_of_export(e)=='Function' for e in wet.exports)
wet_body = function(wet,'ExecuteUbergraph_customWall_wetConcrete')
assert 'VictoryFloatPlusEquals(Inst(dryTime)' in wet_body[10]
assert 'Inst(timeToDry)' in wet_body[47] and 'BeginDeferredActorSpawnFromClass' in wet_body[127]
assert wet_body[320]=='K2_DestroyActor()' # replacement stays native on the host
cement = package('objects__prop_concreteBucket')
assert 'Inst(dryTimer)' not in text(cement,'getData') and 'Inst(dryTimer)' not in text(cement,'loadData')
cement_body = function(cement,'ExecuteUbergraph_prop_concreteBucket')
assert 'VictoryFloatPlusEquals(Inst(dryTimer)' in cement_body[3395]
assert ', 600)' in cement_body[3432]
sponge = package('objects__prop_sponge')
assert '[Inst(power)]' in text(sponge,'getData')
assert 'soapAmount' not in text(sponge,'getData') and 'soapAmount' not in text(sponge,'loadData')
assert 'VictoryFloatMinusEquals(Inst(soapAmount), 5,' in text(sponge,'cleanBubbles')
sponge_bubbles=function(sponge,'cleanBubbles')
assert 'Greater_FloatFloat(Inst(soapAmount), 0)' in sponge_bubbles[0]
assert 'Inst(player)->Inst(hitResult)' in sponge_bubbles[81]
assert 'Conv_VectorToRotator(Local(CallFunc_BreakHitResult_Normal))' in sponge_bubbles[275]
assert 'eff_cleanBubbles.eff_cleanBubbles, Local(CallFunc_BreakHitResult_Location)' in sponge_bubbles[312]
assert '(1,1,1), true, byte:0, true)' in sponge_bubbles[312]
assert sponge_bubbles[377]=='Inst(eff_bubbles)->Activate(false)'
sponge_p = package('objects__prop_sponge_p')
for name in ('getData','loadData'):
    assert not any(e['name']==name and sponge_p.class_of_export(e)=='Function' for e in sponge_p.exports)
assert not any(e['name']=='cleanBubbles' and sponge_p.class_of_export(e)=='Function' for e in sponge_p.exports)
cooking = package('objects__prop_cookingFood')
cooking_tick = function(cooking,'ExecuteUbergraph_prop_cookingFood')
assert 'prop_food_C.ReceiveTick' in cooking_tick[2205]
assert 'VictoryFloatPlusEquals(Inst(cookProgress)' in cooking_tick[2445]
assert cooking_tick[2531] == 'cookItem()'
assert function(cooking,'cookItem')[426] == 'K2_DestroyActor()'
assert 'Inst(temperature) = Inst(temperature)' in text(cooking,'cookItem')
overrides = []
cooking_descendants = sorted(asset_root.glob('VotV__Content__objects__prop_cookingFood_*.uasset'))
assert len(cooking_descendants) == 20
for path in cooking_descendants:
    descendant = package(path.stem.removeprefix('VotV__Content__'))
    names = {e['name'] for e in descendant.exports if descendant.class_of_export(e)=='Function'}
    assert not names.intersection(('getData','loadData','ReceiveTick')), path.name
    if 'cookItem' in names:
        overrides.append(path.stem.removeprefix('VotV__Content__objects__'))
assert overrides == ['prop_cookingFood_cookietray','prop_cookingFood_shrimp','prop_cookingFood_shrimp1']
# Class names do not identify a recipe family: eggs, potatoes, fish steaks and
# several pizzas use other prefixes. Derive the family from every prop's parent.
prop_classes = {}
for path in sorted(asset_root.glob('VotV__Content__objects__prop*.uasset')):
    p = Package(str(path))
    cls = next((e for e in p.exports if e['name'].endswith('_C') and not e['name'].startswith('Default__')), None)
    if cls:
        prop_classes[cls['name']] = (path.stem.removeprefix('VotV__Content__'), p.obj(cls['sup']).split('.')[-1],
            {e['name'] for e in p.exports if p.class_of_export(e)=='Function'})
assert len(prop_classes) == 1278
def cooking_class(name):
    for _ in range(64):
        if name == 'prop_cookingFood_C':
            return True
        if name not in prop_classes:
            return False
        name = prop_classes[name][1]
    raise AssertionError('cyclic cooking ancestry')
recipes = {name: row for name, row in prop_classes.items() if cooking_class(name)}
assert len(recipes) == 78
recipe_factories = {name for name, row in recipes.items() if 'cookItem' in row[2]}
assert recipe_factories == {
    'prop_cookingFood_C','prop_cookingFood_cookietray_C','prop_cookingFood_shrimp_C','prop_cookingFood_shrimp1_C',
    'prop_cf_egg_C','prop_cfood_breadmold_C','prop_cfood_pizzad_mush_C','prop_cfood_pizzad_pepp_C',
    'prop_cfood_pizzad_pine_C','prop_cfood_pizzad_shri_C','prop_fishSteak_C',
    'prop_food_potato_C','prop_food_potato_h_C','prop_food_potato_q_C'}
for name, (asset, parent, names) in recipes.items():
    recipe = package(asset)
    if names.intersection(('getData','loadData')):
        assert name == 'prop_fish_C'
        assert 'prop_food_C.getData' in text(recipe,'getData')
        assert 'prop_food_C.loadData' in text(recipe,'loadData')
        assert 'floats_' not in text(recipe,'getData') and 'floats_' not in text(recipe,'loadData')
banana=package('objects__prop_food_banana')
french=package('objects__prop_cookingFood_french')
french_body=function(french,'ExecuteUbergraph_prop_cookingFood_french')
assert function(french,'eaten')[18]=='ExecuteUbergraph_prop_cookingFood_french(75)'
assert 'cig.cig_C' in french_body[75] and 'getMainPlayer' in french_body[148]
assert 'Inst(sittingOn)' in french_body[194]
assert '250,' in french_body[344] and 'obj:prop_cookingFood_french_C' in french_body[344]
assert 'prop_moniquePlush.prop_moniquePlush_C' in french_body[597]
assert 'FinishSpawningActor' in french_body[807] and 'Local(CallFunc_Array_Get_Item)->K2_DestroyActor()'==french_body[900]
banana_output=function(banana,'ExecuteUbergraph_prop_food_banana')
assert 'prop_bananaHusk.prop_bananaHusk_C' in banana_output[38]
assert function(banana,'eaten')[18]=='ExecuteUbergraph_prop_food_banana(151)'
assert banana_output[151]=='jump @10'
assert not any('addFood' in line or 'getMainPlayer' in line for line in banana_output.values())
cup=package('objects__prop_food_coffeeCup')
cup_body=function(cup,'ExecuteUbergraph_prop_food_coffeeCup')
assert function(cup,'eaten')[18]=='ExecuteUbergraph_prop_food_coffeeCup(1814)'
assert function(cup,'ReceiveTick')[18]=='ExecuteUbergraph_prop_food_coffeeCup(39)'
assert 'LessEqual_FloatFloat' in cup_body[131] and ', 0.5)' in cup_body[131]
assert 'Greater_FloatFloat' in cup_body[234] and ', 900)' in cup_body[234]
assert 'water_splash_Cue.water_splash_Cue' in cup_body[344] and ', 0.5, 1.25, 0,' in cup_body[344] and 'att_small.att_small' in cup_body[344]
assert 'Local(CallFunc_K2_GetActorLocation_ReturnValue)' in cup_body[344]
assert 'Lerp(0.5, 1,' in cup_body[640] and 'foodData).uses_' in cup_body[548]
assert 'grimeProjectile.grimeProjectile_C' in cup_body[783] and 'name:grunge, obj:/Game/objects/grime_coffee.grime_coffee_C' in cup_body[825]
assert 'name:velocity, Local(CallFunc_GetVelocity_ReturnValue_1)' in cup_body[898] and 'FinishSpawningActor' in cup_body[1261]
assert not any('name:size' in text for offset,text in cup_body.items() if 39<=offset<1300)
assert cup_body[1300]=='eaten(noobj)' and cup_body[1315]=='K2_DestroyActor()'
assert cup_body[1814]=='jump @1330' and cup_body[1400]=='Local(Temp_name_Variable) = name:cup'
assert 'FinishSpawningActor' in cup_body[1500] and cup_body[1813]=='pop_flow'
assert not any('addFood' in line or 'getMainPlayer' in line for off,line in cup_body.items() if 1330<=off<=1814)
assert 'getSaveSlot' in cup_body[1819] # personal used() path is separate from eaten() output
window=package('objects__misc__baseWindow')
assert function(window,'setClean')[0]=='Inst(StaticMesh1)->PrimitiveComponent.SetCustomPrimitiveDataFloat(0, Inst(clean))'
assert function(window,'cleanSponge')[72]=='ExecuteUbergraph_baseWindow(29)'
window_body=function(window,'ExecuteUbergraph_baseWindow')
assert 'VictoryFloatMinusEquals' in window_body[788] and 'FMax' in window_body[825]
assert any('setClean()' in line for line in window_body.values())
beer=package('objects__prop_snack_beer')
beer_body=function(beer,'ExecuteUbergraph_prop_snack_beer')
assert function(beer,'ReceiveTick')[18]=='ExecuteUbergraph_prop_snack_beer(2661)'
assert 'Inst(uses) = Local(Temp_int_Variable_4)'==beer_body[2857]
assert 'fuel1.fuel1' in beer_body[2930] and 'Local(CallFunc_GetComponentBounds_Origin)' in beer_body[2930]
assert ', 0.5, 1, 0,' in beer_body[2930] and 'att_default.att_default' in beer_body[2930]
fuel=package('audio__effects__fuel1');default_attenuation=package('audio__misc__att_default')
assert [(e['name'],fuel.class_of_export(e)) for e in fuel.exports]==[('fuel1','SoundWave')]
assert [(e['name'],default_attenuation.class_of_export(e)) for e in default_attenuation.exports]==[('att_default','SoundAttenuation')]
assert 'grimeProjectile.grimeProjectile_C' in beer_body[3111]
assert 'name:grunge, Inst(Grunge)' in beer_body[3153]
assert 'name:velocity,' in beer_body[3691] and 'FinishSpawningActor' in beer_body[3882]
assert '(0.25,0.25,0.25)' in beer_body[3819]
# Broken/broken_fire use a separate, larger spill and are not admitted as tilt requests.
assert function(beer,'broken')[0]=='ExecuteUbergraph_prop_snack_beer(3969)'
assert function(beer,'broken_fire')[0]=='ExecuteUbergraph_prop_snack_beer(4862)'
assert 'Lerp(0.5, 1,' in beer_body[4680] and 'FinishSpawningActor' in beer_body[4823]
assert prop_classes['prop_snack_wine_C'][1]=='prop_snack_beer_C'
assert 'ReceiveTick' not in prop_classes['prop_snack_wine_C'][2]
flight=package('objects__misc__grimeProjectile')
flight_body=function(flight,'ExecuteUbergraph_grimeProjectile')
assert function(flight,'ReceiveTick')[18]=='ExecuteUbergraph_grimeProjectile(130)'
assert function(flight,'ReceiveBeginPlay')[0]=='ExecuteUbergraph_grimeProjectile(15)'
assert {flight.class_of_export(e) for e in flight.exports if e['name'].endswith('_GEN_VARIABLE')}=={'SceneComponent','SphereComponent'}
flight_default,=[e for e in flight.exports if e['name']=='Default__grimeProjectile_C']
flight_data=flight.export_data(flight_default);flight_reader=Reader(flight_data);flight_life=flight_size=None
while (flight_tag:=read_tag(flight,flight_reader)) is not None:
    name,typ,size,index,extra=flight_tag
    if name=='InitialLifeSpan':
        assert typ=='FloatProperty' and size==4
        flight_life=Reader(flight_data,flight_reader.p).f32()
    if name=='size':
        assert typ=='FloatProperty' and size==4
        flight_size=Reader(flight_data,flight_reader.p).f32()
    flight_reader.p+=size
assert flight_life==60 and flight_size==5
assert 'Inst(Sphere)->SetPhysicsLinearVelocity(Inst(velocity)' in flight_body[70]
assert 'SphereTraceSingleForObjects(self, Inst(lastLoc)' in flight_body[204]
assert 'IsSimulatingPhysics' in flight_body[542] and 'MirrorVectorByNormal' in flight_body[1077]
assert 'GetActorScale3D' in flight_body[1239]
assert 'BeginDeferredActorSpawnFromClass(self, Inst(grunge)' in flight_body[1619]
assert 'FinishSpawningActor' in flight_body[2041]
assert 'water_splash_Cue.water_splash_Cue' in flight_body[2251]
assert ', 0.5, 1.25, 0,' in flight_body[2251] and 'att_small.att_small' in flight_body[2251]
assert '->shortCircuit()' in flight_body[2571] and flight_body[2608]=='K2_DestroyActor()'
gas_bottle=package('objects__prop_gasBottle')
gas_break=function(gas_bottle,'ExecuteUbergraph_prop_gasBottle')
assert function(gas_bottle,'broken')[0]=='ExecuteUbergraph_prop_gasBottle(10)'
assert function(gas_bottle,'broken_fire')[0]=='ExecuteUbergraph_prop_gasBottle(551)'
assert 'grimeProjectile.grimeProjectile_C' in gas_break[129]
assert 'name:grunge, obj:/Game/objects/grime_gasoline.grime_gasoline_C' in gas_break[171]
assert 'MirrorVectorByNormal' in gas_break[308] and 'name:velocity' in gas_break[354]
assert '(1,1,1)' in gas_break[445] and 'FinishSpawningActor' in gas_break[508]
gasoline=package('objects__grime_gasoline')
gas_fire=function(gasoline,'ExecuteUbergraph_grime_gasoline')
assert function(gasoline,'ignite')[18]=='ExecuteUbergraph_grime_gasoline(683)'
assert function(gasoline,'fireDamage')[18]=='ExecuteUbergraph_grime_gasoline(731)'
assert 'name:component, Inst(Box)' in gas_fire[74] and 'name:fuelSpeed, 0.25' in gas_fire[115]
assert 'name:igniteChance, 1)' in gas_fire[193] and 'name:igniteScale, 1.25' in gas_fire[230]
assert '->ignite_s(Local(K2Node_Event_fuel))' in gas_fire[358]
assert 'Inst(fuel)' in gas_fire[731] and 'Less_FloatFloat' in gas_fire[768] and gas_fire[10]=='K2_DestroyActor()'
flammable=package('objects__components__comp_flammable')
flame_body=function(flammable,'ExecuteUbergraph_comp_flammable')
assert 'GetOwner()' in flame_body[15] and 'Inst(owner)' in flame_body[35]
assert 'FMin(' in flame_body[1118] and ', 60)' in flame_body[1118]
assert 'fireDamage(' in flame_body[4367] and '->ignite(' in flame_body[7134]
assert 'Inst(fuelSpeed)' in flame_body[8107] and flame_body[8234]=='jump @7893'
assert 'K2_DestroyComponent(self)' in flame_body[4001] and 'Deactivate()' in flame_body[4049]
for name in ('grime_beer','grime_wine','grime_coffee','grime_gasoline'):
    liquid=package('objects__'+name)
    cls,=[e for e in liquid.exports if e['name']==name+'_C']
    assert liquid.obj(cls['sup'])=='/Game/objects/grime.grime_C'
    assert not any(liquid.class_of_export(e)=='Function' and e['name'] in ('applyMaterial','clean','getData','loadData') for e in liquid.exports)
explosion=package('objects__misc__explosion')
assert function(explosion,'ExecuteUbergraph_explosion')[15]=='scorchMark()'
scorch=function(explosion,'scorchMark')
assert scorch[5].endswith('[(1,0,0), (0,1,0), (-1,0,0), (0,-1,0), (0,0,1), (0,0,-1)]')
assert 'Multiply_VectorFloat' in scorch[181] and ', 65)' in scorch[181]
assert '->obj_static(self,' in scorch[223] and 'LineTraceSingleForObjects' in scorch[343]
assert scorch[486]=='pop_flow_if_not(Local(CallFunc_LineTraceSingleForObjects_ReturnValue))'
assert 'GetActorScale3D' in scorch[496] and 'GetActorScale3D' in scorch[830]
assert 'BreakHitResult_Location' in scorch[733] and 'GetActorScale3D_ReturnValue' in scorch[1067]
assert 'BeginDeferredActorSpawnFromClass(self, obj:/Game/objects/grime_explosionScorch.grime_explosionScorch_C' in scorch[788]
assert 'FinishSpawningActor' in scorch[1122] and scorch[1273]=='jump @121'
for name,parent in (('objects__grime_explosionScorch','/Game/objects/grime.grime_C'),
                    ('objects__misc__explosion_saltrocket','/Game/objects/misc/explosion.explosion_C')):
    child=package(name);cls,=[e for e in child.exports if e['name']==name.split('__')[-1]+'_C']
    assert child.obj(cls['sup'])==parent
    assert not any(child.class_of_export(e)=='Function' for e in child.exports)
pc=package('objects__misc__explosion_pc')
assert function(pc,'ExecuteUbergraph_explosion_pc')[10]=='explosion_C.ReceiveBeginPlay()'
underwater=package('objects__misc__explosion_underwater')
cls,=[e for e in underwater.exports if e['name']=='explosion_underwater_C']
assert underwater.obj(cls['sup'])=='/Script/Engine.Actor'

dingus=package('objects__prop_dingus')
plush=package('objects__prop_abplush')
plush_body=function(plush,'ExecuteUbergraph_prop_abplush')
assert 'Inst(input_alt)' in plush_body[52] and plush_body[10]=='Inst(qwak)->Activate(true)'
dingus_body=function(dingus,'ExecuteUbergraph_prop_dingus')
assert dingus_body[5599]=='touch()' and 'ToggleActive()' in dingus_body[5704]
assert function(dingus,'touch')[0]=='ExecuteUbergraph_prop_dingus(6835)'
assert 'Inst(isUsed)' in dingus_body[5782] and 'Deactivate()' in dingus_body[5970]
for toy in ('prop_dingus1','prop_dingus2','prop_dingus3','prop_dingus4ugly','prop_dingus5erie'):package('objects__'+toy)
for wave_name,duration,looping in (
    ('audio__music__newMaxwellDeadassRemixNoCopyrightFuckOff',69.17156219482422,True),
    ('audio__music__dickholeGaming',96.24671936035156,True),
    ('audio__music__eeaaa_ae_ae_ae_a_eaaa',31.639999389648438,True),
    ('audio__music__erieloop',15.998000144958496,True),
    ('audio__00000000audio',2.244262933731079,False)):
    wave=package(wave_name);wave_export,=[e for e in wave.exports if wave.class_of_export(e)=='SoundWave']
    data=wave.export_data(wave_export);reader=Reader(data);properties={}
    while (tag:=read_tag(wave,reader)) is not None:
        name,typ,size,index,extra=tag
        if name=='Duration':properties[name]=Reader(data,reader.p).f32()
        if name=='bLooping':properties[name]=bool(extra['bool'])
        reader.p+=size
    assert abs(properties['Duration']-duration)<.0001
    if looping:assert properties.get('bLooping') is True
    else:assert not properties.get('bLooping',False) # no looping override in this short sound
watoy = package('objects__prop_watoy')
toy_body = function(watoy,'ExecuteUbergraph_prop_watoy')
assert function(watoy,'activate')[0] == 'ExecuteUbergraph_prop_watoy(151)'
assert 'IsValid(Inst(a))' in toy_body[151]
assert 'buttonlong10.buttonlong10, 1, 0.25, 0,' in toy_body[199]
assert 'Inst(Audio)->SetActive(true, true)' == toy_body[259]
assert 'NODE_AddzerogComponent-0' in toy_body[297]
assert 'Actor.SetActorTickEnabled(true)' == toy_body[10]
assert 'TimelineComponent.PlayFromStart()' in toy_body[21]
assert 'KismetMathLibrary.Lerp(0.1, 1,' in toy_body[58]
assert 'SetPhysicsAngularVelocityInDegrees' in toy_body[583] and 'AddForce' in toy_body[876]
assert 'SetPhysicsLinearVelocity' in toy_body[1173]
assert 'KismetMathLibrary.Lerp(1, 2,' in toy_body[1407] and 'SetPitchMultiplier' in toy_body[1454]
assert 'Inst(Audio)->Deactivate()' == toy_body[1500]
zero = package('objects__components__comp_zerog')
zero_body = function(zero,'ExecuteUbergraph_comp_zerog')
assert function(zero,'ReceiveBeginPlay')[0] == 'ExecuteUbergraph_comp_zerog(380)'
assert 'SetPhysicsLinearVelocity((0,0,10), true' in zero_body[63]
assert 'GetComponentBounds' in zero_body[176] and 'InverseTransformLocation' in zero_body[222]
assert 'eff_zgrav.eff_zgrav' in zero_body[268] and 'SpawnEmitterAttached' in zero_body[268]
assert 'Delay(self, 10,' in zero_body[565] and 'skipconst:15' in zero_body[565]
assert zero_body[15] == 'Inst(eff)->Deactivate()' and zero_body[51] == 'Inst(dir) = false'
assert 'SetEnableGravity(true)' in zero_body[620]
assert function(zero,'ReceiveTick')[18] == 'ExecuteUbergraph_comp_zerog(658)'
assert 'AddForce' in zero_body[1188] and 'K2_DestroyComponent' in zero_body[1291]
print(json.dumps({'status': 'PASS', 'contracts': ['native wallbuilder cheat call, host custom-content rule and one successor/retirement body',
    '24 grime classes, six material overrides and native clean process/100 versus setup process/maxProcess',
    'grill native lid repaint/heating, loop fade, exact lid-output graph and verified free persistence float group',
    'plant state/death/depletion', 'growth side effects in all measured subclasses',
    'coin native credit before retirement',
    'bucket-pour birth velocity order and separate collision gameplay/sound/splash/latent retirement audit',
    'pressure washer native source projectile, wade cue/pitch and distinct initial/surface particles',
    'wet-wall native save schema and host replacement; concrete drying and sponge soap omissions',
    'black-hole latent continuations, delayed emails and event bookkeeping',
    'table-selected plant classes', 'hose resume/finish/rebuild and stale endpoint keys', 'native black-hole audio phases/parameters',
    'alien/harvest/faucet sound source/parameters', 'native bucket/fertilizer resource costs',
    'PC/laptop tab camera side effects and cart UI verbs',
    'cooking ancestry across 1278 props, 78 family members and all 14 cookItem declarations',
    'banana/cup output-only eaten branches; sponge soap debit, native trace geometry and particle parameters',
    'native window zero-argument repaint and clean minimum',
    'beer/wine native tilt factory, separate break spill, flight deposit/scale and exact impact sound',
    'gasoline bottle break-only projectile, decal fuel destruction, exact Box flame initialization, native spread and expiry/removal',
    'explosion six static traces, native hit transforms/scales and inherited scorch factory; private PC parent call and separate underwater class',
    'dingus native touch/toggle order and five cooked music wave durations/loop overrides for optional playback positioning',
    'watoy activation, force/audio/timeline and independent zero-gravity component lifecycle'], 'sha256': evidence}, indent=2))
