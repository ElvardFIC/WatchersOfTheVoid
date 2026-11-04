import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[2]
manifest = root.parent.parent / "forder-EmoteWheel-1.0.1/mod/Scripts/manifest.lua"
catalogue = (root / "src/votv-coop/include/ue_wrap/actors/emote_catalogue.inc").read_text(encoding="utf-8")
actual = re.findall(r'\{L"([^"]+)", ([\d.]+)f, (true|false), (?:L"([^"]+)"|nullptr)\}', catalogue)
if manifest.exists():
    source = manifest.read_text(encoding="utf-8-sig")
    parts = re.findall(r'\{ anim = "([^"]+)", len = ([\d.]+), loop = (true|false)(?:, snd = "([^"]+)")? \}', source)
    assert actual == parts, "Every phase/variant/music association must match manifest order exactly"
    assert len(re.findall(r' id = "', source)) == 316
assert len(actual) == 442
assert len({p[0] for p in actual}) == len(actual)
assert all(float(p[1]) > 0 for p in actual)
assert len({p[3] for p in actual if p[3]}) == len([p for p in actual if p[3]])
print(f"PASS EmoteWheel 1.0.1 catalogue: {len(actual)} phases/variants and all sound associations; external mod optional")
