#!/usr/bin/env python3
"""release_sign -- signs and verifies a main.dll with Ed25519, standard library only.

    python -I -B .github/ci/release_sign.py sign   --dll <path> --target <t> --build <n>
                                                   --out <path> [--test-key]
    python -I -B .github/ci/release_sign.py verify --dll <path> --sig <path> [--trust-test-key]
    python -I -B .github/ci/release_sign.py keygen --id <n> --repo <owner/name>

`sign` writes the six-line `.sig` file the game parses (`coop/build_trust`): the DLL's SHA-256, the
build it belongs to and an Ed25519 signature over `signed_message`. `verify` applies the same
grammar and steps as the game, and prints which step refused. `keygen` makes a key in memory, hands
its private half straight to the `release` environment's secret through `gh`, and prints only the
table row; it refuses first unless that environment, and the rulesets over the refs it admits,
are set as docs/release.md sets them.
The game's check is the vendored ed25519-donna `ed25519_sign_open`
(src/votv-coop/third_party/GameNetworkingSockets/src/external/ed25519-donna/ed25519.c);
`verify` is the same cofactorless, byte-exact check (RFC 8032 section 5.1.7 allows it): it
recomputes R' = [S]B - [k]A, encodes it and accepts only when those 32 bytes equal the
signature's R. It differs from the game in two ways, none of which an honest `sign` can reach:
  - S: the game tests only the top three bits of S; `verify` requires S < L, which is stricter.
  - Points: when the game unpacks the public key it reduces y mod p and applies no x = 0 sign-bit
    rule, so it accepts non-canonical encodings that `verify` refuses. Its key is the compiled
    table.
    R is never decoded, by `verify` or by the game: its bytes are only compared.

The job that holds the release key installs nothing, so the signature scheme is written here from
the algorithm of RFC 8032 section 5.1 (field arithmetic, point encoding and decoding, extended
coordinate addition, key generation, sign, verify) and checked against its section 7.1 vectors by
`release_sign_drill.py`. Python's big integers do the field work.

This is NOT constant-time: it signs one DLL per release on a CI runner. The seed is read from the
environment only, and no message and no exception text this file writes holds it.

Exit codes: 0 done; 1 verification failed; 2 a usage or key-table error; 3 the environment (the
seed, or `gh`, the repository's `release` environment and its rulesets).
Every message is one line on stdout and starts `release_sign: `.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys

# The selftest key: its seed is public by design, it signs only rig and drill builds, and the game
# trusts it only where a developer switch is set. It is never a row of the release table.
TEST_SEED_HEX = "920aa36f330a7de4743b96ac4b67cb724338a15c6a1aecbd1d23e8d377477af1"
TEST_KEY_ID = 255
SEED_ENV = "MULTIVOID_RELEASE_KEY"

_MAX_BUILD = 4294967295
_MESSAGE_TAG = b"multivoid-build-v1"

# --- Ed25519 ------------------------------------------------------------------------------------

_P = 2**255 - 19
_L = 2**252 + 27742317777372353535851937790883648493
_D = (-121665 * pow(121666, _P - 2, _P)) % _P
_SQRT_M1 = pow(2, (_P - 1) // 4, _P)
_Y_MASK = (1 << 255) - 1

# A point is (X, Y, Z, T) in extended homogeneous coordinates: x = X/Z, y = Y/Z, x*y = T/Z.
_NEUTRAL = (0, 1, 1, 0)


def _add(p, q):
    x1, y1, z1, t1 = p
    x2, y2, z2, t2 = q
    a = (y1 - x1) * (y2 - x2) % _P
    b = (y1 + x1) * (y2 + x2) % _P
    c = t1 * 2 * _D * t2 % _P
    d = z1 * 2 * z2 % _P
    e, f, g, h = b - a, d - c, d + c, b + a
    return (e * f % _P, g * h % _P, f * g % _P, e * h % _P)


def _double(p):
    x1, y1, z1, _ = p
    a = x1 * x1 % _P
    b = y1 * y1 % _P
    c = 2 * z1 * z1 % _P
    h = a + b
    e = h - (x1 + y1) * (x1 + y1)
    g = a - b
    f = c + g
    return (e * f % _P, g * h % _P, f * g % _P, e * h % _P)


def _mul(k, p):
    """[k]p, most significant bit first."""
    acc = _NEUTRAL
    for bit in range(k.bit_length() - 1, -1, -1):
        acc = _double(acc)
        if (k >> bit) & 1:
            acc = _add(acc, p)
    return acc


def _negate(p):
    return ((-p[0]) % _P, p[1], p[2], (-p[3]) % _P)


def _encode(p):
    zi = pow(p[2], _P - 2, _P)
    x = p[0] * zi % _P
    y = p[1] * zi % _P
    return (y | ((x & 1) << 255)).to_bytes(32, "little")


def _decode(data):
    """The point a 32-byte string encodes, or None when it encodes none."""
    if len(data) != 32:
        return None
    raw = int.from_bytes(data, "little")
    x_odd = raw >> 255
    y = raw & _Y_MASK
    if y >= _P:
        return None
    yy = y * y % _P
    u = (yy - 1) % _P
    v = (_D * yy + 1) % _P
    # x = (u/v)^((p+3)/8), computed with one modular power for the inversion and the root.
    x = u * pow(v, 3, _P) * pow(u * pow(v, 7, _P), (_P - 5) // 8, _P) % _P
    check = v * x * x % _P
    if check == (-u) % _P:
        x = x * _SQRT_M1 % _P
    elif check != u:
        return None
    if x == 0 and x_odd:
        return None
    if (x & 1) != x_odd:
        x = _P - x
    return (x, y, 1, x * y % _P)


_BASE = _decode((4 * pow(5, _P - 2, _P)).to_bytes(32, "little"))


def _digest_int(*parts):
    return int.from_bytes(hashlib.sha512(b"".join(parts)).digest(), "little")


def _expand(seed):
    """(secret scalar, prefix, public key) of a 32-byte seed."""
    if len(seed) != 32:
        raise ValueError("an Ed25519 seed is 32 bytes")
    h = hashlib.sha512(seed).digest()
    s = int.from_bytes(h[:32], "little")
    s = (s & ((1 << 254) - 8)) | (1 << 254)
    return s, h[32:], _encode(_mul(s, _BASE))


def ed25519_public(seed):
    return _expand(seed)[2]


def ed25519_sign(seed, msg):
    s, prefix, pub = _expand(seed)
    r = _digest_int(prefix, msg) % _L
    r_enc = _encode(_mul(r, _BASE))
    k = _digest_int(r_enc, pub, msg) % _L
    return r_enc + ((r + k * s) % _L).to_bytes(32, "little")


def ed25519_verify(pub, msg, sig):
    """True only for a valid signature; a bad point, a bad length or a wrong type is False."""
    if not (isinstance(pub, (bytes, bytearray)) and isinstance(msg, (bytes, bytearray))
            and isinstance(sig, (bytes, bytearray))):
        return False
    pub, msg, sig = bytes(pub), bytes(msg), bytes(sig)
    if len(pub) != 32 or len(sig) != 64:
        return False
    a = _decode(pub)
    s = int.from_bytes(sig[32:], "little")
    if a is None or s >= _L:
        return False
    k = _digest_int(sig[:32], pub, msg) % _L
    # R' = [S]B - [k]A, compared as 32 encoded bytes with the signature's R (cofactorless).
    r_check = _add(_mul(s, _BASE), _negate(_mul(k, a)))
    return _encode(r_check) == sig[:32]


# --- the message and the .sig file --------------------------------------------------------------


class SignError(Exception):
    """A refusal with its step name and exit code; str(e) is the whole output line."""

    def __init__(self, step, code, message):
        super().__init__(message)
        self.step = step
        self.code = code


def _verify_failed(step):
    return SignError(step, 1, "release_sign: verify failed at " + step)


def signed_message(target, build, sha):
    """Byte for byte coop::build_trust::SignedMessage: tag, length, target, u32le build, sha."""
    t = target.encode("ascii")
    return _MESSAGE_TAG + bytes([len(t)]) + t + build.to_bytes(4, "little") + sha


def format_sig(key_id, target, build, sha, sig):
    return ("multivoid-build-sig 1\nkey %d\ntarget %s\nbuild %d\nsha256 %s\nsig %s\n"
            % (key_id, target, build, sha.hex(), sig.hex()))


_SIG_LINES = (
    re.compile(r"multivoid-build-sig 1", re.ASCII),
    re.compile(r"key ([1-9][0-9]{0,2})", re.ASCII),
    re.compile(r"target ([\x21-\x7e]{1,23})", re.ASCII),
    re.compile(r"build (0|[1-9][0-9]{0,9})", re.ASCII),
    re.compile(r"sha256 ([0-9a-f]{64})", re.ASCII),
    re.compile(r"sig ([0-9a-f]{128})", re.ASCII),
)


def parse_sig(text):
    """The game's grammar: six lines in order, "\\n" or "\\r\\n" ends, a final newline optional."""
    lines = text.split("\n")
    if lines[-1] == "":
        del lines[-1]
    if len(lines) != len(_SIG_LINES):
        raise _verify_failed("grammar")
    values = []
    for line, rule in zip(lines, _SIG_LINES):
        if line.endswith("\r"):
            line = line[:-1]
        m = rule.fullmatch(line)
        if m is None:
            raise _verify_failed("grammar")
        values.append(m.group(1) if m.groups() else None)
    key_id = int(values[1])
    build = int(values[3])
    if key_id > 255 or build > _MAX_BUILD:
        raise _verify_failed("grammar")
    return {"key_id": key_id, "target": values[2], "build": build,
            "sha": bytes.fromhex(values[4]), "sig": bytes.fromhex(values[5])}


# --- the key table ------------------------------------------------------------------------------

_ROW = re.compile(r'MV_RELEASE_KEY\(([1-9][0-9]{0,2}), "([0-9a-f]{64})", "([0-9a-f]{128})"\)',
                  re.ASCII)


def inc_path():
    return (pathlib.Path(__file__).resolve().parents[2]
            / "src/votv-coop/include/coop/build_trust/release_keys.inc")


def format_row(key_id, pub, fixture_sig):
    return 'MV_RELEASE_KEY(%d, "%s", "%s")' % (key_id, pub.hex(), fixture_sig.hex())


def read_rows(text):
    """[(id, pub_hex, fixture_sig_hex)] of the table text; the game's compile-time checks, here."""
    rows = []
    seen = set()
    for number, line in enumerate(text.split("\n"), start=1):
        if line.endswith("\r"):
            line = line[:-1]
        if line == "" or line.startswith("//"):
            continue
        m = _ROW.fullmatch(line)
        if m is None or not 1 <= int(m.group(1)) <= 254:
            raise SignError("table", 2,
                            "release_sign: release_keys.inc line %d is not a row" % number)
        key_id = int(m.group(1))
        if key_id in seen:
            raise SignError("table", 2, "release_sign: release_keys.inc repeats id %d" % key_id)
        seen.add(key_id)
        rows.append((key_id, m.group(2), m.group(3)))
    return rows


def _table_rows():
    path = inc_path()
    if not path.is_file():
        raise SignError("table", 2, "release_sign: release_keys.inc not found at the checkout")
    return read_rows(path.read_bytes().decode("utf-8"))


# --- the commands -------------------------------------------------------------------------------


def load_seed(environ):
    value = environ.get(SEED_ENV)
    if value is None:
        raise SignError("seed", 3, "release_sign: %s is missing" % SEED_ENV)
    if re.fullmatch(r"[0-9a-f]{64}", value, re.ASCII) is None:
        raise SignError("seed", 3, "release_sign: %s is not 64 lowercase hex characters" % SEED_ENV)
    return bytes.fromhex(value)


def _file_sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.digest()


def cmd_sign(args, environ, out):
    if args.test_key:
        seed = bytes.fromhex(TEST_SEED_HEX)
        key_id = TEST_KEY_ID
    else:
        seed = load_seed(environ)
        pub_hex = ed25519_public(seed).hex()
        matches = [row[0] for row in _table_rows() if row[1] == pub_hex]
        if not matches:
            raise SignError("key", 2, "release_sign: the key is not in release_keys.inc")
        key_id = matches[0]

    target = args.target
    if re.fullmatch(r"[\x21-\x7e]{1,23}", target, re.ASCII) is None:
        raise SignError("usage", 2,
                        "release_sign: --target must be 1 to 23 printable ASCII characters "
                        "without a space")
    build_text = str(args.build)
    if re.fullmatch(r"0|[1-9][0-9]{0,9}", build_text, re.ASCII) is None or \
            int(build_text) > _MAX_BUILD:
        raise SignError("usage", 2, "release_sign: --build must be a number from 0 to 4294967295")
    build = int(build_text)

    try:
        sha = _file_sha256(args.dll)
    except OSError:
        raise SignError("read", 2, "release_sign: cannot read --dll") from None
    sig = ed25519_sign(seed, signed_message(target, build, sha))
    try:
        with open(args.out, "w", encoding="ascii", newline="\n") as f:
            f.write(format_sig(key_id, target, build, sha, sig))
    except OSError:
        raise SignError("write", 2, "release_sign: cannot write --out") from None
    out.write("release_sign: signed sha %s with key %d\n" % (sha.hex()[:8], key_id))
    return 0


def cmd_verify(args, out):
    try:
        sig_bytes = pathlib.Path(args.sig).read_bytes()
        sha = _file_sha256(args.dll)
    except OSError:
        raise _verify_failed("read") from None
    try:
        text = sig_bytes.decode("ascii")
    except UnicodeDecodeError:
        raise _verify_failed("grammar") from None
    parsed = parse_sig(text)
    if parsed["sha"] != sha:
        raise _verify_failed("sha")

    pub = None
    for key_id, pub_hex, _fixture in _table_rows():
        if key_id == parsed["key_id"]:
            pub = bytes.fromhex(pub_hex)
    if pub is None and args.trust_test_key and parsed["key_id"] == TEST_KEY_ID:
        pub = ed25519_public(bytes.fromhex(TEST_SEED_HEX))
    if pub is None:
        raise _verify_failed("key")
    msg = signed_message(parsed["target"], parsed["build"], parsed["sha"])
    if not ed25519_verify(pub, msg, parsed["sig"]):
        raise _verify_failed("signature")
    out.write("release_sign: verified key %d\n" % parsed["key_id"])
    return 0


_REPO = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", re.ASCII)
_GH_MISSING = "release_sign: gh is not installed or cannot run"
_BRANCH_POLICIES = {("v*", "tag"), ("main", "branch")}


def _unprotected(check):
    return SignError("protection", 3, "release_sign: the release environment is not protected as "
                     "docs/release.md sets it (%s)" % check)


def _gh_api(runner, path):
    """One read of GitHub's REST answer through gh; its output holds no secret."""
    try:
        p = runner(["gh", "api", path], capture_output=True, text=True, encoding="utf-8")
    except OSError:
        raise SignError("gh", 3, _GH_MISSING) from None
    if p.returncode != 0:
        lines = (p.stderr or "").splitlines()
        raise SignError("gh", 3,
                        ("release_sign: gh api failed: " + (lines[0] if lines else "")).rstrip())
    try:
        return json.loads(p.stdout)
    except (TypeError, ValueError):
        raise SignError("gh", 3, "release_sign: gh api failed (the answer is not JSON)") from None


def _check_protection(environment, policies):
    """The protection docs/release.md sets, read from the REST answers; a reviewer's own fields
    are not read."""
    if not isinstance(environment, dict) or environment.get("can_admins_bypass") is not False:
        raise _unprotected("admins can bypass")
    rules = environment.get("protection_rules")
    rules = rules if isinstance(rules, list) else []
    reviewer_rules = [r for r in rules
                      if isinstance(r, dict) and r.get("type") == "required_reviewers"]
    if not reviewer_rules:
        raise _unprotected("required reviewers")
    for rule in reviewer_rules:
        reviewers = rule.get("reviewers")
        if not isinstance(reviewers, list) or not reviewers:
            raise _unprotected("no reviewer")
        if rule.get("prevent_self_review") is not False:
            raise _unprotected("self review")
    branch_policy = environment.get("deployment_branch_policy")
    if not (isinstance(branch_policy, dict)
            and set(branch_policy) == {"protected_branches", "custom_branch_policies"}
            and branch_policy["protected_branches"] is False
            and branch_policy["custom_branch_policies"] is True):
        raise _unprotected("deployment branch policy")
    listed = policies.get("branch_policies") if isinstance(policies, dict) else None
    pairs = ([(q.get("name"), q.get("type")) for q in listed if isinstance(q, dict)]
             if isinstance(listed, list) else [])
    if len(pairs) != len(_BRANCH_POLICIES) or set(pairs) != _BRANCH_POLICIES:
        raise _unprotected("branch policies")


# The release environment admits the v* tags and main, so whoever may create or move such a tag,
# or push to main, reaches the key. Only an admin may: for each requirement an active ruleset over
# that ref with no exclusion, refusing the rule to all but the admin repository role (actor 5, as
# the live rulesets read). The rulesets are read in list order and any one may hold a requirement.
_ADMIN_ROLE = ("RepositoryRole", 5)
_REQUIREMENTS = (("tag", "refs/tags/v*", "creation"), ("tag", "refs/tags/v*", "update"),
                 ("tag", "refs/tags/v*", "deletion"), ("branch", "refs/heads/main", "update"))


def _unruled(pattern, rule):
    return SignError("protection", 3, "release_sign: the refs the release environment admits are "
                     "not held by rulesets as docs/release.md sets it (%s %s)" % (pattern, rule))


def _ruleset_holds(ruleset, pattern, rule):
    """Whether one ruleset refuses `rule` on `pattern` to all but the admin repository role."""
    conditions = ruleset.get("conditions") if isinstance(ruleset, dict) else None
    names = conditions.get("ref_name") if isinstance(conditions, dict) else None
    if not (isinstance(names, dict) and isinstance(names.get("include"), list)
            and pattern in names["include"] and names.get("exclude") == []):
        return False
    rules = ruleset.get("rules")
    kinds = {r.get("type") for r in rules if isinstance(r, dict)} if isinstance(rules, list) else set()
    if rule not in kinds:
        return False
    bypass = ruleset.get("bypass_actors")
    return isinstance(bypass, list) and all(
        isinstance(a, dict) and (a.get("actor_type"), a.get("actor_id")) == _ADMIN_ROLE
        for a in bypass)


def _check_ref_rulesets(runner, repo):
    listed = _gh_api(runner, "repos/%s/rulesets?per_page=100" % repo)
    read = []
    for summary in listed if isinstance(listed, list) else []:
        if not (isinstance(summary, dict) and summary.get("enforcement") == "active"
                and summary.get("target") in ("tag", "branch")):
            continue
        ruleset_id = summary.get("id")
        if isinstance(ruleset_id, int) and not isinstance(ruleset_id, bool):
            read.append((summary["target"],
                         _gh_api(runner, "repos/%s/rulesets/%d" % (repo, ruleset_id))))
    for target, pattern, rule in _REQUIREMENTS:
        if not any(t == target and _ruleset_holds(r, pattern, rule) for t, r in read):
            raise _unruled(pattern, rule)


def _key_id(text):
    """The strict parse `sign --build` applies to its number: plain ASCII digits, no sign, no
    underscore, no leading zero."""
    if re.fullmatch(r"[1-9][0-9]{0,2}", text, re.ASCII) is None or int(text) > 254:
        raise SignError("usage", 2, "release_sign: --id must be 1..254")
    return int(text)


def cmd_keygen(args, runner, out):
    """The order is the custody: refuse an unprotected environment or unheld release refs, then
    make the key, check its row, hand the seed to gh's standard input, and print the row. No file
    is written."""
    if _REPO.fullmatch(args.repo) is None:
        raise SignError("usage", 2, "release_sign: --repo must be owner/name")
    rows = _table_rows()
    key_id = _key_id(str(args.id))
    highest = max((row[0] for row in rows), default=0)
    if key_id <= highest:
        raise SignError("usage", 2, "release_sign: --id must be greater than %d" % highest)

    base = "repos/%s/environments/release" % args.repo
    environment = _gh_api(runner, base)
    policies = _gh_api(runner, base + "/deployment-branch-policies")
    _check_protection(environment, policies)
    _check_ref_rulesets(runner, args.repo)

    seed = os.urandom(32)
    pub = ed25519_public(seed)
    fixture = ed25519_sign(seed, signed_message("selftest-fixture", 0, bytes(32)))
    row = format_row(key_id, pub, fixture)
    if read_rows(row) != [(key_id, pub.hex(), fixture.hex())]:
        raise SignError("row", 3, "release_sign: the new row did not read back")

    # Never check=True: the exception it raises carries the input, which is the seed. The output
    # is read with errors="replace": a byte that does not decode must not raise after the secret
    # is set and before the row is printed.
    try:
        p = runner(["gh", "secret", "set", SEED_ENV, "--env", "release", "--repo", args.repo],
                   input=seed.hex(), capture_output=True, text=True, encoding="utf-8",
                   errors="replace")
    except OSError:
        raise SignError("gh", 3, _GH_MISSING) from None
    if p.returncode != 0:
        raise SignError("secret", 3, "release_sign: gh secret set failed (exit %d)" % p.returncode)
    out.write(row + "\n")
    return 0


def _parser():
    parser = argparse.ArgumentParser(prog="release_sign", allow_abbrev=False)
    sub = parser.add_subparsers(dest="command", required=True)
    s = sub.add_parser("sign", allow_abbrev=False)
    s.add_argument("--dll", required=True)
    s.add_argument("--target", required=True)
    s.add_argument("--build", required=True)
    s.add_argument("--out", required=True)
    s.add_argument("--test-key", action="store_true")
    v = sub.add_parser("verify", allow_abbrev=False)
    v.add_argument("--dll", required=True)
    v.add_argument("--sig", required=True)
    v.add_argument("--trust-test-key", action="store_true")
    k = sub.add_parser("keygen", allow_abbrev=False)
    k.add_argument("--id", required=True)
    k.add_argument("--repo", required=True)
    return parser


def main(argv, environ=None, out=None, runner=subprocess.run):
    environ = os.environ if environ is None else environ
    out = sys.stdout if out is None else out
    args = _parser().parse_args(argv)
    try:
        if args.command == "sign":
            return cmd_sign(args, environ, out)
        if args.command == "keygen":
            return cmd_keygen(args, runner, out)
        return cmd_verify(args, out)
    except SignError as e:
        out.write(str(e) + "\n")
        return e.code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
