"""Check added rate coverage against the existing local 0.9.0n corpus."""
from pathlib import Path
import hashlib
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

path = research / 'assets/VotV__Content__objects__tickers__ticker_eyers.uasset'
p = Package(str(path))
graph = next(e for e in p.exports if e['name'] == 'ExecuteUbergraph_ticker_eyers')
rows = dict(body(p, graph))
text = '\n'.join(rows.values())
assert 'RandomFloatInRange(1800, 2700)' in rows[214]
assert 'SetActorTickInterval' in rows[252]
assert 'RandomBoolWithWeight(0.05)' in rows[271]
assert 'timeZ).Z, 4)' in rows[296]
assert 'timeZ).X, 20)' in rows[383] and 'timeZ).X, 4)' in rows[470]
assert 'eyer.eyer_C' in text and 'FinishSpawningActor' in text
source = Path(__file__).resolve().parents[2] / 'src/votv-coop/src/coop/creatures/owner_entity_sync.cpp'
assert 'L"eyer_C"' in source.read_text(encoding='utf-8')
print('PASS existing native night-eye wait, day/night eligibility, 5% chance and owner-entity product')
print('uasset SHA256', hashlib.sha256(path.read_bytes()).hexdigest())
print('uexp SHA256', hashlib.sha256(path.with_suffix('.uexp').read_bytes()).hexdigest())
