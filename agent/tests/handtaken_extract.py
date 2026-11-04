from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2];out=Path(sys.argv[1])
s=(root/'src/votv-coop/src/ue_wrap/engine/engine_mainplayer.cpp').read_text(encoding='utf-8-sig')
start=s.index('bool CallMainPlayerUpdateHoldWhile(');end=s.index('\nbool CallMainPlayerUseSelectedAction(',start)
out.joinpath('handtaken_native.inc').write_text(s[start:end],encoding='utf-8')
s=(root/'src/votv-coop/src/ue_wrap/core/reflection_props.cpp').read_text(encoding='utf-8-sig')
m=re.search(r'(?ms)^int32_t InstanceOffset::Of\(.*?^\}',s);assert m
out.joinpath('instance_offset_production.inc').write_text('namespace ue_wrap::reflection {\n'+m.group()+'\n}',encoding='utf-8')
print('PASS actual HandTaken native wrappers and inventory offset extraction')
