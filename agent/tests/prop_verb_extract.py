from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
source=(root/'src/votv-coop/src/coop/props/prop_syncer_host.cpp').read_text(encoding='utf-8-sig')
start=source.index('    // (4) the row goes,',source.index('void EndClientRow('))
end=source.index('\nvoid OnDisconnect()',start)
tail=source[start:end].rstrip()
assert tail.endswith('}')
Path(sys.argv[1],'verb_handoff.inc').write_text(tail[:-1]+'\n',encoding='utf-8')
