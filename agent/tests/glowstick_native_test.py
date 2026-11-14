"""Cooked native contracts for the remote glowstick grab, not gameplay acceptance."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root.parent / '_research'))
from kismet_dis import Package, Dis, find_script

assets = root.parent / '_research/current-assets'
package = Package(str(assets / 'VotV__Content__objects__prop_glowstick.uasset'))

def body(name):
    export = next(e for e in package.exports if e['name'] == name and package.class_of_export(e) == 'Function')
    code, _ = find_script(package.export_data(export))
    assert code is not None, name
    rows = Dis(package, code).run()
    assert rows and all('!! ' not in text for _, text in rows), name
    return dict(rows)

prelude = body('playerGrabbed_pre')
assert prelude[36] == 'ExecuteUbergraph_prop_glowstick(1044)'
graph = body('ExecuteUbergraph_prop_glowstick')
assert graph[1044] == 'releaseGlowstick()' and graph[1058] == 'pop_flow'
release = body('releaseGlowstick')
assert release == {
    0: 'Inst(PhysicsConstraint)->PhysicsConstraintComponent.BreakConstraint()',
    32: 'Inst(hook)->SceneComponent.K2_DetachFromComponent(byte:0, byte:0, byte:0, true)',
    71: 'Inst(sticked) = false', 82: 'Inst(stick) = false',
    93: 'Actor.SetActorTickEnabled(false)', 104: 'return nothing', 106: 'end_of_script',
}
# All cooked variants inherit this body without a prelude/release override.
variants = sorted(assets.glob('VotV__Content__objects__prop_glowstick_*.uasset'))
assert variants
for path in variants:
    variant = Package(str(path))
    funcs = {e['name'] for e in variant.exports if variant.class_of_export(e) == 'Function'}
    assert not {'releaseGlowstick', 'playerGrabbed_pre'} & funcs, path.name
    classes = [e for e in variant.exports if variant.class_of_export(e) == 'BlueprintGeneratedClass']
    assert len(classes) == 1 and 'prop_glowstick' in variant.obj(classes[0]['sup']), path.name

drive = (root / 'src/votv-coop/src/coop/props/remote_prop_drive.cpp').read_text()
start = drive[drive.index('void ResolveAndStartDrive('):]
assert 'PrepareGrabPrelude(hold, prop, pose.holdGen, attachable)' in start
assert start.index('if (!candidate.Get()) return; // the prelude') < start.index('void* mesh =')
assert 'return; // do not park a body whose native constraint may still own it' in start
print(f'PASS native glowstick: exact release body, prelude ownership, {len(variants)} inherited variants and drive admission wiring')
