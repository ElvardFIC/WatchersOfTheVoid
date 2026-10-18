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
boar = package('objects__misc__grayboar')
boar_graph = dict(body(boar, next(e for e in boar.exports if e['name'] == 'ExecuteUbergraph_grayboar')))
boar_tick = dict(body(boar, next(e for e in boar.exports if e['name'] == 'ReceiveTick')))
assert 'ExecuteUbergraph_grayboar(17479)' in boar_tick[18]
assert 'Inst(StaticMesh)->AddForce(Inst(movementForce)' in boar_graph[18287]
assert 'grayboarPawn.grayboarPawn_C' in boar_graph[16486]
assert 'Inst(pawn)->Actor.K2_AttachToActor(self' in boar_graph[16652]
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
