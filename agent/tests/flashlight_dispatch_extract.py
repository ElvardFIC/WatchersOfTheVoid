from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
source = (root / 'src/votv-coop/src/coop/dispatch/event_dispatch_entity.cpp').read_text(encoding='utf-8-sig')
start = source.index('case net::ReliableKind::ItemActivate: {')
begin = source.index('{', start)
depth, end = 1, begin + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
path = Path(sys.argv[1])
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(source[start:end], encoding='utf-8')

native = (root / 'src/votv-coop/src/ue_wrap/engine/engine_mainplayer.cpp').read_text(encoding='utf-8-sig')
def function(signature):
    start = native.index(signature)
    begin = native.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (native[end] == '{') - (native[end] == '}')
        end += 1
    return native[start:end]

cache = '\n'.join(line for line in native.splitlines() if line.startswith('void* g_set') and 'Fn' in line)
bodies = [function(signature) for signature in (
    'void* ResolveLightIntensityFn(', 'void* ResolveSceneVisibilityFn(', 'void ResolveSpotConeFns(',
    'bool SetLightIntensity(', 'bool SetSceneComponentVisibility(',
    'bool SetSpotLightOuterConeAngle(', 'bool SetSpotLightInnerConeAngle(')]
path.with_name('flashlight_native_production.inc').write_text(
    'namespace ue_wrap::engine {\nnamespace R=reflection; namespace P=profile;\n' + cache + '\n' +
    '\n'.join(bodies) + '\n}\n', encoding='utf-8')
