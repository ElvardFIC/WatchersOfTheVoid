"""Verify radio hooks against the installed game's cooked Blueprints."""
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root.parent/'_research'))
from kismet_dis import Package,Dis,find_script
def pkg(name): return Package(str(root.parent/'_research/assets'/('VotV__Content__'+name+'.uasset')))
def body(p,name):
    e=next(e for e in p.exports if e['name']==name and p.class_of_export(e)=='Function')
    code,_=find_script(p.export_data(e)); assert code is not None,name
    rows='\n'.join(s for _,s in Dis(p,code).run())
    assert '!! ' not in rows,name
    return rows
radio=pkg('objects__prop_radio_internet')
ui=pkg('umg__interfaces__ui_radioInterface')
source=(root/'src/votv-coop/src/coop/world/radio_sync.cpp').read_text()
for button in re.findall(r'L"(BndEvt__[^"]+)"',source): body(ui,button)
for name in ('openLink','fin','opened'): body(radio,name)
graph=body(radio,'ExecuteUbergraph_prop_radio_internet')
assert 'MediaPlayer.OpenFile(Inst(data).link_' in graph
assert 'Inst(pausePlayer)' in graph and 'MediaPlayer.Pause()' in graph
assert 'OnEndReached' in graph and 'OnMediaOpened' in graph
assert 'isUrl_' in graph and 'radioLinks_offlin' in graph
assert 'Inst(radio)->openLink' in body(ui,'select')
assert 'Inst(radio)->Inst(gamemode)->Inst(radioLinks)' in body(ui,'gen')
graph=body(ui,'ExecuteUbergraph_ui_radioInterface')
assert 'MediaPlayer.Play()' in graph and 'MediaPlayer.Pause()' in graph
assert 'Inst(radio)->fin()' in graph and 'Inst(radio)->Inst(playmode) = switch' in graph
print('PASS: cooked radio file open, catalog, play/pause/skip/mode UI and async callbacks')
