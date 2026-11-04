from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2]
source=(root/'src/votv-coop/src/ue_wrap/engine/engine.cpp').read_text(encoding='utf-8-sig')
match=re.search(r'(?ms)^bool TryGetActorTransform\(.*?^}',source)
assert match
Path(sys.argv[1],'npc_pose_transform.inc').write_text(match.group()+'\n',encoding='utf-8')
