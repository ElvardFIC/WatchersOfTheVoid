#!/usr/bin/env python3
"""workflow_expr_gate -- no ${{ }} expression is pasted into a run: script.

GitHub substitutes an expression into a run: script as TEXT before the shell parses it, so
a value that carries quotes or $( ) becomes code: a tag name, a dispatch input or a job
output turns into a command in whatever step holds it. A value reaches a script through
the step's env: and is read as a variable ($env:NAME, "$NAME"), where it stays data.

FAILS on any ${{ inside a run: value, block or inline, in .github/workflows/*.yml and in
composite actions. A comment line inside the script counts: GitHub substitutes there too.
Every run first fires two fixtures (a violating workflow must fail, a clean one must pass);
if either does not hold, the gate cannot be trusted and exits 2.

USAGE
    python .github/ci/workflow_expr_gate.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RUN_KEY = re.compile(r"^(?P<lead>\s*(?:-\s+)?)run:(?P<rest>.*)$")
BLOCK = re.compile(r"^\s*[|>][-+0-9]*\s*(#.*)?$")


def violations(text):
    """(line number, line) of every ${{ inside a run: value of one YAML file."""
    found = []
    block_indent = None
    for no, line in enumerate(text.splitlines(), 1):
        if block_indent is not None:
            body = line.strip()
            if body and len(line) - len(line.lstrip()) <= block_indent:
                block_indent = None
            else:
                if "${{" in line:
                    found.append((no, line.strip()))
                continue
        m = RUN_KEY.match(line)
        if not m:
            continue
        if BLOCK.match(m.group("rest")):
            block_indent = len(m.group("lead"))
        elif "${{" in m.group("rest"):
            found.append((no, line.strip()))
    return found


FIXTURE_BAD = """jobs:
  a:
    steps:
      - name: inline
        run: echo "${{ inputs.x }}"
      - shell: pwsh
        run: |
          $t = '${{ github.event.inputs.tag }}'
          # ${{ secrets.K }} in a comment is substituted too
        env:
          OK: ${{ inputs.y }}
"""
FIXTURE_GOOD = """jobs:
  a:
    runs-on: ${{ matrix.os }}
    steps:
      - name: env passes the value
        if: ${{ inputs.go }}
        env:
          TAG: ${{ inputs.tag }}
        run: |
          Write-Host "tag $env:TAG"
      - uses: actions/cache@0
        with:
          key: k-${{ hashFiles('x') }}
"""


def fixtures_hold():
    bad = [no for no, _ in violations(FIXTURE_BAD)]
    good = violations(FIXTURE_GOOD)
    if bad != [5, 8, 9]:
        print(f"workflow_expr_gate: FIXTURE the violating workflow fired on lines {bad}, expected [5, 8, 9]")
        return False
    if good:
        print(f"workflow_expr_gate: FIXTURE the clean workflow fired: {good}")
        return False
    return True


def main():
    if not fixtures_hold():
        return 2
    files = sorted((ROOT / ".github" / "workflows").glob("*.y*ml"))
    files += sorted((ROOT / ".github" / "actions").glob("**/action.y*ml"))
    if not files:
        print("workflow_expr_gate: FAIL no workflow found under .github/workflows")
        return 2
    bad = 0
    for path in files:
        for no, line in violations(path.read_text(encoding="utf-8")):
            rel = path.relative_to(ROOT).as_posix()
            print(f"{rel}:{no}: an expression inside a run: script -- pass it through env: ({line})")
            bad += 1
    if bad:
        print(f"workflow_expr_gate: FAIL {bad} expression(s) inside run: scripts")
        return 1
    print(f"workflow_expr_gate: PASS {len(files)} workflow files, no expression inside a run: script")
    return 0


if __name__ == "__main__":
    sys.exit(main())
