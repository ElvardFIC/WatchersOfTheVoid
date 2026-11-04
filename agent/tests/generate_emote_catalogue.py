"""Generate metadata only from the supplied, unmodified EmoteWheel manifest."""
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8-sig")
rows = re.findall(r'\{ anim = "([^"]+)", len = ([\d.]+), loop = (true|false)(?:, snd = "([^"]+)")? \}', source)
assert len(rows) > 316 and len({row[0] for row in rows}) == len(rows)
output = pathlib.Path(__file__).resolve().parents[2] / "src/votv-coop/include/ue_wrap/actors/emote_catalogue.inc"
output.write_text('// Generated from forder-EmoteWheel 1.0.1 manifest.lua; metadata only.\n' + ''.join(
    f'    {{L"{anim}", {length}f, {loop}, ' + (f'L"{sound}"' if sound else 'nullptr') + '},\n'
    for anim, length, loop, sound in rows), encoding="utf-8")
print(f"Generated {len(rows)} animation phases/variants")
