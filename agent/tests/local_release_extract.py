"""Compile the production release edge with deterministic transport and engine boundaries."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
source = (root / 'src/votv-coop/src/coop/player/local_streams.cpp').read_text(encoding='utf-8-sig')

def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

Path(sys.argv[1]).write_text('\n'.join(function(s) for s in (
    'void SendHeldRelease(', 'void ArmKerfurTakeRelease(')), encoding='utf-8')
