"""Pin the cooked repair side effects used by the tower coordinator fixture."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root.parent/'_research'))
from kismet_dis import Package,Dis,find_script

def body(asset,function):
    p=Package(str(root.parent/'_research/assets'/('VotV__Content__'+asset+'.uasset')))
    e=next(x for x in p.exports if x['name']==function and p.class_of_export(x)=='Function')
    code,_=find_script(p.export_data(e))
    assert code is not None,function
    result='\n'.join(line for _,line in Dis(p,code).run())
    assert '!! ' not in result,function
    return result

coord=body('objects__misc__coordRadarDish','solvePuzzle')
radio=body('objects__radiotower','completeMinigame')
for text,array in ((coord,'puzzleLights'),(radio,'puzzleData')):
    assert 'Local(Temp_bool_Variable) = true' in text
    assert 'Array_Set(Inst('+array+')' in text
    assert 'updPuzzle()' in text
assert 'Inst(isBroken) = false' in coord and 'audio_success' in coord
assert 'tryToFix()' in radio
judge=body('objects__radiotower','tryToFix')
assert 'Array_Contains(Inst(puzzleData)' in judge
assert 'Inst(fuses)' not in judge  # do not invent a fuse precondition in the native double
assert 'setBroken(false, true)' in judge
setter=body('objects__radiotower','setBroken')
assert 'Inst(isBroken) = Local(isBroken)' in setter
assert 'stateChanged__DelegateSignature' in setter and 'setblinktimer()' in setter
print('PASS cooked tower contract: repairs fill puzzle arrays, judgement reads cells, setBroken emits stateChanged/timer')
