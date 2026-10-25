"""Check the extracted 0.9.0n bay-window canvas contract; no runtime acceptance."""
from pathlib import Path
import hashlib
import json
import sys

root = Path(__file__).resolve().parents[2]
research = root.parent / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

asset = research / 'current-assets/VotV__Content__objects__d_window.uasset'
package = Package(str(asset))

def function(name):
    export, = [e for e in package.exports if package.class_of_export(e) == 'Function' and e['name'] == name]
    return dict(body(package, export))

canvas = function('canvas')
graph = function('ExecuteUbergraph_d_window')
assert canvas[0] == 'ExecuteUbergraph_d_window(3972)'
assert graph[3972] == 'jump @3952'
assert graph[3952] == 'push_flow @3897'
assert graph[3957] == 'if not (Inst(cv)) jump @815'
assert graph[815] == 'Inst(cv) = true' and graph[826] == 'jump @800'
assert graph[800] == 'setDraw()'
assert 'RetriggerableDelay(self, 0.25,' in graph[3897]
assert graph[238] == 'endDraw()' and graph[252] == 'Inst(cv) = false'
set_draw = '\n'.join(function('setDraw').values())
assert 'BeginDrawCanvasToRenderTarget(self, Inst(rt_w),' in set_draw
assert 'Inst(canv)' in set_draw and 'K2_DrawMaterial' not in set_draw
assert 'EndDrawCanvasToRenderTarget(self, Inst(cont))' in '\n'.join(function('endDraw').values())
assert graph[1978] == 'canvas()'
assert 'Inst(canv)->Canvas.K2_DrawMaterial(' in graph[2038]
assert 'Inst(a)' in graph[2038] and '0.5, 0.5' in graph[2038]
assert 'Inst(analogPanels)->Inst(isPlayingSignal)' in graph[2960]

sponge_asset = research / 'current-assets/VotV__Content__objects__prop_sponge.uasset'
package = Package(str(sponge_asset))
bubbles = '\n'.join(function('cleanBubbles').values())
strength = '\n'.join(function('getStrength').values())
assert 'Greater_FloatFloat(Inst(soapAmount), 0)' in bubbles
assert 'VictoryFloatMinusEquals(Inst(soapAmount), 5,' in bubbles
assert 'FMax' not in bubbles and 'Clamp' not in bubbles
assert 'Greater_FloatFloat(Inst(soapAmount), 0)' in strength
assert 'Out(strength) = Inst(strength)' in strength

source = (root / 'src/votv-coop/src/ue_wrap/devices/window_canvas.cpp').read_text()
assert 'L"setDraw"' not in source and 'L"endDraw"' not in source
out = root / 'build/window-stroke-check/native-contract.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({
    'game': '0.9.0n', 'scope': 'canvas opens/joins and rearms its native close; no painting until K2_DrawMaterial',
    'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in
              (asset, asset.with_suffix('.uexp'), sponge_asset, sponge_asset.with_suffix('.uexp'))},
    'nativeGameplayVerified': False,
}, indent=2) + '\n')
print('PASS cooked window canvas: native open/join/retrigger/close, signal veto, separate draw admission and valid final-use negative soap')
