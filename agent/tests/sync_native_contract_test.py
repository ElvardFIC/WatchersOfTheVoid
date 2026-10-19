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

evidence = {}

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

plant = package('objects__growingPlant')
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
bucket_body=function(bucket,'ExecuteUbergraph_prop_bucket')
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
print(json.dumps({'status': 'PASS', 'contracts': ['plant state/death/depletion', 'growth side effects in all measured subclasses',
    'table-selected plant classes', 'hose resume/finish/rebuild and stale endpoint keys', 'native black-hole audio phases/parameters',
    'alien/harvest/faucet sound source/parameters', 'native bucket/fertilizer resource costs',
    'PC/laptop tab camera side effects and cart UI verbs'], 'sha256': evidence}, indent=2))
