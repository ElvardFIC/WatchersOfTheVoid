from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
source=(root/'src/votv-coop/src/ue_wrap/actors/kerfur.cpp').read_text(encoding='utf-8-sig')
start=source.index('bool RunActionName(')
end=source.index('\nbool IsInsertPlayerRead(',start)
Path(sys.argv[1],'omega_verb.inc').write_text(source[start:end],encoding='utf-8')
print('PASS actual Omega native replay extraction')
