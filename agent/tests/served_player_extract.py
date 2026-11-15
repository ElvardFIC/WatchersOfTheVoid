from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
source=(root/'src/votv-coop/src/ue_wrap/actors/kerfur.cpp').read_text(encoding='utf-8-sig')
start=source.index('bool IsInsertPlayerReadWhile(')
end=source.index('\nbool ReadHasFloppy(',start)
Path(sys.argv[1],'served_insert.inc').write_text(source[start:end],encoding='utf-8')
assert 'IsInsertPlayerReadWhile' in source[start:end]
print('PASS actual insert-read wrapper extraction')
