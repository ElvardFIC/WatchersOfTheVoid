#!/usr/bin/env python3
"""fact_rows_gate -- every game-function row of ue_wrap/facts is well formed and has a reference.

A row (`UE_FACT_FN(ident, "feature", L"class" | nullptr, L"function", Mode)`, declared in the file that
registers it) is what the mod's fingerprint census checks against the reference table of the game
target, `src/votv-coop/include/ue_wrap/facts/fingerprints_<target>.inc` (docs/bytecode-fingerprint.md).
This gate holds, over the mod's source:

  1. a row lives in a .cpp: in a header it would register once per translation unit;
  2. its class and function are string literals and its feature a lowercase id ([a-z0-9_.]): the
     table, this gate and a reader all read them as written;
  3. its mode is one of Declared, Dispatch, AnyClass, RuntimeClass, and an AnyClass row has no class;
  4. its ident is used in its file beyond the declaration: a row nothing registers is a dead fact;
  5. the target's table has an entry for its key -- present or recorded absent for a class row, a
     signature set for an any-class row, and for a run-time class row with no class an entry on at
     least one class (the census keys it on the class its site resolves) -- so a release never ships a
     row the census cannot judge.

  python .github/ci/fact_rows_gate.py           # PASS/FAIL over the tree
  python .github/ci/fact_rows_gate.py --drill   # the gate must fail each planted fault
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
MOD = ROOT / "src" / "votv-coop"
ROW = re.compile(r"UE_FACT_FN\s*\(([^;]*?)\)\s*;", re.S)
ARGS = re.compile(r'^\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*(nullptr|L"[^"]*")\s*,\s*(L"[^"]*")\s*,\s*(\w+)\s*$', re.S)
MODES = {"Declared", "Dispatch", "AnyClass", "RuntimeClass"}
TABLE_FN = re.compile(r'\{L"([^"]*)",\s*L"([^"]*)",')
TABLE_ANY = re.compile(r'\{L"([^"]*)",\s*\d+,\s*\d+,\s*\d+\}')


def target():
    m = re.search(r'set\(VOTVCOOP_GAME_TARGET\s+"([^"]+)"\)', (MOD / "CMakeLists.txt").read_text(encoding="utf-8"))
    return m.group(1) if m else None


def check(files, table_text):
    """files: {path: text}. Returns the list of faults."""
    faults = []
    fns = {(c.lower(), f.lower()) for c, f in TABLE_FN.findall(table_text or "")}
    anys = {f.lower() for f in TABLE_ANY.findall(table_text or "")}
    for path, text in sorted(files.items()):
        if path.endswith(("function_row.h",)):
            continue  # the macro's own definition and its comment example
        for m in ROW.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            where = "%s:%d" % (path, line)
            a = ARGS.match(m.group(1))
            if not path.endswith(".cpp"):
                faults.append("%s: a row outside a .cpp" % where)
            if not a:
                faults.append("%s: a row whose arguments are not (ident, \"feature\", L\"class\"|nullptr, "
                              "L\"function\", Mode) literals" % where)
                continue
            ident, feature, cls, fn, mode = a.groups()
            if not re.fullmatch(r"[a-z0-9_.]+", feature):
                faults.append("%s: feature %r is not a lowercase id" % (where, feature))
            if mode not in MODES:
                faults.append("%s: mode %s is not one of %s" % (where, mode, sorted(MODES)))
            if mode == "AnyClass" and cls != "nullptr":
                faults.append("%s: an AnyClass row names a class" % where)
            if len(re.findall(r"\b%s\b" % re.escape(ident), text)) < 2:
                faults.append("%s: row %s is declared and never used" % (where, ident))
            if table_text is None:
                continue
            fname = fn[2:-1].lower()
            if mode == "AnyClass":
                if fname not in anys:
                    faults.append("%s: no reference entry for any-class row %s" % (where, fname))
            elif cls == "nullptr":
                if fname not in {f for _, f in fns}:
                    faults.append("%s: no reference entry keys run-time class row %s on any class" % (where, fname))
            elif (cls[2:-1].lower(), fname) not in fns:
                faults.append("%s: no reference entry for %s.%s" % (where, cls[2:-1], fn[2:-1]))
    return faults


def tree_files():
    out = {}
    for p in list((MOD / "src").rglob("*")) + list((MOD / "include").rglob("*")):
        if p.suffix in (".cpp", ".h", ".inc", ".hpp"):
            text = p.read_text(encoding="utf-8", errors="replace")
            if "UE_FACT_FN" in text:
                out[str(p.relative_to(ROOT)).replace("\\", "/")] = text
    return out


def drill():
    good_table = ('{L"a_c", L"f", L"a_c.f", 0x1ull, 0x2ull},\n{L"g", 3, 0, 1},\n')
    base = 'UE_FACT_FN(kRowF, "lane", L"A_C", L"f", Declared);\nvoid X() { Use(kRowF); }\n'
    cases = {
        "header row": ({"x/a.h": base}, good_table),
        "non-literal class": ({"x/a.cpp": base.replace('L"A_C"', "kClass")}, good_table),
        "upper-case feature": ({"x/a.cpp": base.replace('"lane"', '"Lane"')}, good_table),
        "unknown mode": ({"x/a.cpp": base.replace("Declared", "Exact")}, good_table),
        "unused row": ({"x/a.cpp": base.replace("Use(kRowF);", "")}, good_table),
        "no reference": ({"x/a.cpp": base.replace('L"f"', 'L"h"').replace("kRowF", "kRowH")}, good_table),
        "any-class with a class": ({"x/a.cpp": 'UE_FACT_FN(kRowG, "lane", L"A_C", L"g", AnyClass);\n'
                                               'void X() { Use(kRowG); }\n'}, good_table),
        "run-time class row keyed on no class": ({"x/a.cpp": 'UE_FACT_FN(kRowR, "lane", nullptr, L"r", RuntimeClass);\n'
                                                            'void X() { Use(kRowR); }\n'}, good_table),
    }
    bad = 0
    for name, (files, table) in cases.items():
        if not check(files, table):
            print("drill: the gate PASSED a planted fault: %s" % name)
            bad += 1
    clean = base + 'UE_FACT_FN(kRowR, "lane", nullptr, L"f", RuntimeClass);\nvoid Y() { Use(kRowR); }\n'
    if check({"x/a.cpp": clean}, good_table):
        print("drill: the gate FAILED a clean row")
        bad += 1
    print("fact_rows_gate drill: %s (%d planted fault(s))" % ("PASS" if not bad else "FAIL", len(cases)))
    return 1 if bad else 0


def main():
    if "--drill" in sys.argv:
        return drill()
    t = target()
    table = MOD / "include" / "ue_wrap" / "facts" / ("fingerprints_%s.inc" % t)
    table_text = table.read_text(encoding="utf-8") if table.exists() else None
    files = tree_files()
    faults = check(files, table_text)
    if table_text is None:
        faults.append("no reference table %s for the game target %s" % (table.relative_to(ROOT), t))
    rows = sum(len(ROW.findall(x)) for p, x in files.items() if not p.endswith("function_row.h"))
    for f in faults:
        print(f)
    print("fact_rows_gate: %s (%d row(s) in %d file(s), reference %s)" % ("PASS" if not faults else "FAIL", rows,
                                                                          len(files), t))
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())
