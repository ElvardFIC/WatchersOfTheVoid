#!/usr/bin/env python3
"""release_sign_keygen_drill -- proves `release_sign.py keygen` keeps the key's custody.

Standard library only. A fake runner answers canned `gh` output and records every call's argv and
keyword arguments, so no process starts and the real `gh` is never run; each run happens inside a
temporary working directory with stdout and stderr captured. The fake answers only the
commands keygen may run (its reads, then the secret), with the output a real `gh` prints (so an echo of it is seen), and acts
on the keyword arguments the way `subprocess.run` does. Three mutants are always run and the same
assertion helper as the green arms must flag each of them; one starts a process, which the guard
below must refuse.

The drill names a repository that does not exist and, for its whole run, refuses the process
routes it can see with an AssertionError, so a bypass of the injected runner through one of them
is a FAIL line and starts nothing. Covered: the shared `subprocess.Popen` and everything built on
it (`run`, `check_output`, `check_call`, `call`, a `run` imported by name); `release_sign`'s own
`subprocess` copy and its own `os` copy (`system`, `popen`, `startfile`, `spawn*`, `exec*`,
`posix_spawn*`, `fork*`, whichever the platform has). Not covered: a name bound before the guard
(`from subprocess import Popen`, `from os import system`), an alias or a function-local import of
the real `os`, `_winapi`, `ctypes` or `multiprocessing` called directly. Detected where it can,
never a sandbox: the drill judges the code, it does not contain it; a new route is for the
custody review of the diff. The guards are restored when the run ends.

Run: python -I -B .github/ci/release_sign_keygen_drill.py
"""
import argparse
import contextlib
import importlib.util
import inspect
import io
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import types

HERE = pathlib.Path(__file__).resolve().parent

_spec = importlib.util.spec_from_file_location("release_sign_drill_lib",
                                               HERE / "release_sign_drill_lib.py")
lib = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(lib)

rs = lib.rs
check = lib.check
guarded = lib.guarded
table_at = lib.table_at
write_ascii = lib.write_ascii
leaks = lib.leaks


REAL_RUN = subprocess.run
REAL_POPEN = subprocess.Popen
REAL_OS = os
# Process names the armed check asks for by name, so a typo in `process_routes` cannot hide a route
# from the guard and from the check at once; a name this platform lacks is skipped.
PROCESS_NAMES = ("system", "popen", "startfile", "spawnl", "spawnv", "execl", "execv",
                 "posix_spawn", "fork")
REPO = "drill-owner/drill-repo"
ENV_ARGV = ["gh", "api", "repos/%s/environments/release" % REPO]
POLICIES_ARGV = ["gh", "api", "repos/%s/environments/release/deployment-branch-policies" % REPO]
RULESETS_ARGV = ["gh", "api", "repos/%s/rulesets?per_page=100" % REPO]
BRANCH_RULESET_ID, TAG_RULESET_ID, MAIN_RULESET_ID = 10, 11, 12
DETAIL_PREFIX = "repos/%s/rulesets/" % REPO


def ruleset_argv(ruleset_id):
    return ["gh", "api", DETAIL_PREFIX + str(ruleset_id)]


READS = [ENV_ARGV, POLICIES_ARGV, RULESETS_ARGV, ruleset_argv(BRANCH_RULESET_ID),
         ruleset_argv(TAG_RULESET_ID), ruleset_argv(MAIN_RULESET_ID)]
SECRET_ARGV = ["gh", "secret", "set", rs.SEED_ENV, "--env", "release", "--repo", REPO]
API_KWARGS = {"capture_output": True, "text": True, "encoding": "utf-8"}
SECRET_KWARGS = dict(API_KWARGS, errors="replace")
UNPROTECTED = ("release_sign: the release environment is not protected as docs/release.md sets "
               "it (%s)")
ID_RANGE = "release_sign: --id must be 1..254\n"
GH_LINE = "Set Actions secret %s for environment release in %s\n" % (rs.SEED_ENV, REPO)
GH_WARNING = "warning: a newer release of gh is available\n"
ABSENT = object()


def canned_env(rule=None, place="last", **over):
    """GitHub's REST answer for a protected environment; `rule` edits the reviewers rule (a value of
    ABSENT drops that key) and `place` puts that rule last, first, or last of three."""
    reviewers = {"id": 2, "node_id": "n2", "type": "required_reviewers",
                 "prevent_self_review": False,
                 "reviewers": [{"type": "Team", "reviewer": {"name": "maintainers", "id": 7}}]}
    reviewers.update(rule or {})
    for key in [k for k, v in reviewers.items() if v is ABSENT]:
        del reviewers[key]
    other = {"id": 1, "node_id": "n1", "type": "branch_policy"}
    rules = {"last": [other, reviewers], "first": [reviewers, other],
             "last of three": [{"id": 0, "node_id": "n0", "type": "wait_timer"}, other,
                               reviewers]}[place]
    env = {"name": "release", "can_admins_bypass": False, "protection_rules": rules,
           "deployment_branch_policy": {"protected_branches": False,
                                        "custom_branch_policies": True}}
    env.update(over)
    return env


def canned_policies(*extra):
    items = [("v*", "tag"), ("main", "branch")] + list(extra)
    return {"total_count": len(items), "branch_policies": [
        {"id": 3 + i, "node_id": "n%d" % i, "name": n, "type": t} for i, (n, t) in enumerate(items)]}


ADMIN_BYPASS = {"actor_id": 5, "actor_type": "RepositoryRole", "bypass_mode": "always"}
WRITE_BYPASS = {"actor_id": 4, "actor_type": "RepositoryRole", "bypass_mode": "always"}
# id -> (target, name, ref pattern, rule types, bypass actors): the live repository's rulesets
# 10 to 12 in its order, and a second v* ruleset (13) for the arm that reads two.
RULESET_SHAPES = {
    BRANCH_RULESET_ID: ("branch", "main-no-force-no-delete", "refs/heads/main",
                        ["deletion", "non_fast_forward"], []),
    TAG_RULESET_ID: ("tag", "v-tags-admin-only", "refs/tags/v*",
                     ["creation", "update", "deletion"], [ADMIN_BYPASS]),
    MAIN_RULESET_ID: ("branch", "main-push-admin-only", "refs/heads/main", ["update"],
                      [ADMIN_BYPASS]),
    13: ("tag", "v-tags-second", "refs/tags/v*", ["creation", "update", "deletion"],
         [ADMIN_BYPASS]),
}


def dropping_absent(values):
    return {k: v for k, v in values.items() if v is not ABSENT}


def canned_rulesets(ids=(BRANCH_RULESET_ID, TAG_RULESET_ID, MAIN_RULESET_ID), edit=None, drop=()):
    """The repository's ruleset list, in the order of `ids` less `drop`; `edit` maps an id to
    overrides of its summary."""
    items = []
    for ruleset_id in ids:
        if ruleset_id in drop:
            continue
        target, name = RULESET_SHAPES[ruleset_id][:2]
        summary = {"id": ruleset_id, "name": name, "target": target, "enforcement": "active",
                   "source_type": "Repository"}
        summary.update((edit or {}).get(ruleset_id, {}))
        items.append(summary)
    return items


def canned_details(edit=None):
    """GitHub's REST answer for each ruleset, by id, shaped as the live repository's; `edit`
    maps an id to overrides (a value of ABSENT drops that key)."""
    details = {}
    for ruleset_id, (target, name, pattern, rules, bypass) in RULESET_SHAPES.items():
        detail = {"id": ruleset_id, "name": name, "target": target, "enforcement": "active",
                  "conditions": {"ref_name": {"exclude": [], "include": [pattern]}},
                  "rules": [{"type": t} for t in rules], "bypass_actors": list(bypass)}
        detail.update((edit or {}).get(ruleset_id, {}))
        details[ruleset_id] = dropping_absent(detail)
    return details


class Uncanned(Exception):
    """The code under test ran a command the fake has no answer for."""


class FakeGh:
    """A runner that answers from canned values and records every call; it starts no process."""

    def __init__(self, environment=None, policies=None, secret=None, rulesets=None, details=None):
        self.environment = canned_env() if environment is None else environment
        self.policies = canned_policies() if policies is None else policies
        self.rulesets = canned_rulesets() if rulesets is None else rulesets
        self.details = canned_details() if details is None else details
        self.secret = secret or (lambda text: (0, GH_LINE, GH_WARNING))
        self.calls = []

    def __call__(self, argv, **kwargs):
        self.calls.append((list(argv), dict(kwargs)))
        if argv == SECRET_ARGV:
            answer = self.secret(kwargs.get("input"))
        elif argv == ENV_ARGV:
            answer = self.environment
        elif argv == POLICIES_ARGV:
            answer = self.policies
        elif argv == RULESETS_ARGV:
            answer = self.rulesets
        elif (len(argv) == 3 and argv[:2] == ["gh", "api"] and argv[2].startswith(DETAIL_PREFIX)
              and argv[2][len(DETAIL_PREFIX):].isdigit()
              and int(argv[2][len(DETAIL_PREFIX):]) in self.details):
            answer = self.details[int(argv[2][len(DETAIL_PREFIX):])]
        else:
            raise Uncanned(" ".join(argv))
        if isinstance(answer, (dict, list)):
            answer = (0, json.dumps(answer), GH_WARNING)
        if isinstance(answer, Exception):
            raise answer
        code, stdout, stderr = answer
        if kwargs.get("check") and code != 0:
            raise subprocess.CalledProcessError(code, argv, stdout, stderr)
        if not kwargs.get("capture_output"):
            # The child writes to the terminal, and the caller gets nothing back.
            sys.stdout.write(stdout if isinstance(stdout, str) else "")
            sys.stderr.write(stderr if isinstance(stderr, str) else "")
            return subprocess.CompletedProcess(argv, code, None, None)
        if kwargs.get("text") and kwargs.get("encoding"):
            errors = kwargs.get("errors", "strict")
            stdout, stderr = (v.decode(kwargs["encoding"], errors) if isinstance(v, bytes) else v
                              for v in (stdout, stderr))
        return subprocess.CompletedProcess(argv, code, stdout, stderr)

    def secret_calls(self):
        return [c for c in self.calls if c[0][:3] == ["gh", "secret", "set"]]


def drive(keygen, fake, table, key_id="2", repo=REPO):
    """Runs a keygen inside a temporary working directory with stdout and stderr captured."""
    out, cap_out, cap_err = io.StringIO(), io.StringIO(), io.StringIO()
    args = argparse.Namespace(id=key_id, repo=repo)
    here = os.getcwd()
    raised = None
    with tempfile.TemporaryDirectory() as work:
        os.chdir(work)
        try:
            with table_at(table), contextlib.redirect_stdout(cap_out), \
                    contextlib.redirect_stderr(cap_err):
                try:
                    rc = keygen(args, fake, out)
                except rs.SignError as e:
                    out.write(str(e) + "\n")
                    rc = e.code
                except SystemExit as e:
                    rc = e.code
                except Exception as e:
                    rc = None
                    raised = type(e).__name__
        finally:
            os.chdir(here)
        left = os.listdir(work)
    return {"rc": rc, "out": out.getvalue() + cap_out.getvalue(), "stderr": cap_err.getvalue(),
            "left": left, "raised": raised}


def via_main(args, runner, out):
    """The command line's own path: the subparser, then main's runner parameter."""
    return rs.main(["keygen", "--id", args.id, "--repo", args.repo], environ={}, out=out,
                   runner=runner)


def refuse(*args, **kwargs):
    raise AssertionError("the drill must never start a process")


def process_routes(namespace):
    """The names in `namespace` that start a process, by name: `system`, `popen`, `startfile` and
    the `spawn*`, `exec*`, `posix_spawn*` and `fork*` families. `dir` lists only the names this
    platform has (`startfile` on Windows, `posix_spawn*` and `fork*` on POSIX)."""
    return [n for n in dir(namespace)
            if n in ("system", "popen", "startfile")
            or n.startswith(("spawn", "exec", "posix_spawn", "fork"))]


@contextlib.contextmanager
def no_process():
    """Refuses the process routes the drill can see, for the run (the module docstring lists what
    is covered and what is not). `subprocess.Popen` is replaced on the shared module, because
    `run`, `check_output`, `check_call`, `call` and a `run` imported by name all reach it through
    the module's globals; `release_sign`'s own `subprocess` (its `run` and `Popen`) and its own
    `os` routes (`process_routes`) are replaced by copies local to that module's namespace.
    Detected where it can, never a sandbox: a name bound before the guard, an alias or a local
    import of the real `os`, `_winapi`, `ctypes` and `multiprocessing` stay open. Every name this
    patches is restored in the `finally`."""
    real_sub, real_os, real_popen = rs.subprocess, rs.os, subprocess.Popen
    sub_stub = types.SimpleNamespace(**vars(real_sub))
    sub_stub.run = sub_stub.Popen = refuse
    os_stub = types.SimpleNamespace(**vars(real_os))
    for name in process_routes(real_os):
        setattr(os_stub, name, refuse)
    rs.subprocess, rs.os = sub_stub, os_stub
    subprocess.Popen = refuse
    try:
        yield
    finally:
        subprocess.Popen = real_popen
        rs.subprocess, rs.os = real_sub, real_os


def keygen_flaws(keygen, fake, table, key_id=2, seeds=None, reads=READS):
    """Every custody rule a green keygen run must keep; an empty list is a clean run. `key_id` is
    the id the run asks for; the seed each run hands to gh is appended to `seeds`; `reads` is the
    exact list of reads the run makes before the secret."""
    table_before = table.read_bytes()
    r = drive(keygen, fake, table, key_id=str(key_id))
    flaws = []

    def need(name, ok):
        if not ok:
            flaws.append(name)

    calls, secrets = fake.calls, fake.secret_calls()
    need("no exception (%s)" % r["raised"], r["raised"] is None)
    need("exit 0", r["rc"] == 0)
    need("stderr empty", r["stderr"] == "")
    last = len(reads)
    need("%d calls, the secret last" % (last + 1),
         len(calls) == last + 1 and len(secrets) == 1 and calls[last] is secrets[0])
    need("the protection reads, exact argv and kwargs",
         [c[0] for c in calls[:last]] == reads and all(c[1] == API_KWARGS for c in calls[:last]))
    need("the dictated gh secret set argv", bool(secrets) and secrets[0][0] == SECRET_ARGV)
    need("the secret set kwargs, exactly, and the input",
         bool(secrets) and "input" in secrets[0][1]
         and {k: v for k, v in secrets[0][1].items() if k != "input"} == SECRET_KWARGS)
    seed_hex = secrets[0][1].get("input") if secrets else None
    need("the input is 64 lowercase hex",
         isinstance(seed_hex, str) and re.fullmatch(r"[0-9a-f]{64}", seed_hex) is not None)
    if not isinstance(seed_hex, str):
        return flaws
    if seeds is not None:
        seeds.append(seed_hex)
    seed = bytes.fromhex(seed_hex) if re.fullmatch(r"[0-9a-f]{64}", seed_hex) else b"\0" * 32
    pub = rs.ed25519_public(seed)
    try:
        rows = rs.read_rows(r["out"])
    except rs.SignError:
        rows = []
    need("exactly the row and a newline", len(rows) == 1 and rows[0][0] == key_id
         and rows[0][1] == pub.hex() and r["out"] == rs.format_row(
             key_id, pub, bytes.fromhex(rows[0][2])) + "\n")
    need("the fixture verifies under the row's key", len(rows) == 1 and rs.ed25519_verify(
        pub, rs.signed_message("selftest-fixture", 0, bytes(32)), bytes.fromhex(rows[0][2])))
    elsewhere = repr([(argv, {k: v for k, v in kw.items() if k != "input"}) for argv, kw in calls])
    need("the seed only in the input", not leaks(seed, elsewhere + r["out"] + r["stderr"]))
    need("the table is unchanged", table.read_bytes() == table_before)
    need("the working directory is empty", r["left"] == [])
    return flaws


def leaking_keygen(args, runner, out):
    seen = []

    def spy(argv, **kwargs):
        seen.append(kwargs.get("input"))
        return runner(argv, **kwargs)

    rc = rs.cmd_keygen(args, spy, out)
    out.write(seen[-1] + "\n")
    return rc


def process_starting_keygen(args, runner, out):
    """Starts a harmless child through release_sign's own references; the guard must refuse it."""
    rs.subprocess.Popen([sys.executable, "-c", "pass"])
    return rs.cmd_keygen(args, runner, out)


def unchecked_keygen(args, runner, out):
    runner(SECRET_ARGV, input=os.urandom(32).hex(), **SECRET_KWARGS)
    return rs.cmd_keygen(args, runner, out)


def one_row_table(tmp):
    table = tmp / "kg.inc"
    write_ascii(table, "// one row\n" + rs.format_row(1, rs.ed25519_public(os.urandom(32)),
                                                       bytes(64)) + "\n")
    return table


def drill_keygen_green(tmp):
    table = one_row_table(tmp)
    user = {"reviewers": [{"type": "User", "reviewer": {"login": "x", "id": 1}}]}
    undecoded = lambda text: (0, "Set Actions secret �\n", "! � warning\n")
    raw_bytes = lambda text: (0, b"Set Actions secret \xff\n", b"! \xfe warning\n")
    seeds = []
    for name, keygen, fake, key_id in (
            ("a Team reviewer", rs.cmd_keygen, FakeGh(), 2),
            ("a User reviewer", rs.cmd_keygen, FakeGh(canned_env(user)), 2),
            ("the reviewer rule first", rs.cmd_keygen, FakeGh(canned_env(place="first")), 2),
            ("the reviewer rule last of three", rs.cmd_keygen,
             FakeGh(canned_env(place="last of three")), 2),
            ("a secret set whose output holds U+FFFD", rs.cmd_keygen, FakeGh(secret=undecoded), 2),
            ("a secret set whose output is not UTF-8", rs.cmd_keygen, FakeGh(secret=raw_bytes),
             2),
            ("the command line through main", via_main, FakeGh(), 2),
            ("--id 254, the top of the range", rs.cmd_keygen, FakeGh(), 254),
            ("--id 100, the first three-digit id", rs.cmd_keygen, FakeGh(), 100),
            ("--id 254 through main", via_main, FakeGh(), 254)):
        flaws = keygen_flaws(keygen, fake, table, key_id, seeds)
        check("keygen green with " + name + (": " + ", ".join(flaws) if flaws else ""), not flaws)
    excluded = {"conditions": {"ref_name": {"exclude": ["refs/tags/v0*"],
                                            "include": ["refs/tags/v*"]}}}
    two_tag_rulesets = FakeGh(rulesets=canned_rulesets(ids=(10, 11, 12, 13)),
                              details=canned_details({TAG_RULESET_ID: excluded}))
    flaws = keygen_flaws(rs.cmd_keygen, two_tag_rulesets, table, 2, seeds,
                         reads=READS + [ruleset_argv(13)])
    check("keygen green with two tag rulesets, the first listed flawed and the second holding"
          + (": " + ", ".join(flaws) if flaws else ""), not flaws)
    check("keygen green: every run's seed is its own and none is the public selftest seed",
          len(seeds) == 11 and len(set(seeds)) == len(seeds) and rs.TEST_SEED_HEX not in seeds)


def drill_keygen_mutants(tmp):
    table = one_row_table(tmp)
    # The process mutant counts as caught only by the guard's own refusal: any other exception (a
    # typo in the mutant) would also be a flaw and would pass the check for the wrong reason.
    for name, mutant, must in (("a keygen that prints the seed", leaking_keygen, None),
                               ("a keygen that skips the protection check", unchecked_keygen, None),
                               ("a keygen that starts a process", process_starting_keygen,
                                "no exception (AssertionError)")):
        flaws = keygen_flaws(mutant, FakeGh(), table)
        check("mutant: %s caught" % name, bool(flaws) and (must is None or must in flaws))


def drill_keygen_refuses(tmp):
    table = one_row_table(tmp)
    refusals = {
        "no reviewer": (canned_env({"reviewers": []}), None, "no reviewer"),
        "no reviewers rule": (canned_env({"type": "wait_timer"}), None, "required reviewers"),
        "prevent_self_review true": (canned_env({"prevent_self_review": True}), None,
                                     "self review"),
        "prevent_self_review absent": (canned_env({"prevent_self_review": ABSENT}), None,
                                       "self review"),
        "admins can bypass": (canned_env(can_admins_bypass=True), None, "admins can bypass"),
        "admin bypass absent": ({k: v for k, v in canned_env().items() if k != "can_admins_bypass"},
                                None, "admins can bypass"),
        "protected_branches true": (canned_env(deployment_branch_policy={
            "protected_branches": True, "custom_branch_policies": True}), None,
            "deployment branch policy"),
        "custom_branch_policies false": (canned_env(deployment_branch_policy={
            "protected_branches": False, "custom_branch_policies": False}), None,
            "deployment branch policy"),
        "an extra deployment_branch_policy key": (canned_env(deployment_branch_policy={
            "protected_branches": False, "custom_branch_policies": True, "extra": True}), None,
            "deployment branch policy"),
        "a third policy": (None, canned_policies(("dev", "branch")), "branch policies"),
        "a duplicated policy": (None, canned_policies(("main", "branch")), "branch policies"),
        "a missing policy": (None, {"total_count": 1, "branch_policies": [
            {"id": 3, "node_id": "n3", "name": "v*", "type": "tag"}]}, "branch policies"),
        "a policy of the wrong type": (None, {"total_count": 2, "branch_policies": [
            {"id": 3, "node_id": "n3", "name": "v*", "type": "branch"},
            {"id": 4, "node_id": "n4", "name": "main", "type": "branch"}]}, "branch policies"),
        "an environment that is not an object": ([1, 2], None, "admins can bypass"),
    }
    # Each: (the ruleset list or None, edits to the ruleset details or None, the requirement the
    # refusal must name). The first requirement a ruleset breaks is the tag's creation.
    creation = ("refs/tags/v*", "creation")
    main_update = ("refs/heads/main", "update")
    tag_rules = lambda *kinds: {TAG_RULESET_ID: {"rules": [{"type": k} for k in kinds]}}
    tag_bypass = lambda *actors: {TAG_RULESET_ID: {"bypass_actors": list(actors)}}
    unruled = {
        "no tag ruleset": (canned_rulesets(drop=(TAG_RULESET_ID,)), None, creation),
        "the tag ruleset only evaluated": (canned_rulesets(
            edit={TAG_RULESET_ID: {"enforcement": "evaluate"}}), None, creation),
        "a ruleset list that is not a list": ({"id": 11}, None, creation),
        "another tag pattern": (None, {TAG_RULESET_ID: {"conditions": {"ref_name": {
            "exclude": [], "include": ["refs/tags/release-*"]}}}}, creation),
        "an exclusion": (None, {TAG_RULESET_ID: {"conditions": {"ref_name": {
            "exclude": ["refs/tags/v0*"], "include": ["refs/tags/v*"]}}}}, creation),
        "no conditions": (None, {TAG_RULESET_ID: {"conditions": None}}, creation),
        "creation allowed": (None, tag_rules("update", "deletion"), creation),
        "update allowed": (None, tag_rules("creation", "deletion"), ("refs/tags/v*", "update")),
        "deletion allowed": (None, tag_rules("creation", "update"), ("refs/tags/v*", "deletion")),
        "a write role may bypass": (None, tag_bypass(ADMIN_BYPASS, WRITE_BYPASS), creation),
        "an OrganizationAdmin with id 5": (None, tag_bypass(
            dict(ADMIN_BYPASS, actor_type="OrganizationAdmin")), creation),
        "a bypass list that is absent": (None, {TAG_RULESET_ID: {"bypass_actors": ABSENT}},
                                         creation),
        "a boolean id on the tag summary": (canned_rulesets(edit={TAG_RULESET_ID: {"id": True}}),
                                            None, creation),
        "main pushes open": (canned_rulesets(drop=(MAIN_RULESET_ID,)), None, main_update),
        "main's ruleset lets a write role bypass": (None, {MAIN_RULESET_ID: {
            "bypass_actors": [ADMIN_BYPASS, WRITE_BYPASS]}}, main_update),
    }
    unruled_text = ("release_sign: the refs the release environment admits are not held by "
                    "rulesets as docs/release.md sets it (%s %s)")
    gh_failed = {
        "the environment missing": (FakeGh(environment=(1, "", "gh: Not Found (HTTP 404)\nmore")),
                                    "release_sign: gh api failed: gh: Not Found (HTTP 404)"),
        "gh missing": (FakeGh(environment=FileNotFoundError("gh")),
                       "release_sign: gh is not installed or cannot run"),
        "a non-JSON answer": (FakeGh(policies=(0, "<html>", "")),
                              "release_sign: gh api failed (the answer is not JSON)"),
        "a ruleset read failed": (FakeGh(details={
            **canned_details(), BRANCH_RULESET_ID: (1, "", "gh: Not Found (HTTP 404)")}),
                                    "release_sign: gh api failed: gh: Not Found (HTTP 404)"),
    }
    cases = [(name, FakeGh(env, pol), UNPROTECTED % check_name)
             for name, (env, pol, check_name) in refusals.items()]
    cases += [(name, FakeGh(rulesets=lst, details=canned_details(edit)), unruled_text % required)
              for name, (lst, edit, required) in unruled.items()]
    cases += [(name, fake, text) for name, (fake, text) in gh_failed.items()]
    for name, fake, text in cases:
        r = drive(rs.cmd_keygen, fake, table)
        check("keygen refuses " + name,
              r["rc"] == 3 and r["out"] == text + "\n" and r["stderr"] == ""
              and not fake.secret_calls() and r["left"] == [])


def drill_keygen_secret(tmp):
    table = one_row_table(tmp)
    fake = FakeGh(secret=lambda text: (1, text, "rejected " + text))
    r = drive(rs.cmd_keygen, fake, table)
    check("keygen secret fails: the message is dictated and holds nothing else",
          r["rc"] == 3 and r["out"] == "release_sign: gh secret set failed (exit 1)\n"
          and r["stderr"] == "" and len(fake.calls) == 7)
    fake = FakeGh(secret=lambda text: FileNotFoundError("gh"))
    r = drive(rs.cmd_keygen, fake, table)
    check("keygen secret set cannot run: exit 3",
          r["rc"] == 3 and r["out"] == "release_sign: gh is not installed or cannot run\n")


def drill_keygen_readback(tmp):
    """The new row must read back BEFORE the secret is set; a row that does not stops the run."""
    table = one_row_table(tmp)
    original = rs.read_rows

    def loses_the_new_row(text):
        return [] if text.startswith("MV_RELEASE_KEY(") else original(text)

    fake = FakeGh()
    rs.read_rows = loses_the_new_row
    try:
        r = drive(rs.cmd_keygen, fake, table)
    finally:
        rs.read_rows = original
    check("keygen row read back: a row that does not is exit 3 and no secret is set",
          r["rc"] == 3 and r["out"] == "release_sign: the new row did not read back\n"
          and r["stderr"] == "" and not fake.secret_calls() and len(fake.calls) == 6
          and r["left"] == [])


def drill_keygen_ids(tmp):
    table = tmp / "kg_ids.inc"
    write_ascii(table, "// two rows\n" + "".join(
        rs.format_row(i, rs.ed25519_public(os.urandom(32)), bytes(64)) + "\n" for i in (3, 5)))
    rank_text = "release_sign: --id must be greater than 5\n"
    repo_text = "release_sign: --repo must be owner/name\n"
    absent = "release_sign: release_keys.inc not found at the checkout\n"
    for label, keygen, where, key_id, repo, text in (
            ("--id 0", rs.cmd_keygen, table, "0", REPO, ID_RANGE),
            ("--id 255", rs.cmd_keygen, table, "255", REPO, ID_RANGE),
            ("--id 5, the highest row", rs.cmd_keygen, table, "5", REPO, rank_text),
            ("--id 4, below it", rs.cmd_keygen, table, "4", REPO, rank_text),
            ("a bad --repo", rs.cmd_keygen, table, "6", "not a repo", repo_text),
            ("a --repo with a trailing path", rs.cmd_keygen, table, "6", REPO + "/extra",
             repo_text),
            ("a --repo with a trailing newline", rs.cmd_keygen, table, "6", REPO + "\n",
             repo_text),
            ("no table", rs.cmd_keygen, tmp / "absent.inc", "6", REPO, absent),
            ("--id 0 with no table: the table is read before --id", rs.cmd_keygen,
             tmp / "absent.inc", "0", REPO, absent),
            ("a bad --repo with no table: the repo is read before the table", rs.cmd_keygen,
             tmp / "absent.inc", "6", "not a repo", repo_text),
            ("--id with a trailing newline on the command line", via_main, table, "7\n", REPO,
             ID_RANGE),
            ("a negative --id on the command line", via_main, table, "-5", REPO, ID_RANGE),
            ("--id 1_0 on the command line", via_main, table, "1_0", REPO, ID_RANGE),
            ("--id +7 on the command line", via_main, table, "+7", REPO, ID_RANGE),
            ("--id 007 on the command line", via_main, table, "007", REPO, ID_RANGE),
            ("--id with a trailing Arabic-Indic digit", via_main, table, "1٥", REPO,
             ID_RANGE),
            ("--id with a leading space", via_main, table, " 7", REPO, ID_RANGE),
            ("an empty --id", via_main, table, "", REPO, ID_RANGE)):
        fake = FakeGh()
        r = drive(keygen, fake, where, key_id=key_id, repo=repo)
        check("keygen ids: %s is exit 2 with no runner call" % label,
              r["rc"] == 2 and r["out"] == text and r["stderr"] == "" and not fake.calls)


def drill_keygen_wiring():
    check("the drill's repository fullmatches release_sign's repo pattern",
          rs._REPO.fullmatch(REPO) is not None)
    check("main's default runner is subprocess.run",
          inspect.signature(rs.main).parameters["runner"].default is REAL_RUN)
    routes = set(process_routes(REAL_OS)) | {n for n in PROCESS_NAMES if hasattr(REAL_OS, n)}
    check("the process guards are armed on the routes they cover (nothing is started to prove it)",
          rs.subprocess is not subprocess and rs.subprocess.run is not REAL_RUN
          and subprocess.Popen is not REAL_POPEN and rs.subprocess.Popen is not REAL_POPEN
          and rs.os is not REAL_OS
          and all(getattr(rs.os, n) is not getattr(REAL_OS, n) for n in routes))


def main():
    with tempfile.TemporaryDirectory() as t, no_process():
        tmp = pathlib.Path(t)
        guarded(drill_keygen_wiring)
        for group in (drill_keygen_green, drill_keygen_mutants, drill_keygen_refuses,
                      drill_keygen_secret, drill_keygen_readback, drill_keygen_ids):
            guarded(group, tmp)
    check("the process guards are restored after the run: subprocess.Popen and release_sign's "
          "subprocess and os",
          subprocess.Popen is REAL_POPEN and rs.subprocess is subprocess and rs.os is REAL_OS)
    return lib.finish("release_sign keygen drill")


if __name__ == "__main__":
    sys.exit(main())
