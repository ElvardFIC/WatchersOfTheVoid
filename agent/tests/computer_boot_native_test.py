"""Check desktop/boot ownership against the existing 0.9.0n laptop corpus."""
from pathlib import Path
import sys

research = Path(__file__).resolve().parents[3] / "_research"
sys.path.insert(0, str(research))
from kismet_dis import Package
from dump_2135 import body

package = Package(str(research / "assets/VotV__Content__objects__laptop.uasset"))
export, = [entry for entry in package.exports if entry["name"] == "ExecuteUbergraph_laptop"]
rows = dict(body(package, export))
assert rows[29] == "Inst(widget)->Inst(rootswitch)->SetActiveWidgetIndex(0)"
assert rows[329] == "Inst(widget)->Inst(rootswitch)->SetActiveWidgetIndex(1)"
assert "RetriggerableDelay" in rows[256] and "skipconst:315" in rows[256]
assert rows[550] == "Inst(anim) = false"
assert "pcLaunched__DelegateSignature" in rows[561] and "Inst(isOpened)" in rows[561]
print("PASS native computer boot owns rootswitch 0/1, latent completion, anim and launched notification")
