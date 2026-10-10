#!/usr/bin/env python3
"""workflow_expr_gate -- no ${{ }} expression is pasted into a run: or script: value.

GitHub substitutes an expression into a run: script (and into actions/github-script's
script: JS) as TEXT before the shell or the interpreter parses it, so a value that carries
quotes or $( ) becomes code: a tag name, a dispatch input or a job output turns into a
command in whatever step holds it. A value reaches a script through the step's env: and is
read as a variable ($env:NAME, "$NAME", process.env.NAME), where it stays data.

FAILS on any ${{ inside such a value, in .github/workflows/*.yml and in composite actions:
the rest of the key's line plus every following line that is blank or indented deeper (a
block, a folded scalar, an anchor or tag before it, a quoted scalar over lines, a value on
the next line), a quoted key, a flow mapping. A comment inside the script counts too.
Every run first fires two fixtures (a violating workflow must fail, a clean one must pass);
if either does not hold, the gate cannot be trusted and exits 2.

USAGE
    python .github/ci/workflow_expr_gate.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT_KEY = re.compile(r"^(?P<lead>\s*(?:-\s+)?)[\"']?(?:run|script)[\"']?\s*:(?P<rest>.*)$")
FLOW_KEY = re.compile(r"[{,]\s*[\"']?(?:run|script)[\"']?\s*:")


def violations(text):
    """(line number, line) of every ${{ inside a run:/script: value of one YAML file."""
    found = []
    column = None
    for no, line in enumerate(text.splitlines(), 1):
        if column is not None:
            if not line.strip() or len(line) - len(line.lstrip()) > column:
                if "${{" in line:
                    found.append((no, line.strip()))
                continue
            column = None
        m = SCRIPT_KEY.match(line)
        if m:
            column = len(m.group("lead"))
            if "${{" in m.group("rest"):
                found.append((no, line.strip()))
            continue
        flow = FLOW_KEY.search(line)
        if flow and "${{" in line[flow.end():]:
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
      - name: value on the next line
        run:
          echo ${{ a }}
      - name: quoted scalar over two lines
        run: "echo
          ${{ a }}"
      - name: space before the colon
        run : echo ${{ a }}
      - name: quoted key
        "run": echo ${{ b }}
      - name: tag before the block
        run: !!str |
          echo ${{ a }}
      - name: anchor before the block
        run: &x |
          echo ${{ a }}
      - {name: n, run: "echo ${{ c }}"}
      - uses: actions/github-script@0
        with:
          script: |
            const t = '${{ inputs.tag }}';
"""
FIXTURE_BAD_LINES = [5, 8, 9, 14, 17, 19, 21, 24, 27, 28, 32]
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
      - name: value on the next line, read through env
        env:
          X: ${{ inputs.x }}
        run:
          Write-Host $env:X
      - name: a dedent ends the value
        run: |
          Write-Host ok
        if: ${{ x }}
      - uses: actions/cache@0
        with:
          key: k-${{ hashFiles('x') }}
"""


def fixtures_hold():
    bad = [no for no, _ in violations(FIXTURE_BAD)]
    good = violations(FIXTURE_GOOD)
    if bad != FIXTURE_BAD_LINES:
        print(f"workflow_expr_gate: FIXTURE the violating workflow fired on lines {bad}, expected {FIXTURE_BAD_LINES}")
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
