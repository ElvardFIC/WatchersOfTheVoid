from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root.parent/'_research'))
from dump_2135 import Package,body
def script(asset,fn):
 p=Package(str(root.parent/'_research/assets'/f'VotV__Content__{asset}.uasset'))
 e=next(e for e in p.exports if e['name']==fn and p.class_of_export(e)=='Function')
 rows=body(p,e);assert 'NO VALID' not in str(rows);return dict(rows)
console=script('objects__droneConsole','ExecuteUbergraph_droneConsole')
assert console[935]=='if not (Inst(lookat_changeTime)) jump @1083'
assert 'Inst(drone)->Inst(leaveAfter5min) =' in console[1000]
assert console[713]=='pop_flow_if_not(Inst(opened))' # only keyboard has lid gate
drone=script('objects__drone','ExecuteUbergraph_drone')
assert drone[12787]=='pop_flow_if_not(Inst(leaveAfter5min))'
quit=script('umg__interfaces__ui_notebook','quit')
assert 'Inst(clipboard)->Inst(text)' in quit[276]
assert quit[357]=='jump @14' and quit[44]=='op5F(Inst(clipboard), noobj)'
commit=script('umg__interfaces__ui_notebook','BndEvt__MultiLineEditableText_86_K2Node_ComponentBoundEvent_5_OnMultiLineEditableTextCommittedEvent__DelegateSignature')
assert commit[36]=='ExecuteUbergraph_ui_notebook(3116)'
ui=script('umg__interfaces__ui_notebook','ExecuteUbergraph_ui_notebook')
assert 'Inst(clipboard)->Inst(text)' in ui[3711]
assert 'quit' in ui[1838] and 'quit' in ui[2956] # page destroy/tear terminate via quit
load=script('objects__prop_notebook','loadData')
assert 'strings_' in load[252] and ', 2,' in load[252]
assert 'Inst(text)' in load[732]
assert script('objects__prop_notebook','upd')=={0:'return nothing',2:'end_of_script'}
append=script('objects__prop_notebook','ExecuteUbergraph_prop_notebook')
assert 'Array_Append(Inst(text)' in append[372]
src=root/'src/votv-coop'
sub=(src/'src/coop/session/subsystems.cpp').read_text()
for fn in ('Install(&session)','Tick()','OnDisconnect()'):assert 'notebook_refresh::'+fn in sub
assert 'src/coop/props/notebook_refresh.cpp' in (src/'sources.cmake').read_text()
print('PASS native evidence: console priority/lid/timer; notebook commit, close-before-null, page operations, save record and lifecycle wiring')
