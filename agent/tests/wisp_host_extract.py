from pathlib import Path
import sys

root=Path(__file__).resolve().parents[2]
source=(root/'src/votv-coop/src/coop/creatures/wisp_attack_sync.cpp').read_text(encoding='utf-8-sig')
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def body(signature):
    start=source.index(signature);begin=source.index('{',start);end=begin+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
parts=[body(key)+';' for key in ('struct PendingDestroy','struct NpcWatch','struct Aggro','struct Closing')]
for key in ('constexpr float    kLiftCmPerSec','constexpr float    kLiftMaxStepCm','constexpr uint64_t kNpcWatchWindowMs','constexpr uint32_t kKillDelayMs'):
    start=source.index(key);parts.append(source[start:source.index(';',start)+1])
(out/'wisp_host_types.inc').write_text('\n'.join(parts),encoding='utf8')
(out/'wisp_host_pending.inc').write_text('\n'.join(body(key) for key in (
    'void DischargePendingDestroys(', 'bool RefreshHostWorld(',
    'void EnrollNpcKillWatch(', 'void DischargeNpcKillWatch(',
    'void ObserveWispBody(', 'bool PlayerAlive(', 'bool RelayGrab(')),encoding='utf8')
cleanup=body('void OnDisconnect(')
assert cleanup.index('++g_pendingEpoch')<cleanup.index('g_pendingDestroy.clear()')<cleanup.index('coop::ragdoll_gate::Release')
assert cleanup.index('g_canRagdollForced=false')<cleanup.index('coop::ragdoll_gate::Release')
assert cleanup.index('g_wispBodies.clear()')<cleanup.index('coop::ragdoll_gate::Release')
relay=body('bool RelayGrab(')
assert relay.index('const uint64_t started')<relay.index('SendReliableToSlot')<relay.index('OnWispTear(t, 0)')<relay.index('g_pendingDestroy.push_back')
assert 'if (!s->SendReliableToSlot' in relay
print('PASS host pending destruction owns original body/world and teardown retires work before native ragdoll release')
