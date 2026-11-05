#!/usr/bin/env python3
"""Generate the golden test data of spine-reader: tests/data/fixtures/<case>.{h5,expected.json},
and tests/data/fixtures/manifest.json, the record of which SPINE commit wrote each case.

* The **current set** (``current_*``) is written by one SPINE tag, by default the tag of the
  model (``schema/spine-*.json``); ``--tag`` names another. The **frozen cases** (``v2root``,
  ``v0.11.1_v1``) keep the release that wrote them, and are written only with ``--frozen``.
* The .h5 files are written by SPINE's own writers, one subprocess per SPINE version
  (``spine_worker.py``). The expected values come from ``decode_raw.py`` (plain h5py, no SPINE).
* Cross-checks (all asserted):
    - V1 == V2 decode for the same objects (full and lite), except layout metadata;
    - lite == full minus exactly the fields the schema's ``lite_dropped`` lists;
    - fixed-only == full restricted to its retained, non-pooled columns;
    - decode_raw == SPINE's own HDF5Reader (the version that wrote the file),
      for every stored field the reader returns; plus a report on the fields the
      reader re-derives when it rebuilds the classes;
    - every enum value a current case stores is named by the file's enum.

Run it under the LCG view (needs h5py, numpy):

    source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
    python3 tools/fixtures/make_fixtures.py [--tag vX.Y.Z [--sha SHA] [--schema PATH]] [--frozen]

The SPINE commits are cloned into ``--workdir`` (default ``$TMPDIR/spine-reader-fixtures``),
one folder ``spine_<sha>`` per commit, the first time they are needed.

Exit codes: 0 success (warnings allowed); 1 a check failed; 2 usage error; 3 a worker could not
drive SPINE's API; 4 any other error.
"""

import argparse
import copy
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import spine_checkout  # noqa: E402  (no h5py / numpy)

REPO_ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DEFAULT_OUT = os.path.join(REPO_ROOT, "tests", "data", "fixtures")
DEFAULT_SPINE_REPO = "https://github.com/DeepLearnPhysics/spine"
WORKER = os.path.join(HERE, "spine_worker.py")
MANIFEST = "manifest.json"

EXIT_OK, EXIT_CHECK, EXIT_USAGE, EXIT_TOOLING, EXIT_ERROR = 0, 1, 2, 3, 4

# name -> how it is made. "derive" cases are made from another case's file. The current set takes
# its tag from --tag; the role of a case is in its name ("current_" or not).
CURRENT_CASES = {
    "current_v1": dict(fv=1),
    "current_v2": dict(fv=2),
    "current_v1_lite": dict(fv=1, lite=True),
    "current_v2_lite": dict(fv=2, lite=True),
    "current_v2_fixed_only": dict(derive="current_v2"),
    "current_incomplete": dict(fv=1, products="min", n_events=2, incomplete=True),
    # Not shipped by default (--with-exotic): non-object products (string / list / multi_list
    # kinds). They are only generated, decoded and cross-checked in a scratch directory, to
    # test decode_raw.py on every product kind.
    "current_exotic_v1": dict(fv=1, exotic=True, ship=False),
    "current_exotic_v2": dict(fv=2, exotic=True, ship=False),
}
# Never rewritten unless --frozen; their commits are recorded here and checked on checkout.
FROZEN_CASES = {
    "v2root": dict(tag="v0.16.3", sha="a9ad6c63ce5532429c26b3374ec73c7ecc2f1cea", fv=2,
                   products="core", n_events=3),
    "v0.11.1_v1": dict(tag="v0.11.1", sha="b78a6d64f60ce0be5a24611541df19033620b95a", fv=1,
                       products="core", n_events=3,
                       # SPINE v0.11.1's reader cannot resolve its own class_name "ImageMeta3D":
                       # the reader of the current tag reads it instead
                       reader_fallback=True),
}
CASES = {**CURRENT_CASES, **FROZEN_CASES}

# the cross-checks, by role name: (label, first case, second case)
PAIRS = {
    "v1_v2": ("current_v1", "current_v2"),
    "v1_v2_lite": ("current_v1_lite", "current_v2_lite"),
    "lite": ("current_v1", "current_v1_lite"),
    "fixed_only": ("current_v2", "current_v2_fixed_only"),
}


def role(name):
    return "current" if name.startswith("current_") else "frozen"


def log(*a):
    print(*a, file=sys.stderr, flush=True)


class UsageError(Exception):
    """Exit code 2."""


class ToolError(Exception):
    """An error of the environment or the tooling, reported without a traceback (exit code 4)."""


def import_decoder():
    try:
        import decode_raw  # h5py + numpy
    except ImportError as exc:
        raise ToolError(f"{exc}: run make_fixtures.py where h5py and numpy are installed, for "
                        "instance in the LCG view (see tools/fixtures/README.md)") from exc
    return decode_raw


class WorkerError(Exception):
    """A spine_worker.py subprocess failed; ``code`` is its exit code."""

    def __init__(self, step, cmd, code, stderr):
        super().__init__(step, cmd, code, stderr)
        self.step, self.cmd, self.code, self.stderr = step, cmd, code, stderr

    def __str__(self):
        return f"worker {self.step} failed (exit {self.code}): {' '.join(self.cmd)}\n{self.stderr}"


# ---------------------------------------------------------------------------
# schema
# ---------------------------------------------------------------------------
def committed_schema():
    found = sorted(glob.glob(os.path.join(REPO_ROOT, "schema", "spine-*.json")))
    if len(found) != 1:
        raise UsageError(f"expected one schema/spine-*.json, found {len(found)}; pass --schema")
    return found[0]


def load_schema(path):
    with open(path, encoding="utf-8") as fh:
        schema = json.load(fh)
    if "spine_version" not in schema or "classes" not in schema:
        raise UsageError(f"{path}: not a spine-reader schema (no spine_version or classes)")
    return schema


def lite_dropped(schema):
    """{class name: set of the fields lite mode drops}, from the schema."""
    return {name: set(cls.get("lite_dropped", ())) for name, cls in schema["classes"].items()}


# ---------------------------------------------------------------------------
# SPINE subprocesses
# ---------------------------------------------------------------------------
def run_worker(tag_src, step, *args, log_path=None):
    """Run ``spine_worker.py <step> args`` with the SPINE at ``tag_src``; return its stderr.
    A nonzero exit raises ``WorkerError`` with the worker's exit code."""
    env = dict(os.environ)
    env["PYTHONPATH"] = tag_src + os.pathsep + env.get("PYTHONPATH", "")
    env["PYTHONHASHSEED"] = "0"  # V1 'events' member order follows set() order
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    cmd = ["nice", "-n", "10", sys.executable, WORKER, step, *map(str, args)]
    proc = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if log_path:
        os.makedirs(os.path.dirname(log_path), exist_ok=True)
        with open(log_path, "w", encoding="utf-8") as fh:
            fh.write(f"$ {' '.join(cmd)}\n(exit {proc.returncode})\n{proc.stderr}")
    if proc.returncode != 0:
        raise WorkerError(step, cmd, proc.returncode, proc.stderr)
    return proc.stderr


SUBSTITUTION_RE = re.compile(r"^SUBSTITUTION (\{.*\})$", re.M)


def substitutions_of(stderr):
    return [json.loads(m) for m in SUBSTITUTION_RE.findall(stderr or "")]


# ---------------------------------------------------------------------------
# comparisons
# ---------------------------------------------------------------------------
def diff_docs(a, b, ignore_columns=()):
    """List the differences between two decoded documents (after removing the
    layout metadata and ``ignore_columns`` of ``a`` and ``b`` alike)."""
    problems = []
    if a["num_events"] != b["num_events"]:
        problems.append(f"num_events {a['num_events']} != {b['num_events']}")
    if set(a["products"]) != set(b["products"]):
        problems.append(f"products {sorted(a['products'])} != {sorted(b['products'])}")
    for key in sorted(set(a["products"]) & set(b["products"])):
        pa, pb = a["products"][key], b["products"][key]
        for field in ("kind", "class_name", "scalar"):
            if pa[field] != pb[field]:
                problems.append(f"{key}.{field}: {pa[field]!r} != {pb[field]!r}")
        ca = {c: v for c, v in pa["columns"].items() if c not in ignore_columns}
        cb = {c: v for c, v in pb["columns"].items() if c not in ignore_columns}
        if ca != cb:
            problems.append(f"{key}.columns differ: {sorted(set(ca) ^ set(cb))} "
                            f"{[c for c in set(ca) & set(cb) if ca[c] != cb[c]]}")
        for e, (ea, eb) in enumerate(zip(pa["events"], pb["events"])):
            if pa["kind"] == "objects":
                ea = [{c: v for c, v in r.items() if c not in ignore_columns} for r in ea]
                eb = [{c: v for c, v in r.items() if c not in ignore_columns} for r in eb]
            if ea != eb:
                problems.append(f"{key}.events[{e}] differ")
    return problems


def layout_free(doc):
    d = copy.deepcopy(doc)
    d["case"] = None
    d["layout"] = None
    d["writer"]["format_version"] = None
    return d


def restrict(doc, keys=None, drop_columns=(), drop_by_product=None):
    """A copy of ``doc`` with only the products ``keys``, and without the columns
    ``drop_columns`` (all products) and ``drop_by_product[key]`` (that product)."""
    d = copy.deepcopy(doc)
    if keys is not None:
        d["products"] = {k: v for k, v in d["products"].items() if k in keys}
    for key, p in d["products"].items():
        drop = set(drop_columns) | set((drop_by_product or {}).get(key, ()))
        for c in drop:
            p["columns"].pop(c, None)
        if p["kind"] == "objects":
            for ev in p["events"]:
                for row in ev:
                    for c in drop:
                        row.pop(c, None)
    return d


def lite_problems(full, lite, dropped_by_class):
    """lite == full minus, in each object product, exactly the fields its class's
    ``lite_dropped`` lists (those the full file has)."""
    expected_drop = {}
    for key, p in full["products"].items():
        if p["kind"] == "objects":
            expected_drop[key] = sorted(set(p["columns"]) & dropped_by_class.get(p["class_name"], set()))
    problems = diff_docs(restrict(full, drop_by_product=expected_drop), lite)
    for key, p in full["products"].items():
        if key not in lite["products"]:
            continue
        dropped = sorted(set(p["columns"]) - set(lite["products"][key]["columns"]))
        if dropped != expected_drop.get(key, []):
            problems.append(f"{key}: lite dropped {dropped}, the schema's lite_dropped says "
                            f"{expected_drop.get(key, [])}")
    return expected_drop, problems


# ---------------------------------------------------------------------------
# content checks: the fixtures must exercise what the brief asks for
# ---------------------------------------------------------------------------
OBJECT_LISTS = ("reco_particles", "reco_interactions", "truth_particles", "truth_interactions")
ENUM_COLUMNS = (("reco_particles", "shape"), ("reco_particles", "pid"), ("reco_particles", "chi2_pid"),
                ("truth_particles", "shape"), ("truth_particles", "pid"),
                ("truth_interactions", "current_type"), ("truth_interactions", "interaction_scheme"))


def check_content(doc, full):
    """Conventions (ids = list positions, references in range, best match first)
    always; coverage of the special values only when ``full``. Enum labels are
    checked by ``check_enums``."""
    problems = []
    P = doc["products"]
    n = doc["num_events"]
    for e in range(n):
        ev = {k: P[k]["events"][e] for k in OBJECT_LISTS if k in P}
        for k, rows in ev.items():
            if [r["id"] for r in rows] != list(range(len(rows))):
                problems.append(f"event {e} {k}: ids {[r['id'] for r in rows]} are not the list positions")
        for side in ("reco", "truth"):
            parts, ints = ev.get(f"{side}_particles"), ev.get(f"{side}_interactions")
            other_p = ev.get(f"{'truth' if side == 'reco' else 'reco'}_particles")
            other_i = ev.get(f"{'truth' if side == 'reco' else 'reco'}_interactions")
            if parts is None:
                continue
            for r in parts:
                if r["interaction_id"] not in range(len(ints)):
                    problems.append(f"event {e} {side} particle {r['id']}: interaction_id {r['interaction_id']}")
                if any(m not in range(len(other_p)) for m in r.get("match_ids", ())):
                    problems.append(f"event {e} {side} particle {r['id']}: match_ids {r['match_ids']} out of range")
            for r in ints:
                if any(p not in range(len(parts)) or parts[p]["interaction_id"] != r["id"]
                       for p in r.get("particle_ids", ())):
                    problems.append(f"event {e} {side} interaction {r['id']}: particle_ids {r['particle_ids']}")
                if any(m not in range(len(other_i)) for m in r.get("match_ids", ())):
                    problems.append(f"event {e} {side} interaction {r['id']}: match_ids {r['match_ids']} out of range")
            for r in parts + ints:
                if "match_ids" not in r:  # pooled column, absent from a fixed-only file
                    continue
                ov = r["match_overlaps"]
                if len(ov) != len(r["match_ids"]) or ov != sorted(ov, reverse=True):
                    problems.append(f"event {e} {side} {r['id']}: match_overlaps {ov} not best-first")
    if not full:
        return problems

    bools, specials, strings, empty_var, full_var = set(), set(), [], False, False

    def walk(v, col):
        nonlocal empty_var, full_var
        if isinstance(v, bool):
            bools.add(v)
        elif isinstance(v, str):
            if col["type"] == "string":
                strings.append(v)
            else:
                specials.add(v)
        elif isinstance(v, list):
            if col["variable"]:
                empty_var |= not v
                full_var |= bool(v)
            for x in v:
                walk(x, col)

    for p in P.values():
        if p["kind"] != "objects":
            continue
        for ev in p["events"]:
            for r in ev:
                for c, v in r.items():
                    walk(v, p["columns"][c])
    if bools != {True, False}:
        problems.append(f"bool values seen: {bools}")
    if specials != {"nan", "inf", "-inf"}:
        problems.append(f"special float values seen: {sorted(specials)}")
    if not any(any(ord(ch) > 127 for ch in s) for s in strings):
        problems.append("no non-ASCII string")
    if not (empty_var and full_var):
        problems.append("need both empty and non-empty variable arrays")
    if not any(all(not P[k]["events"][e] for k in OBJECT_LISTS) for e in range(n)):
        problems.append("no event without any particle/interaction")
    if not any(not r["particle_ids"] for ev in P["reco_interactions"]["events"] for r in ev):
        problems.append("no interaction without particles")
    return problems


def check_enums(doc, full):
    """For a current case: every stored value of an enum column is named by the file's enum
    (a failure otherwise). When ``full``, a label the file defines that no row stores is a
    warning, as is an enum column stored as a plain integer. Returns (problems, warnings)."""
    problems, warnings = [], []
    P = doc["products"]
    for prod, col in ENUM_COLUMNS:
        if prod not in P or col not in P[prod]["columns"]:
            continue
        enum = P[prod]["columns"][col]["enum"]
        seen = {r[col] for ev in P[prod]["events"] for r in ev}
        if enum is None:
            if full:
                warnings.append({"kind": "not_an_enum", "name": f"{prod}.{col}",
                                 "detail": "stored as a plain integer, without labels"})
            continue
        unnamed = sorted(seen - set(enum.values()))
        if unnamed:
            problems.append(f"{prod}.{col}: values {unnamed} are not named by the file's enum "
                            f"{enum}")
        unused = sorted(k for k, v in enum.items() if v not in seen)
        if full and unused:
            warnings.append({"kind": "unused_label", "name": f"{prod}.{col}",
                             "detail": "labels no fixture row stores: " + ", ".join(unused)})
    return problems, warnings


# ---------------------------------------------------------------------------
# check against SPINE's own reader
# ---------------------------------------------------------------------------
def num_eq(x, y):
    if isinstance(x, str) or isinstance(y, str):  # "nan" / "inf" / "-inf" / text
        return x == y
    if isinstance(x, bool) or isinstance(y, bool):
        return bool(x) == bool(y)
    return x == y


def val_eq(exp, act):
    if isinstance(exp, list):
        return isinstance(act, list) and len(exp) == len(act) and all(val_eq(e, a) for e, a in zip(exp, act))
    return num_eq(exp, act)


def reader_check(doc, rd):
    """Compare the decoded document with what SPINE's reader returned.

    ``rd["raw"]`` is the reader with ``build_classes=False`` (the stored values);
    ``rd["classes"]`` the rebuilt classes' ``as_dict()`` (derived values
    recomputed by SPINE).  Returns (n_compared, errors, class_report).
    """
    errors, compared = [], 0
    if rd["num_entries"] != doc["num_events"]:
        errors.append(f"reader sees {rd['num_entries']} entries, decode sees {doc['num_events']}")
    # administrative entries the reader adds (it owns `index` and the file/entry provenance)
    admin = {"index", "file_index", "file_entry_index", "source_file_index", "source_file_entry_index",
             "source_file_name", "source_file_size", "source_file_mtime_ns"}
    for i, ev in enumerate(rd["raw"][: doc["num_events"]]):
        extra = set(ev) - set(doc["products"]) - admin
        if extra:
            errors.append(f"event {i}: reader returned unknown products {sorted(extra)}")
        for key, p in doc["products"].items():
            if key not in ev:
                errors.append(f"event {i}: product {key} missing from reader output")
                continue
            got, exp = ev[key], p["events"][i]
            if p["kind"] == "objects":
                rows = got if isinstance(got, list) else [got]
                if len(rows) != len(exp):
                    errors.append(f"event {i} {key}: {len(rows)} objects, expected {len(exp)}")
                    continue
                for r, (erow, grow) in enumerate(zip(exp, rows)):
                    missing = set(erow) - set(grow)
                    if missing:
                        errors.append(f"event {i} {key}[{r}]: reader lacks columns {sorted(missing)}")
                    for c in set(erow) & set(grow):
                        compared += 1
                        if not val_eq(erow[c], grow[c]):
                            errors.append(f"event {i} {key}[{r}].{c}: {erow[c]!r} != reader {grow[c]!r}")
                    unk = set(grow) - set(erow)
                    if unk:
                        errors.append(f"event {i} {key}[{r}]: reader has unexpected columns {sorted(unk)}")
            elif p["kind"] == "string":
                compared += 1
                if exp != [got]:
                    errors.append(f"event {i} {key}: {exp!r} != reader {got!r}")
            elif p["kind"] == "array":
                got_rows = [got] if p["scalar"] else got
                compared += 1
                if not val_eq(exp, got_rows):
                    errors.append(f"event {i} {key}: {exp!r} != reader {got_rows!r}")
            else:  # list / multi_list
                compared += 1
                if not val_eq(exp, got):
                    errors.append(f"event {i} {key}: {exp!r} != reader {got!r}")

    # derived-value report: columns whose value changes when the classes are rebuilt
    report = {}
    if "classes" in rd:
        for i, ev in enumerate(rd["classes"][: doc["num_events"]]):
            for key, rows in ev.items():
                p = doc["products"].get(key)
                if p is None or p["kind"] != "objects":
                    continue
                for erow, grow in zip(p["events"][i], rows):
                    for c, ev_val in erow.items():
                        if c not in grow:
                            report.setdefault(key, {}).setdefault("not_in_class", set()).add(c)
                        elif not val_eq(ev_val, grow[c]):
                            report.setdefault(key, {}).setdefault("recomputed_differently", set()).add(c)
    return compared, errors, report


# ---------------------------------------------------------------------------
# manifest
# ---------------------------------------------------------------------------
def manifest_entry(name, tag, sha, doc):
    return {
        "role": role(name),
        "tag": tag,
        "sha": sha,
        "layout": doc["layout"],
        "lite": bool(doc["writer"]["lite"]),
        "derived_from": CASES[name].get("derive"),
        "h5": name + ".h5",
        "expected": name + ".expected.json",
    }


def read_manifest(path):
    if not path or not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def write_manifest(path, manifest):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2, sort_keys=True)
        fh.write("\n")


def merge_manifest(base, run_top, written, partial):
    """The manifest after a run: the frozen entries of ``base`` (and, in a partial run, its
    current entries too) are carried over; the entries of the cases the run wrote replace
    theirs. The top-level keys describe the run when it wrote a current case."""
    cases = {name: entry for name, entry in base.get("cases", {}).items()
             if entry.get("role") == "frozen" or partial}
    cases.update(written)
    top = {k: base.get(k) for k in ("spine_tag", "spine_sha", "spine_version")}
    top.update(partial=base.get("partial", False), warnings=base.get("warnings", []),
               substitutions=base.get("substitutions", []))
    if any(role(name) == "current" for name in written):
        top.update(run_top)
    return {**top, "cases": dict(sorted(cases.items()))}


# ---------------------------------------------------------------------------
def parse_args(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=DEFAULT_OUT, help="output directory (default: tests/data/fixtures)")
    ap.add_argument("--tag", default=None,
                    help="SPINE tag that writes the current set (default: 'v' + the schema's spine_version)")
    ap.add_argument("--sha", default=None,
                    help="full commit SHA of --tag; given, the tag is not resolved again")
    ap.add_argument("--schema", default=None,
                    help="schema read for lite_dropped (default: the committed schema/spine-*.json)")
    ap.add_argument("--frozen", action="store_true",
                    help="also write the frozen cases (v2root, v0.11.1_v1) with their own releases")
    ap.add_argument("--spine-repo", default=os.environ.get("SPINE_REPO", DEFAULT_SPINE_REPO),
                    help="git URL or path of the SPINE repository (tags are resolved and cloned from it)")
    ap.add_argument("--workdir", default=os.environ.get(
        "SPINE_FIXTURE_WORKDIR", os.path.join(tempfile.gettempdir(), "spine-reader-fixtures")),
        help="where the per-commit SPINE clones (spine_<sha>) live / are created")
    ap.add_argument("--libver", default="v110",
                    help="HDF5 file-format bound for the writers (v110 or earliest = h5py default)")
    ap.add_argument("--only", nargs="+", default=None,
                    help="restrict to these cases (a partial run: the cross-checks whose pair is "
                         "not written are skipped)")
    ap.add_argument("--with-exotic", action="store_true",
                    help="also write the (unshipped) exotic-product cases to --out")
    ap.add_argument("--no-reader-check", action="store_true",
                    help="skip the comparison with SPINE's own reader (a partial run)")
    ap.add_argument("--report", default=None, help="write the reader/derived-field report to this JSON file")
    ap.add_argument("--artifacts-dir", default=None,
                    help="keep the unshipped .h5 and decoded JSON, the reader output and the worker "
                         "logs in this directory")
    ap.add_argument("--manifest", default=None, help="manifest written (default: <out>/manifest.json)")
    ap.add_argument("--manifest-base", default=os.path.join(DEFAULT_OUT, MANIFEST),
                    help="manifest whose frozen entries are carried over, read only "
                         "(default: the committed tests/data/fixtures/manifest.json)")
    ap.add_argument("--manifest-only", action="store_true",
                    help="write the manifest of the cases already in --out, without writing any case")
    args = ap.parse_args(argv)
    if args.sha is not None and not spine_checkout.SHA_RE.match(args.sha):
        raise UsageError(f"--sha must be a full 40-character commit SHA, not {args.sha!r}")
    if args.tag is not None and not args.tag.startswith("v"):
        raise UsageError(f"--tag must be a SPINE tag such as v1.4.0, not {args.tag!r}")
    if args.manifest_only and (args.only or args.frozen or args.with_exotic):
        raise UsageError("--manifest-only writes no case: it takes no --only, --frozen or --with-exotic")
    if args.only:
        unknown = [n for n in args.only if n not in CASES]
        if unknown:
            raise UsageError(f"unknown case(s) {unknown}; the cases are {sorted(CASES)}")
        frozen = [n for n in args.only if role(n) == "frozen"]
        if frozen and not args.frozen:
            raise UsageError(f"{frozen} are frozen cases: they are written only with --frozen")
    args.manifest = args.manifest or os.path.join(args.out, MANIFEST)
    return args


class Run:
    """One run: the arguments, the SPINE checkouts, and what the run found."""

    def __init__(self, args):
        self.args = args
        self.schema_path = args.schema or committed_schema()
        self.schema = load_schema(self.schema_path)
        self.tag = args.tag or "v" + self.schema["spine_version"]
        self._sha = args.sha
        self.srcs = {}
        self.warnings, self.substitutions = [], []
        self.partial = bool(args.only) or args.no_reader_check

    @property
    def sha(self):
        """The commit of the current tag, resolved once when it is first needed."""
        if self._sha is None:
            self._sha = spine_checkout.resolve_tag(self.args.spine_repo, self.tag)
        return self._sha

    def tag_sha(self, name):
        c = CASES[name]
        return (c["tag"], c["sha"]) if role(name) == "frozen" else (self.tag, self.sha)

    def src(self, tag, sha):
        if sha not in self.srcs:
            self.srcs[sha] = spine_checkout.ensure_clone(tag, sha, self.args.spine_repo, self.args.workdir)
        return self.srcs[sha]

    def warn(self, item):
        if item not in self.warnings:
            self.warnings.append(item)

    def log_path(self, name, step):
        if not self.args.artifacts_dir:
            return None
        return os.path.join(self.args.artifacts_dir, "logs", f"{name}.{step}.log")


def manifest_only(run):
    args = run.args
    written, versions = {}, []
    for name in CASES:
        h5 = os.path.join(args.out, name + ".h5")
        exp = os.path.join(args.out, name + ".expected.json")
        if not (os.path.exists(h5) and os.path.exists(exp)):
            continue
        with open(exp, encoding="utf-8") as fh:
            doc = json.load(fh)
        if doc.get("case") != name:
            raise UsageError(f"{exp}: its case is {doc.get('case')!r}, not {name!r}")
        tag, sha = run.tag_sha(name)
        written[name] = manifest_entry(name, tag, sha, doc)
        if role(name) == "current":
            versions.append(doc["writer"]["spine_version"])
    if not written:
        raise UsageError(f"no case in {args.out}")
    if len(set(versions)) > 1:
        raise UsageError(f"the current cases of {args.out} were written by several SPINE versions: "
                         f"{sorted(set(versions))}")
    top = dict(spine_tag=run.tag, spine_sha=run.sha,
               spine_version=versions[0] if versions else run.tag[1:],
               partial=False, warnings=[], substitutions=[])
    manifest = {**top, "cases": dict(sorted(written.items()))}
    write_manifest(args.manifest, manifest)
    log(f"manifest of {len(written)} existing case(s) written to {args.manifest}")
    print(json.dumps({"manifest": args.manifest, "cases": sorted(written), "partial": False}))
    return EXIT_OK


def main(argv=None):
    try:
        args = parse_args(argv)
        run = Run(args)
        if args.manifest_only:
            return manifest_only(run)
        return write_cases(run)
    except UsageError as exc:
        log(f"make_fixtures: {exc}")
        return EXIT_USAGE
    except WorkerError as exc:
        log(f"make_fixtures: {exc}")
        return EXIT_TOOLING if exc.code == 3 else EXIT_ERROR
    except (spine_checkout.CheckoutError, ToolError) as exc:
        log(f"make_fixtures: {exc}")
        return EXIT_ERROR
    except Exception:  # noqa: BLE001  (Python's own exit code, 1, would read as a check failure)
        import traceback
        traceback.print_exc()
        return EXIT_ERROR


def write_cases(run):
    args = run.args
    decode_raw = import_decoder()

    names = [n for n in CURRENT_CASES if args.only is None or n in args.only]
    if args.frozen:
        names += [n for n in FROZEN_CASES if args.only is None or n in args.only]
    run_has_current = any(role(n) == "current" for n in names)
    if run_has_current and run.tag != "v" + run.schema["spine_version"]:
        run.warn({"kind": "schema_tag_mismatch", "name": os.path.basename(run.schema_path),
                  "detail": f"the schema is SPINE {run.schema['spine_version']}, the current set is "
                            f"written by {run.tag}"})
    dropped_by_class = lite_dropped(run.schema)

    os.makedirs(args.out, exist_ok=True)
    if args.artifacts_dir:
        scratch = os.path.join(args.artifacts_dir, "unshipped")
        os.makedirs(scratch, exist_ok=True)
    else:
        scratch = tempfile.mkdtemp(prefix="spine-fixtures-")
    docs, paths, notes = {}, {}, {}

    def out_dir(name):
        return args.out if CASES[name].get("ship", True) or args.with_exotic else scratch

    def shipped(name):
        return out_dir(name) == args.out

    # --- 1. write the .h5 files with SPINE's own writers --------------------
    for name in sorted(names, key=lambda n: "derive" in CASES[n]):
        c = CASES[name]
        tag, sha = run.tag_sha(name)
        path = os.path.join(out_dir(name), name + ".h5")
        paths[name] = path
        if "derive" in c:
            if c["derive"] not in paths:
                paths[c["derive"]] = os.path.join(out_dir(c["derive"]), c["derive"] + ".h5")
            log(f"[{name}] litify(fixed_only) of {c['derive']}")
            notes[name] = run_worker(run.src(tag, sha), "litify", "--src", paths[c["derive"]], "--out", path,
                                     "--libver", args.libver, log_path=run.log_path(name, "litify"))
        else:
            cmd = ["--out", path, "--format-version", c["fv"], "--libver", args.libver,
                   "--products", c.get("products", "all"), "--n-events", c.get("n_events", 5)]
            for flag in ("lite", "incomplete", "exotic"):
                if c.get(flag):
                    cmd.append("--" + flag)
            if role(name) == "current":
                cmd.append("--configure-enums")
            log(f"[{name}] SPINE {tag} ({sha[:8]}) writer, format_version={c['fv']}"
                f"{' lite' if c.get('lite') else ''}{' (not finalized)' if c.get('incomplete') else ''}")
            notes[name] = run_worker(run.src(tag, sha), "write", *cmd, log_path=run.log_path(name, "write"))
            for sub in substitutions_of(notes[name]):
                if sub not in run.substitutions:
                    run.substitutions.append(sub)
                    log(f"note  {sub['name']}: {sub['detail']}")

    # --- 2. decode with the independent decoder ------------------------------
    for name in names:
        c = CASES[name]
        docs[name] = decode_raw.decode(paths[name], case=name, lite=bool(c.get("lite")))
        with open(os.path.join(out_dir(name), name + ".expected.json"), "w", encoding="utf-8") as fh:
            fh.write(decode_raw.dumps(docs[name]))

    # --- 3. cross-checks ------------------------------------------------------
    failures = []

    def check(label, problems):
        if problems:
            failures.append((label, problems))
            log(f"FAIL  {label}")
            for p in problems[:10]:
                log(f"        {p}")
        else:
            log(f"ok    {label}")

    for name in names:
        c = CASES[name]
        if "derive" in c:
            continue
        w = docs[name]["writer"]
        problems = []
        if w["format_version"] != c["fv"]:
            problems.append(f"the file is format_version {w['format_version']}, asked {c['fv']}")
        if (w["complete"] is False) != bool(c.get("incomplete")):
            problems.append(f"info@complete is {w['complete']}, the case is "
                            f"{'' if c.get('incomplete') else 'not '}incomplete")
        check(f"{name}: written as asked (format_version, info@complete)", problems)

    for name in names:
        c = CASES[name]
        if c.get("exotic"):
            continue
        full = c.get("products", "all") == "all" and c.get("n_events", 5) == 5 and "derive" not in c
        if c.get("products") != "min":
            check(f"{name}: conventions" + (" and coverage (bools, nan/inf/-inf, UTF-8, empty arrays, "
                  "empty event)" if full else ""), check_content(docs[name], full))
        if role(name) == "current":
            problems, warnings = check_enums(docs[name], full)
            check(f"{name}: every stored enum value is named by the file's enum", problems)
            for item in warnings:
                run.warn(item)

    def pair(key):
        a, b = PAIRS[key]
        if a in docs and b in docs:
            return a, b
        missing = [n for n in (a, b) if n not in docs]
        if run.partial:
            log(f"skip  cross-check {a} / {b}: {missing} not written by this partial run")
        else:
            check(f"cross-check {a} / {b}", [f"case(s) {missing} not written"])
        return None

    if pair("v1_v2"):
        a, b = PAIRS["v1_v2"]
        check(f"decode({a}) == decode({b})  [only layout/format_version differ]",
              diff_docs(layout_free(docs[a]), layout_free(docs[b])))
    if pair("v1_v2_lite"):
        a, b = PAIRS["v1_v2_lite"]
        check(f"decode({a}) == decode({b})", diff_docs(layout_free(docs[a]), layout_free(docs[b])))
    if pair("lite"):
        a, b = PAIRS["lite"]
        expected_drop, problems = lite_problems(docs[a], docs[b], dropped_by_class)
        check(f"{b} == {a} minus the schema's lite_dropped  (dropped: "
              f"{ {k: v for k, v in expected_drop.items() if v} })", problems)
    if pair("fixed_only"):
        a, b = PAIRS["fixed_only"]
        full, fo = docs[a], docs[b]
        keep = set(fo["products"])
        with_pools = {}
        import h5py
        with h5py.File(paths[a], "r") as f:
            for k in keep:
                g = f["products"][k]
                if g.attrs["kind"] == "objects":
                    for pn in g["variables"]:
                        fields = decode_raw.parse_fields(g["variables"][pn].attrs["fields"])
                        with_pools.setdefault(k, set()).update(fields)
        problems = []
        for k in keep:
            pools = with_pools.get(k, set())
            exp = restrict({"products": {k: full["products"][k]}}, drop_columns=pools)["products"][k]
            got = fo["products"][k]
            if exp != got:
                problems.append(f"{k}: fixed-only differs from full minus pooled columns {sorted(pools)}")
        if not keep <= set(full["products"]):
            problems.append("fixed-only has products the full file lacks")
        check(f"{b} == {a} restricted to {{six default keys + index}} minus every pooled column", problems)

    # --- 4. SPINE's own reader ------------------------------------------------
    # The reader of the writing tag; only v0.11.1_v1 falls back to the current tag's reader. A
    # reader that fails is a check failure, not a tooling error.
    class_report = {}
    if not args.no_reader_check:
        for name in names:
            tag, sha = run.tag_sha(name)
            readers = [(tag, sha)]
            if CASES[name].get("reader_fallback"):
                readers.append((run.tag, run.sha))
            tried = []
            for rtag, rsha in readers:
                if args.artifacts_dir:
                    out = os.path.join(args.artifacts_dir, "reader", f"{name}.{rtag}.json")
                    os.makedirs(os.path.dirname(out), exist_ok=True)
                else:
                    out = os.path.join(scratch, f"{name}.{rtag}.reader.json")
                try:
                    run_worker(run.src(rtag, rsha), "read", "--file", paths[name], "--out", out,
                               log_path=run.log_path(name, f"read.{rtag}"))
                except WorkerError as exc:
                    last = (exc.stderr.strip().splitlines() or ["(no output)"])[-1]
                    log(f"note  {name}: SPINE {rtag} HDF5Reader cannot read this file: {last}")
                    tried.append(f"SPINE {rtag}: {last}")
                    continue
                with open(out) as fh:
                    rd = json.load(fh)
                n, errors, rep = reader_check(docs[name], rd)
                complete = docs[name]["writer"]["complete"]
                if complete is False and not rd["strict_open"].startswith("RuntimeError") or \
                        complete is not False and rd["strict_open"] != "ok":
                    errors.append(f"default reader open: {rd['strict_open']!r} (info@complete={complete})")
                elif complete is False:
                    log(f"note  {name}: SPINE refuses it by default: {rd['strict_open'][:90]}...")
                check(f"{name}: expected == SPINE {rtag} HDF5Reader (build_classes=False), {n} values", errors)
                class_report[name] = {"reader": rtag, "error": rd.get("classes_error"),
                                      "products": {k: {kk: sorted(vv) for kk, vv in v.items()}
                                                   for k, v in rep.items()}}
                break
            else:
                check(f"{name}: SPINE's own reader reads it", tried)
        if args.report:
            with open(args.report, "w") as fh:
                json.dump(class_report, fh, indent=1, sort_keys=True)
        for name, rep in class_report.items():
            if rep["error"]:
                log(f"note  {name}: reader could not rebuild classes: {rep['error']}")
            for k, v in rep["products"].items():
                for kind, cols in v.items():
                    log(f"note  {name}: {k}: {kind}: {', '.join(cols)}")

    # --- 5. manifest ----------------------------------------------------------
    written = {}
    for name in names:
        if shipped(name):
            tag, sha = run.tag_sha(name)
            written[name] = manifest_entry(name, tag, sha, docs[name])
    current_versions = sorted({docs[n]["writer"]["spine_version"] for n in names if role(n) == "current"})
    run_top = dict(spine_tag=run.tag, spine_sha=run.sha if run_has_current else None,
                   spine_version=current_versions[0] if current_versions else None,
                   partial=run.partial, warnings=run.warnings, substitutions=run.substitutions)
    manifest = merge_manifest(read_manifest(args.manifest_base), run_top, written, run.partial)
    if not run_has_current:
        manifest["partial"] = manifest["partial"] or run.partial
    write_manifest(args.manifest, manifest)

    # --- 6. summary -----------------------------------------------------------
    total = 0
    log("\nfile sizes:")
    for name in names:
        h, j = os.path.getsize(paths[name]), os.path.getsize(os.path.join(out_dir(name), name + ".expected.json"))
        total += (h + j) if shipped(name) else 0
        log(f"  {name:24s} h5 {h:7d} B ({h / 1024:6.1f} KiB)   json {j:7d} B ({j / 1024:6.1f} KiB)"
            f"{'' if shipped(name) else '   [not shipped]'}")
        for line in notes.get(name, "").splitlines():
            if "not in this SPINE" in line:
                log("      " + line.strip())
    log(f"  total shipped {total} B ({total / 1024:.1f} KiB)")
    for item in run.warnings:
        log(f"warning  {item['kind']} {item['name']}: {item['detail']}")
    log(f"manifest: {args.manifest}{'  [partial run]' if run.partial else ''}")
    if not args.artifacts_dir:
        shutil.rmtree(scratch, ignore_errors=True)
    print(json.dumps({"spine_tag": run.tag, "spine_sha": run.sha if run_has_current else None,
                      "cases": sorted(names), "partial": run.partial, "failures": len(failures),
                      "warnings": run.warnings, "substitutions": run.substitutions,
                      "manifest": args.manifest}))
    if failures:
        log(f"{len(failures)} check(s) failed")
        return EXIT_CHECK
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
