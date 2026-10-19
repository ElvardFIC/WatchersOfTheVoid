"""Native evidence and explicit integration assertions, not an in-game test."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / '_research'))
from kismet_dis import Package, Dis, find_script

def body(asset, function):
    p = Package(str(root / '_research/assets' / ('VotV__Content__' + asset + '.uasset')))
    e = next(e for e in p.exports if e['name'] == function and p.class_of_export(e) == 'Function')
    code, _ = find_script(p.export_data(e))
    rows = Dis(p, code).run()
    assert all('!! ' not in s for _, s in rows)
    return rows

player = 'main__mainPlayer'
rag = body(player, 'ragdollMode')
assert 'pop_flow_if_not(Inst(canRagdoll))' in dict(rag)[5]
assert any('Inst(Mesh)->SceneComponent.SetHiddenInGame(true' in s for _, s in rag)
assert any('Inst(mesh_playerVisible)->SceneComponent.SetHiddenInGame(true' in s for _, s in rag)
for fn in ('fallen', 'ignite', 'startBurning', 'extinguishFire'):
    assert body(player, fn)
graph = dict(body(player, 'ExecuteUbergraph_mainPlayer'))
assert 'Greater_FloatFloat(Inst(burningTime), 0)' in graph[6575]
assert 'GetPlayerController(self, 0)' in graph[7029]
assert 'SetControlRotation' in graph[7306]
assert 'Inst(worldDeltaSeconds)' in graph[8922]
assert 'Inst(burningEffect)->Deactivate()' in graph[9061]
assert 'Inst(burningAudio)->Deactivate()' in graph[9097]
assert 'ExecuteUbergraph_mainPlayer(109768)' in body(player, 'extinguishFire')[0][1]
assert graph[109768] == 'jump @109740'
assert graph[109740] == 'Inst(burningTime) = 0'
assert graph[109763] == 'jump @109142'
assert 'Array_RemoveItem' in graph[109142] and 'allBurningObjects' in graph[109142]
assert graph[109088] == 'Inst(isBurning) = false'
explosion = '\n'.join(s for _, s in body('objects__misc__explosion', 'ExecuteUbergraph_explosion'))
assert '->ignite(' in explosion
source = root / 'src/votv-coop/src'
revive = (source / 'coop/player/death_revive.cpp').read_text()
run = revive[revive.index('bool RunRevive('):revive.index('// How many pump ticks')]
assert run.index('ExtinguishMainPlayerFire(pawn)') < run.index('V::Write(V::Field::Health') < run.index('ClearDeadFlag(pawn)')
assert 'fireCleared && vitalsOk' in run
spawn = (source / 'ue_wrap/actors/puppet_spawn.cpp').read_text()
assert 'SetMainPlayerCanRagdoll(actor, false)' in spawn
remote = (source / 'coop/player/remote_player_ragdoll.cpp').read_text()
assert 'SpawnPlayerRagdollBody' in remote and 'SetMainPlayerRagdollMode' not in remote
print('PASS: native fire loops/extinguish/explosion, ragdoll gate, revive order and independent remote ragdoll integration')
