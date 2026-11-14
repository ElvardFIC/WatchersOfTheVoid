"""Run upstream's pure selftests against their production decision bodies."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2] / 'src/votv-coop/src'
def block(text, signature, namespace=False):
    start = text.index(signature)
    begin = text.index('{', start) if namespace else start + re.search(r'\)\s*\{', text[start:]).end()-1
    end, depth = begin+1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

pieces = ['''#include "coop/props/container_act_host.h"
#include "coop/props/container_act_request.h"
#include "coop/props/container_record_edit.h"
#include "coop/props/container_slice_wire.h"
#include "coop/props/container_truth.h"
#include "ue_wrap/actors/container_inventory.h"
#include <algorithm>
#include <cwchar>
#include <cstring>
''']
for filename, owner in [
    ('container_act_host.cpp','container_act_host'),
    ('container_act_host_put.cpp','container_act_host'),
    ('container_act_request.cpp','container_act_request'),
]:
    source = (root/'coop/props'/filename).read_text(encoding='utf-8-sig')
    pieces += ['namespace coop::props::'+owner+' {\nnamespace cr=coop::props::container_reach;\nnamespace cas=coop::props::container_act_state;\nnamespace caw=coop::props::container_act_wire;\nnamespace SR=ue_wrap::save_record;\n', block(source,'namespace internal {',True), '\n}\n']
source = (root/'coop/props/container_act_host.cpp').read_text(encoding='utf-8-sig')
pieces += ['namespace coop::props::container_act_host {', block(source,'const char* TallyName('), '}']
source = (root/'coop/props/container_record_edit.cpp').read_text(encoding='utf-8-sig')
pieces += ['namespace coop::props::container_record_edit::internal {',block(source,'size_t ClampIndex('),'}']
source = (root/'coop/props/container_act_put.cpp').read_text(encoding='utf-8-sig')
put = block(source,'namespace coop::props::container_act_request::internal {',True)
begin = put.index('uint32_t PutExclusionRowsOf(')
removed = block(put,'uint32_t PutExclusionRowsOf(')
pieces += [put.replace(removed,'')]
source = (root/'coop/props/container_truth.cpp').read_text(encoding='utf-8-sig')
pieces += ['namespace coop::props::container_truth {',block(source,'RefusalOnce::Say RefusalOnce::Note('),block(source,'bool TruthDue('),block(source,'bool SlotTruthDue('),'}']
source = (root/'coop/props/container_record_edit.cpp').read_text(encoding='utf-8-sig')
pieces += ['namespace coop::props::container_record_edit::internal { namespace SR=ue_wrap::save_record; namespace ci=ue_wrap::container_inventory;',block(source,'void NeuterIfNested('),'}']
Path(sys.argv[1]).write_text('\n'.join(pieces),encoding='utf-8')
