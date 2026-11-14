"""Reject incomplete/stale qualification while preserving the finished archive and report."""
from pathlib import Path
import copy
import json
import subprocess

root = Path(__file__).resolve().parents[1]
release = json.loads((root / "package/release.json").read_text())
assert release.get("qualification_level") == "offline-test"
path = root / release["qualification"]
original = path.read_bytes()
qualification = json.loads(original.decode("utf-8-sig"))
assert qualification["TwoPlayerGameplayVerified"] is False
controls = []
try:
    for name in ("failed_group", "stale_label", "wrong_dll", "incomplete_suite", "focused_suite"):
        bad = copy.deepcopy(qualification)
        if name == "failed_group":
            bad["Results"][0]["ExitCode"] = 1
        elif name == "stale_label":
            bad["ui"]["packageLabel"] = "Watchers of the Void 0.9.2157"
        elif name == "wrong_dll":
            bad["DllSHA256"] = "0" * 64
        elif name == "incomplete_suite":
            bad["PassingGroups"] = 0
        else:
            bad["FullRegression"] = False
        path.write_text(json.dumps(bad))
        result = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                 str(root / "package/package_current.ps1"), "-VerifyOnly"],
                                capture_output=True, text=True, errors="replace", cwd=root)
        assert result.returncode != 0 and "Offline test package does not match" in result.stderr, (name, result)
        controls.append({"control": name, "rejected": True})
finally:
    path.write_bytes(original)
subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                str(root / "package/package_current.ps1"), "-VerifyOnly"], check=True, cwd=root)
report = json.loads((root / "build/package-current/verification.json").read_text(encoding="utf-8-sig"))
assert report["offlineTestPackage"] is True and report["nativeGameplayVerified"] is False
assert report["fullOfflineGroupCount"] >= 68
(root / "build/package-current/rejection-controls.json").write_text(json.dumps(controls, indent=2) + "\n")
print("PASS test package: failed/incomplete suites, stale UI and mismatched DLL rejected; native acceptance stays false")
