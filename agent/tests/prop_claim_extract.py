from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1])
source=(root/'src/votv-coop/src/coop/props/prop_syncer_host.cpp').read_text(encoding='utf-8-sig')
start=source.index('    // (4) the row goes,',source.index('void EndClientRow('));end=source.index('\nvoid OnDisconnect()',start)
out.joinpath('claim_handoff.inc').write_text(source[start:end].rstrip()[:-1]+'\n',encoding='utf-8')
source=(root/'src/votv-coop/src/coop/props/watoy_sync.cpp').read_text(encoding='utf-8-sig')
sample=re.search(r'(?ms)^void SampleWorld\(.*?^}',source);assert sample
out.joinpath('claim_watoy.inc').write_text(sample.group()+'\n',encoding='utf-8')
routes={
 'coop/items/hook_prop_claim.cpp':'Hook','coop/creatures/npc_pose_host.cpp':'Kerfur',
 'coop/creatures/kerfur_command.cpp':'Kerfur','coop/props/remote_prop.cpp':'Kerfur',
 'coop/creatures/kerfus_state.cpp':'Kerfus','coop/props/watoy_sync.cpp':'Watoy',
 'coop/world/salt_rocket_flight.cpp':'Rocket','coop/creatures/grayboar_sync.cpp':'Grayboar',
 'coop/props/prop_syncer_client.cpp':'ClientFeeder'}
for path,owner in routes.items():
 text=(root/'src/votv-coop/src'/path).read_text(encoding='utf-8-sig')
 assert 'ClaimOwned(' in text and 'ClaimSource::'+owner in text,path
 assert not re.search(r'(?:prop_drive_host::|prop_drive_host\s*::\s*)Claim\(',text),path
print('PASS canonical claim-source routes:',len(routes))
