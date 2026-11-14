"""Check display-only console clearing against the current cooked Blueprint."""
from pathlib import Path
import hashlib
import json
import sys

root = Path(__file__).resolve().parents[2]
research = root.parent / "_research"
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

asset = research / "current-assets/VotV__Content__umg__ui_console.uasset"
package = Package(str(asset))
for name in ("consoleLine", "logText", "command", "processing"):
    assert name in package.names, name
graph, = [e for e in package.exports if e["name"] == "enterCommand"]
rows = body(package, graph)
assert rows and all("NO VALID SCRIPT" not in text for _, text in rows)
assert any("Inst(consoleLine) = ''" in text and i + 1 < len(rows)
           and "Inst(logText) = ''" in rows[i + 1][1]
           for i, (_, text) in enumerate(rows)), "clr must reset both backing strings"
write, = [e for e in package.exports if e["name"] == "writeToLog"]
assert any("Inst(consoleLine)" in text for _, text in body(package, write))
print(json.dumps({"result": "PASS cooked console clr resets consoleLine and logText",
                  "sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in (asset, asset.with_suffix(".uexp"))}}))
