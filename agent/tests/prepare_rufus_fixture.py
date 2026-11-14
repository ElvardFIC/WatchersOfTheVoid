"""Extract actual admission bodies; engine/registry/transport remain fixture boundaries."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
source = (root / 'src/votv-coop/src/coop/creatures/npc_world_enum.cpp').read_text(encoding='utf-8-sig')

def between(text, start, end):
    left = text.index(start)
    return text[left:text.index(end, left)]

enroll = between(source, 'coop::element::ElementId EnrollUntrackedNpcActor(', '// Deferred-spawn sources')
table = between(source, 'constexpr const wchar_t* kConsoleNpcOutputs[]', '// A source\'s output')
queue = between(source, 'using PendingExSpawn=', '// The Func-thunk post-native callback')
catch = between(source, 'void OnBeginDeferredExSpawn(', '\n}  // namespace')
drain = between(source, 'void DrainPendingExSpawns()', 'void ClearPendingExSpawns()')
adoption = (root / 'src/votv-coop/src/coop/creatures/npc_adoption.cpp').read_text(encoding='utf-8-sig')
bind = between(adoption, 'bool BindAsMirror(', '// A candidate local actor')
output = Path(sys.argv[1])
output.write_text(
    'namespace coop::npc_world_enum {\nnamespace R=ue_wrap::reflection;\n'
    'using coop::element::NpcMirrors;\n'
    'struct ExSpawnSource {const wchar_t* cls;const wchar_t* const* outputs;};\n'
    'bool IsWaAllowlistedClass(void*){return false;}\n'
    + enroll + table + queue + catch + drain + '}\n'
    'namespace adoption_fixture { namespace R=ue_wrap::reflection;\n' + bind + '}\n',
    encoding='utf-8', newline='\n')
print('PASS extract actual NPC catch/drain/enroll and adoption bodies')
