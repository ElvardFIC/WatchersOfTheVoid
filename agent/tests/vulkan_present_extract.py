from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2]
source=(root/'src/votv-coop/src/ui/overlay_backend_vk_frame.cpp').read_text(encoding='utf-8-sig')
functions=[]
for name in ['SwappedPresent','DestroyGuarded','SwappedDestroy']:
    match=re.search(r'(?ms)^(?:VkResult|void) '+name+r'\(.*?^}',source)
    assert match,name
    functions.append(match.group())
Path(sys.argv[1]).write_text('\n\n'.join(functions)+'\n',encoding='utf-8')
