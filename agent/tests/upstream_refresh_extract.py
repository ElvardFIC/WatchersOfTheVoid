from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
source = (root / 'src/votv-coop/src/coop/net/session.cpp').read_text(encoding='utf-8-sig')

def body(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

Path(sys.argv[1]).write_text(
    'namespace coop::net {\n' + '\n'.join(body(s) for s in
    ('int BuildReliableWire_(', 'int BuildReliableBatch_(')) + '\n}\n', encoding='utf-8')
