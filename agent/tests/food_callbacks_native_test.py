"""Fresh cooked food: final-bite ordering and the exact eaten output factories."""
from pathlib import Path
import re
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body


def functions(name):
    package = Package(str(research / 'current-assets' / f'VotV__Content__objects__{name}.uasset'))
    return {e['name']: body(package, e) for e in package.exports if package.class_of_export(e) == 'Function'}


def reachable(rows, start):
    graph = dict(rows)
    following = {at: rows[i + 1][0] for i, (at, _) in enumerate(rows[:-1])}
    visited, pending = {}, [start]
    while pending:
        at = pending.pop()
        if at in visited:
            continue
        line = graph[at]
        visited[at] = line
        jump = re.search(r'jump @(\d+)', line)
        if jump:
            pending.append(int(jump[1]))
        if line.startswith(('jump @', 'return ', 'pop_flow', 'end_of_script')):
            continue
        pending.append(following[at])
    return visited


entries = {'prop_food': 6309, 'prop_foodBox': 10, 'prop_food_banana': 151,
           'prop_food_coffeeCup': 1814, 'prop_food_funnyshroom': 77,
           'prop_food_poison': 197, 'prop_food_showberry': 116,
           'prop_food_snuskLoaf': 300, 'prop_food_spng': 96,
           'prop_food_superdeathShroom': 96}
factories = {'prop_foodBox': ('prop.prop_C', 'name:g_foodbox'),
             'prop_food_banana': ('prop_bananaHusk.prop_bananaHusk_C', ''),
             'prop_food_coffeeCup': ('prop.prop_C', 'name:cup')}
paths = {}
for name, entry in entries.items():
    fs = functions(name)
    assert any(f'ExecuteUbergraph_{name}({entry})' in line for _, line in fs['eaten']), name
    paths[name] = text = '\n'.join(reachable(fs['ExecuteUbergraph_' + name], entry).values())
    if name in factories:
        actor, item = factories[name]
        assert text.count('BeginDeferredActorSpawnFromClass(') == 1, name
        assert text.count('GameplayStatics.FinishSpawningActor(') == 1, name
        assert actor in text and item in text, name
        assert not any(s in text for s in ('addEffect(', 'progressAchievement(', 'addHunger', 'playerInteracted')), name
    else:
        assert not any(s in text for s in ('SpawnActor', 'FinishSpawningActor', 'replaceProp')), name

assert 'addEffect(name:lsd' in paths['prop_food_funnyshroom']
assert 'addEffect(name:poo' in paths['prop_food_showberry']
assert 'progressAchievement(name:snusklord' in paths['prop_food_snuskLoaf']
assert 'addEffect(name:foodPoison, 40, 10' in paths['prop_food_spng']
assert 'addEffect(name:foodPoison, 1000, 10' in paths['prop_food_superdeathShroom']
assert 'prop_food_C.eaten(noobj)' in paths['prop_food_poison'] and 'addHint(' in paths['prop_food_poison']
assert 'SetPhysicsLinearVelocity' in paths['prop_food_coffeeCup']
assert 'SetPhysicsAngularVelocityInDegrees' in paths['prop_food_coffeeCup']
base = dict(functions('prop_food')['ExecuteUbergraph_prop_food'])
assert 'Subtract_IntInt(Inst(uses), 1)' in base[29]
assert 'Inst(uses) = ' in base[98]
assert base[173] == 'bitten(Inst(playerInteracted))'
assert base[196] == 'eaten(Inst(playerInteracted))'
assert 'food_eaten' in base[411] and base[469] == 'K2_DestroyActor()'
print('PASS cooked food: final bite commits before eaten/output/destruction; exactly three wrapper factories; six personal overrides and empty base excluded from host replay')
