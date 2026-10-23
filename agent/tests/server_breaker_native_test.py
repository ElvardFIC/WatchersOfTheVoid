"""Fresh-pak proof of server-breaker eligibility and both concrete break outputs.

The frequency route remains inadmissible until its protection-floppy consumption,
sound and delegate consequences have a verified authority/effect route as well.
"""
from pathlib import Path
import hashlib
import sys

research = Path(__file__).resolve().parents[3] / '_research'
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

assets = Path(sys.argv[1]) if len(sys.argv) > 1 else research / 'current-assets'


def graph(asset, function):
    path = assets / f'VotV__Content__objects__{asset}.uasset'
    package = Package(str(path))
    export = next(e for e in package.exports if e['name'] == function
                  and package.class_of_export(e) == 'Function')
    rows = dict(body(package, export))
    assert rows and not any('!! ' in row or 'NO VALID SCRIPT' in row for row in rows.values())
    return rows


ticker = graph('tickers__ticker_serverBreaker', 'ExecuteUbergraph_ticker_serverBreaker')
assert graph('tickers__ticker_serverBreaker', 'ReceiveTick')[18] == 'ExecuteUbergraph_ticker_serverBreaker(433)'
for offset, value in ((479, 0), (532, 1), (623, 6)):
    assert f'byte:{value})' in ticker[offset]
assert 'Divide_FloatFloat' in ticker[728] and 'totalTime), 25200)' in ticker[728]
assert 'FClamp(' in ticker[814] and ', 0, 1)' in ticker[814]
assert 'Lerp(1200, 600,' in ticker[861] and 'SetActorTickInterval' in ticker[908]
assert 'Less_IntInt' in ticker[927] and 'brokenServers), 5)' in ticker[927]
for offset, chance in ((997, '.45'), (1020, '.4'), (1043, '.35'), (1066, '.3'), (1089, '.25')):
    assert ticker[offset].endswith('0' + chance)
assert 'RandomBoolWithWeight(switch(' in ticker[1161] and 'Inst(difficulty)' in ticker[1112]
assert 'getNamedDaytime(' in ticker[10]
assert 'Divide_FloatFloat' in ticker[132] and ', 7)' in ticker[132]
assert 'FClamp(' in ticker[174] and 'RandomBoolWithWeight' in ticker[268]
assert 'Array_Random' in ticker[311] and 'Inst(servers)' in ticker[311]
assert ticker[392].endswith('->breakServer()')

native = graph('serverBox', 'breakServer')
assert 'canBreak(' in native[5] and 'pop_flow_if_not' in native[28]
assert 'Array_Length(Inst(floppyData))' in native[84]
assert "Contains(Local(CallFunc_Array_Get_Item), '$srv', false, false)" in native[286]
assert 'Array_Remove(Inst(floppyData)' in native[333]
assert native[383] == 'Inst(server_repair)->Play(0)'
assert native[424] == 'serverBroke__DelegateSignature(Inst(serverBroke), self, true)'
assert native[445] == 'jump @849'  # protection consumes a row and bypasses the broken-state branch
assert native[470] == 'Inst(isBroken) = true' and native[481] == 'check()'
assert native[586].startswith('Inst(gamemode)->Inst(brokenServers) =')
assert native[635] == 'serverBroke__DelegateSignature(Inst(serverBroke), self, false)'
assert native[656] == 'Inst(gamemode)->calcServerEff()'
assert 'getRandomServerMinigameType(' in native[767] and native[821].startswith('Inst(minigame) =')
chance = graph('serverBox', 'canBreak')
assert 'Inst(upgrades)' in chance[0] and 'Divide_FloatFloat' in chance[37]
assert 'MultiplyMultiply_FloatFloat' in chance[79]
assert 'Lerp(1, 0.1,' in chance[121] and 'RandomBoolWithWeight' in chance[168]

print('PASS fresh native server-breaker eligibility/wait, difficulty/day/upgrade probability, chosen box and protection-floppy/delegate versus broken-state branches')
for name in ('tickers__ticker_serverBreaker', 'serverBox'):
    for suffix in ('.uasset', '.uexp'):
        path = assets / f'VotV__Content__objects__{name}{suffix}'
        print(path.name, 'SHA256', hashlib.sha256(path.read_bytes()).hexdigest())
