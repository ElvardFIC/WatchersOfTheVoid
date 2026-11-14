from pathlib import Path
import sys

root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def body(path,signature):
    text=(root/path).read_text(encoding='utf-8-sig')
    start=text.index(signature);begin=text.index('{',start);end=begin+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
out.joinpath('icons_prepare.inc').write_text(body('src/votv-coop/src/ue_wrap/hotbar/icons.cpp','bool PrepareForWorld('),encoding='utf8')
edge='src/votv-coop/src/coop/items/hotbar_icon_edge.cpp'
out.joinpath('icons_edge.inc').write_text(body(edge,'bool BuiltWithoutIcons(')+'\n'+body(edge,'void Tick('),encoding='utf8')
load=(root/'src/votv-coop/src/ue_wrap/engine/engine_save.cpp').read_text(encoding='utf-8-sig')
for signature in ('bool LoadStorySave(', 'bool StartFreshGame('):
    code=body('src/votv-coop/src/ue_wrap/engine/engine_save.cpp',signature)
    assert code.index('hotbar::PrepareForWorld(')<code.index('ExecuteConsoleCommand(openCmd.c_str())')
print('PASS both world boot routes validate item icons before native travel')
