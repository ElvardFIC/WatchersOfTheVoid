from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2]
source=(root/'src/votv-coop/include/coop/net/session.h').read_text(encoding='utf-8-sig')
methods=[]
for name in ['MarkSlotWorldReady','IsSlotWorldReady','SlotWorldReadyRevision']:
    match=re.search(r'(?ms)^    (?:void|bool|uint64_t) '+name+r'\(.*?^    }',source)
    assert match,name
    methods.append(match.group())
Path(sys.argv[1]).write_text('\n\n'.join(methods)+'\n',encoding='utf-8')
