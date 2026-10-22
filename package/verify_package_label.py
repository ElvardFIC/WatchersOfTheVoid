"""Run the production menu/browser formatter against the generated build header."""
from pathlib import Path
import json
import subprocess

root = Path(__file__).resolve().parents[1]
release = json.loads((root / "package/release.json").read_text())
build = (root / release["dll"]).parent
source = (root / "src/votv-coop/src/coop/session/session_manager.cpp").read_text()
start = source.index("std::string DisplayVersion() {")
end = source.index("\n}", start) + 2
body = source[start:end]
expected = "Watchers of the Void " + release["version_number"]
out = (root / release["qualification"]).parent
fixture = out / "package_label.cpp"
fixture.write_text('#include <string>\n#include <cstdio>\n#include "coop/version.h"\n'
                   + body + '\nint main(){auto label=DisplayVersion();puts(label.c_str());return label=='
                   + json.dumps(expected) + '?0:1;}\n')
exe = out / "package_label.exe"
subprocess.run(["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8",
                f"/I{build / 'generated'}", f"/Fo{out / 'package_label.obj'}", f"/Fe{exe}", str(fixture)], check=True)
result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
assert result.stdout.strip() == expected
for path in ("multiplayer_menu.cpp", "server_browser.cpp", "server_browser_panels.cpp"):
    assert "DisplayVersion()" in (root / "src/votv-coop/src/ui" / path).read_text()
report_path = root / release["qualification"]
report = json.loads(report_path.read_text(encoding="utf-8-sig"))
report["ui"] = {"packageLabel": expected, "displayFormatterVerified": True,
                "scope": "production formatter with generated header; no rendered UI test"}
report_path.write_text(json.dumps(report, indent=2) + "\n")
print("PASS production menu/browser formatter:", expected)
