from pathlib import Path
import sys

research=Path(__file__).resolve().parents[3]/'_research'
sys.path.insert(0,str(research))
from kismet_dis import Package,Dis,find_script
assets=research/'current-assets'
def rows(asset,function):
    p=Package(str(assets/('VotV__Content__'+asset+'.uasset')))
    e=next(e for e in p.exports if e['name']==function and p.class_of_export(e)=='Function')
    code,_=find_script(p.export_data(e)); assert code is not None
    return '\n'.join(line for _,line in Dis(p,code).run())
menu=rows('umg__interfaces__ui_menu','ExecuteUbergraph_ui_menu')
for ordinal in (2,0,1,6,4,7): assert 'Inst(gamemode) = byte:'+str(ordinal) in menu
tutorial=rows('umg__interfaces__ui_gamemode','ExecuteUbergraph_ui_gamemode')
assert 'openTutor(name:tutorial3,' in tutorial
prefix=rows('umg__interfaces__ui_saveSlots','getSavePrefix')
for value in ('s_','i_','a_','b_','l_'):assert repr(value) in prefix
create=rows('umg__interfaces__ui_saveSlots','ExecuteUbergraph_ui_saveSlots')
assert 'Inst(tempSave)->Inst(mainMap) =' in create
maps=(research/'current-pak-files.txt').read_text(encoding='utf-8-sig')
assert '/maps/tutorial3.umap' in maps
processor=rows('objects__propProcessor','ExecuteUbergraph_propProcessor')
assert 'GetDirectoryContentsWithFileInfo' in processor
assert 'EqualEqual_IntInt(Local(CallFunc_Array_Length_ReturnValue_3), Local(CallFunc_Array_Length_ReturnValue_6))' in processor
assert 'BytesToImage(Local(CallFunc_LoadFile_bytes), byte:2' in processor
assert 'Inst(texs_GAMEINST) = Inst(toTex)' in processor
print('PASS cooked mode ordinals, Tutorial first map, save prefixes and native mainMap creation')
print('PASS cooked item importer count-only disk admission and game-instance texture publication')
