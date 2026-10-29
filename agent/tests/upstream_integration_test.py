"""Published fork values remain reserved; new upstream messages are distinct."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
protocol = (root / 'src/votv-coop/include/coop/net/protocol.h').read_text()
body = re.search(r'enum class ReliableKind : uint16_t \{(.*?)\n\};', protocol, re.S)[1]
pairs = re.findall(r'^\s*(\w+)\s*=\s*(\d+)', body, re.M)
actual = {name: int(value) for name, value in pairs}
published = json.loads(Path(__file__).with_name('protocol_2214_ids.json').read_text())
assert len(actual) == len(pairs) == len(set(actual.values()))
assert all(actual[name] == value for name, value in published.items())
assert all(0 <= value < 65536 for value in actual.values())
expected_additions = {
    'PlayerHit': 238,
    'PropActRequest': 239,
    'PropActReply': 240,
    'PropActCommit': 241,
    'CreatureHitIntent': 242,
    'ExplosionSpawn': 243,
    'ProjectileFired': 244,
    'ExplosionRequest': 245,
    'SleepingbagIntent': 246,
    'AtvExplodeRequest': 247,
    'PropIgniteIntent': 248,
    'AtvSeat': 249,
    'AtvTakeover': 250,
    'AtvAnswer': 251,
    'AtvSetComplete': 252,
    'LlamaState': 253,
    'FuseTowerState': 254, 'SwingerRestPose': 255,
    'PropSyncClaim': 257, 'PropSyncAnswer': 258, 'DevLoadFiller': 259,
    'DevParityReport': 260, 'DevContainerParity': 261,
    'GeomOctaEvent': 262, 'CoffeeActionIntent': 263,
}
assert {name: value for name, value in actual.items() if name not in published} == expected_additions
version = int(re.search(r'kProtocolVersion\s*=\s*(\d+);', protocol)[1])
release = json.loads((root / 'package/release.json').read_text())
assert version == 2218
assert release['protocol'] == 2217  # last qualified package; this is an unpackaged development integration
assert 'uint16_t kind;     // ReliableKind' in protocol
assert 'sizeof(ReliableHeader) == 8' in protocol
assert 'kPeerCapacity = 32' in (root / 'src/votv-coop/include/coop/net/peer_limits.h').read_text()
print(f'PASS protocol integration: {len(published)} published values retained, {len(expected_additions)} exact additions, 32 peers and development protocol 2218; qualified-package metadata retained')

atv=(root/'src/votv-coop/src/coop/interactables/atv_sync.cpp').read_text()
assert 'coop::atv_steering_sync::Install(session)' in atv
assert 'coop::atv_condition_sync::FillPayload(e.actor,out)' in atv
print('PASS merged ATV wiring: installed steering receiver and native condition read-back before host stamping')
