from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2]
for relative,name,output in [
 ('src/votv-coop/src/ue_wrap/engine/engine.cpp','TryGetActorTransform','npc_transform.inc'),
 ('src/votv-coop/src/ue_wrap/actors/kerfur.cpp','TryHasSaveKey','npc_save_key.inc')]:
    source=(root/relative).read_text(encoding='utf-8-sig')
    match=re.search(r'(?ms)^bool '+name+r'\(.*?^}',source)
    assert match,name
    Path(sys.argv[1],output).write_text(match.group()+'\n',encoding='utf-8')
source=(root/'src/votv-coop/src/coop/props/prop_snapshot.cpp').read_text(encoding='utf-8-sig')
match=re.search(r'(?ms)^void CompleteDrainForCurrentSlot\(.*?^}',source)
assert match
Path(sys.argv[1],'npc_snapshot_complete.inc').write_text(match.group()+'\n',encoding='utf-8')
# The production route must capture NPCs before a prop bracket can finish.
replay=(root/'src/votv-coop/src/coop/session/connect_replay.cpp').read_text(encoding='utf-8-sig')
assert replay.index('npc_world_enum::RegisterExistingWorldNpcs(slot)')<replay.index('npc_sync::QueueConnectBroadcastForSlot(slot)')<replay.index('prop_snapshot::TriggerForSlot(slot)')
drain=source[source.index('void DrainChunk()'):source.index('// One additive PropSpawn')]
assert 'const int drainingSlot=' in drain and 'SnapshotCurrent(drainingSlot,drainingToken)' in drain
assert drain.index('if(!current())return;',drain.index('BuildPropSpawnPayload_'))<drain.index('ReliableKind::PropSpawn')
