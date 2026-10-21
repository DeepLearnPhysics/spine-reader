#!/usr/bin/env python3
"""Generate the golden test data of spine-reader: tests/data/fixtures/<case>.{h5,expected.json}.

* The .h5 files are written by SPINE's own writers (v1.4.0, v0.16.3, v0.11.1),
  one subprocess per SPINE version (``spine_worker.py``).
* The expected values come from ``decode_raw.py`` (plain h5py, no SPINE).
* Cross-checks (all asserted):
    - V1 == V2 decode for the same objects (full and lite), except layout metadata;
    - lite == full minus the writer's lite-skipped columns;
    - fixed-only == full restricted to its retained, non-pooled columns;
    - decode_raw == SPINE's own HDF5Reader (the version that wrote the file),
      for every stored field the reader returns; plus a report on the fields the
      reader re-derives when it rebuilds the classes.

Run it under the LCG view (needs h5py, numpy):

    source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
    python3 tools/fixtures/make_fixtures.py [--spine-repo PATH_OR_URL] [--workdir DIR] [--only CASE ...]

SPINE tags v1.4.0, v0.16.3 and v0.11.1 are cloned into ``--workdir`` (default
``$TMPDIR/spine-reader-fixtures``) the first time.
"""

import argparse
import copy
import json
import math
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import decode_raw  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DEFAULT_OUT = os.path.join(REPO_ROOT, "tests", "data", "fixtures")
DEFAULT_SPINE_REPO = "https://github.com/DeepLearnPhysics/spine"
WORKER = os.path.join(HERE, "spine_worker.py")

# name -> how it is made.  "derive" cases are made from another case's file.
CASES = {
    "v1.4.0_v1": dict(tag="v1.4.0", fv=1),
    "v1.4.0_v2": dict(tag="v1.4.0", fv=2),
    "v1.4.0_v1_lite": dict(tag="v1.4.0", fv=1, lite=True),
    "v1.4.0_v2_lite": dict(tag="v1.4.0", fv=2, lite=True),
    "v2root": dict(tag="v0.16.3", fv=2, products="core", n_events=3),
    "v0.11.1_v1": dict(tag="v0.11.1", fv=1, products="core", n_events=3),
    "v1.4.0_v2_fixed_only": dict(tag="v1.4.0", derive="v1.4.0_v2"),
    "incomplete": dict(tag="v1.4.0", fv=1, products="min", n_events=2, incomplete=True),
    # Not shipped by default (--with-exotic): non-object products (string / list / multi_list
    # kinds).  They are only generated, decoded and cross-checked in a temporary directory, to
    # test decode_raw.py on every product kind.
    "v1.4.0_exotic_v1": dict(tag="v1.4.0", fv=1, exotic=True, ship=False),
    "v1.4.0_exotic_v2": dict(tag="v1.4.0", fv=2, exotic=True, ship=False),
}

# Columns that SPINE's lite mode drops (RecoBase/TruthBase ``lite_skip``): the
# per-voxel index arrays.
LITE_SKIPPED = {"index", "orig_index", "index_adapt", "index_g4"}


def log(*a):
    print(*a, file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------
# SPINE checkouts and subprocesses
# ---------------------------------------------------------------------------
def ensure_clone(tag, repo, workdir):
    path = os.path.join(workdir, f"spine_{tag}")
    if not os.path.isdir(os.path.join(path, "src", "spine")):
        os.makedirs(workdir, exist_ok=True)
        cmd = ["git", "clone", "-q", "--branch", tag]
        if "://" in repo:
            cmd += ["--depth", "1"]
        subprocess.run(cmd + [repo, path], check=True)
    return os.path.join(path, "src")


def run_worker(tag_src, *args):
    env = dict(os.environ)
    env["PYTHONPATH"] = tag_src + os.pathsep + env.get("PYTHONPATH", "")
    env["PYTHONHASHSEED"] = "0"  # V1 'events' member order follows set() order
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    cmd = ["nice", "-n", "10", sys.executable, WORKER, *map(str, args)]
    proc = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"worker failed: {' '.join(cmd)}\n{proc.stderr}")
    return proc.stderr


# ---------------------------------------------------------------------------
# comparisons
# ---------------------------------------------------------------------------
def same(a, b):
    """Exact equality where NaN markers are strings (so plain == works)."""
    return a == b


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


def restrict(doc, keys=None, drop_columns=()):
    d = copy.deepcopy(doc)
    if keys is not None:
        d["products"] = {k: v for k, v in d["products"].items() if k in keys}
    for p in d["products"].values():
        for c in drop_columns:
            p["columns"].pop(c, None)
        if p["kind"] == "objects":
            for ev in p["events"]:
                for row in ev:
                    for c in drop_columns:
                        row.pop(c, None)
    return d



# ---------------------------------------------------------------------------
# content checks: the fixtures must exercise what the brief asks for
# ---------------------------------------------------------------------------
OBJECT_LISTS = ("reco_particles", "reco_interactions", "truth_particles", "truth_interactions")
ENUM_COLUMNS = (("reco_particles", "shape"), ("reco_particles", "pid"), ("reco_particles", "chi2_pid"),
                ("truth_particles", "shape"), ("truth_particles", "pid"),
                ("truth_interactions", "current_type"), ("truth_interactions", "interaction_scheme"))


def check_content(doc, full):
    """Conventions (ids = list positions, references in range, best match first)
    always; coverage of every enum label / special value only when ``full``."""
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

    for prod, col in ENUM_COLUMNS:
        enum = P[prod]["columns"][col]["enum"]
        seen = {r[col] for ev in P[prod]["events"] for r in ev}
        if enum is None or set(enum.values()) - seen:
            problems.append(f"{prod}.{col}: labels never used: "
                            f"{sorted(k for k, v in (enum or {}).items() if v not in seen)}")
    bools, specials, strings, empty_var, full_var = set(), set(), [], False, False

    def walk(v, col):
        nonlocal bools, empty_var, full_var
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
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=DEFAULT_OUT, help="output directory (default: tests/data/fixtures)")
    ap.add_argument("--spine-repo", default=os.environ.get("SPINE_REPO", DEFAULT_SPINE_REPO),
                    help="git URL or path of the SPINE repository (tags are cloned from it)")
    ap.add_argument("--workdir", default=os.environ.get(
        "SPINE_FIXTURE_WORKDIR", os.path.join(tempfile.gettempdir(), "spine-reader-fixtures")),
        help="where the per-tag SPINE clones (spine_<tag>) live / are created")
    ap.add_argument("--libver", default="v110",
                    help="HDF5 file-format bound for the writers (v110 or earliest = h5py default)")
    ap.add_argument("--only", nargs="*", default=None, help="restrict to these cases (cross-checks that need "
                    "other cases are skipped)")
    ap.add_argument("--with-exotic", action="store_true",
                    help="also write the (unshipped) exotic-product cases to --out")
    ap.add_argument("--no-reader-check", action="store_true")
    ap.add_argument("--report", default=None, help="write the reader/derived-field report to this JSON file")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    names = [n for n in CASES if args.only is None or n in args.only]
    scratch = tempfile.mkdtemp(prefix="spine-fixtures-")
    srcs = {}
    docs, paths, notes = {}, {}, {}

    def out_dir(name):
        return args.out if CASES[name].get("ship", True) or args.with_exotic else scratch

    def src(tag):
        if tag not in srcs:
            srcs[tag] = ensure_clone(tag, args.spine_repo, args.workdir)
        return srcs[tag]

    # --- 1. write the .h5 files with SPINE's own writers --------------------
    for name in sorted(names, key=lambda n: "derive" in CASES[n]):
        c = CASES[name]
        path = os.path.join(out_dir(name), name + ".h5")
        paths[name] = path
        if "derive" in c:
            base = CASES[c["derive"]]
            if c["derive"] not in paths:
                paths[c["derive"]] = os.path.join(out_dir(c["derive"]), c["derive"] + ".h5")
            log(f"[{name}] litify(fixed_only) of {c['derive']}")
            notes[name] = run_worker(src(c["tag"]), "litify", "--src", paths[c["derive"]], "--out", path,
                                     "--libver", args.libver)
        else:
            cmd = ["write", "--out", path, "--format-version", c["fv"], "--libver", args.libver,
                   "--products", c.get("products", "all"), "--n-events", c.get("n_events", 5)]
            if c.get("lite"):
                cmd.append("--lite")
            if c.get("incomplete"):
                cmd.append("--incomplete")
            if c.get("exotic"):
                cmd.append("--exotic")
            log(f"[{name}] SPINE {c['tag']} writer, format_version={c['fv']}"
                f"{' lite' if c.get('lite') else ''}{' (not finalized)' if c.get('incomplete') else ''}")
            notes[name] = run_worker(src(c["tag"]), *cmd)

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

    def have(*n):
        return all(x in docs for x in n)

    for name in names:
        c = CASES[name]
        if c.get("exotic") or c.get("products") == "min":
            continue
        full = c.get("products", "all") == "all" and c.get("n_events", 5) == 5 and "derive" not in c
        check(f"{name}: conventions" + (" and coverage (every enum label, bools, nan/inf/-inf, UTF-8, "
              "empty arrays, empty event)" if full else ""), check_content(docs[name], full))
    if have("v1.4.0_v1", "v1.4.0_v2"):
        check("decode(v1.4.0_v1) == decode(v1.4.0_v2)  [only layout/format_version differ]",
              diff_docs(layout_free(docs["v1.4.0_v1"]), layout_free(docs["v1.4.0_v2"])))
    if have("v1.4.0_v1_lite", "v1.4.0_v2_lite"):
        check("decode(v1.4.0_v1_lite) == decode(v1.4.0_v2_lite)",
              diff_docs(layout_free(docs["v1.4.0_v1_lite"]), layout_free(docs["v1.4.0_v2_lite"])))
    if have("v1.4.0_v1", "v1.4.0_v1_lite"):
        full, lite = docs["v1.4.0_v1"], docs["v1.4.0_v1_lite"]
        dropped = {k: sorted(set(p["columns"]) - set(lite["products"][k]["columns"])) for k, p in full["products"].items()}
        dropped = {k: v for k, v in dropped.items() if v}
        problems = diff_docs(restrict(full, drop_columns=LITE_SKIPPED), restrict(lite, drop_columns=LITE_SKIPPED))
        for k, v in dropped.items():
            if not set(v) <= LITE_SKIPPED:
                problems.append(f"{k}: lite dropped unexpected columns {v}")
        check(f"lite == full minus {sorted(LITE_SKIPPED)}  (dropped: "
              f"{ {k: v for k, v in dropped.items()} })", problems)
    if have("v1.4.0_v2", "v1.4.0_v2_fixed_only"):
        full, fo = docs["v1.4.0_v2"], docs["v1.4.0_v2_fixed_only"]
        keep = set(fo["products"])
        pooled = set()  # columns the full file keeps in pools
        with_pools = {}
        import h5py
        with h5py.File(paths["v1.4.0_v2"], "r") as f:
            for k in keep:
                g = f["products"][k]
                if g.attrs["kind"] == "objects":
                    for pn in g["variables"]:
                        with_pools.setdefault(k, set()).update(decode_raw.parse_fields(g["variables"][pn].attrs["fields"]))
        problems = []
        for k in keep:
            pools = with_pools.get(k, set())
            exp = restrict({"products": {k: full["products"][k]}}, drop_columns=pools)["products"][k]
            got = fo["products"][k]
            if exp != got:
                problems.append(f"{k}: fixed-only differs from full minus pooled columns {sorted(pools)}")
        if not keep <= set(full["products"]):
            problems.append("fixed-only has products the full file lacks")
        check("fixed_only == full restricted to {six default keys + index} minus every pooled column", problems)

    # --- 4. SPINE's own reader ------------------------------------------------
    class_report = {}
    if not args.no_reader_check:
        for name in names:
            c = CASES[name]
            # the reader of the writing SPINE first; the current one as a fallback
            # (SPINE v0.11.1's reader cannot resolve its own class_name "ImageMeta3D")
            ok = False
            for tag in dict.fromkeys([c["tag"], "v1.4.0"]):
                with tempfile.TemporaryDirectory() as td:
                    out = os.path.join(td, "reader.json")
                    try:
                        run_worker(src(tag), "read", "--file", paths[name], "--out", out)
                    except RuntimeError as exc:
                        last = str(exc).strip().splitlines()[-1]
                        log(f"note  {name}: SPINE {tag} HDF5Reader cannot read this file: {last}")
                        notes.setdefault("reader_failures", []).append((name, tag, last))
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
                check(f"{name}: expected == SPINE {tag} HDF5Reader (build_classes=False), {n} values", errors)
                class_report[name] = {"reader": tag, "error": rd.get("classes_error"),
                                      "products": {k: {kk: sorted(vv) for kk, vv in v.items()}
                                                   for k, v in rep.items()}}
                ok = True
                break
            if not ok:
                failures.append((f"{name}: no SPINE reader could read it", []))
        if args.report:
            with open(args.report, "w") as fh:
                json.dump(class_report, fh, indent=1, sort_keys=True)
        for name, rep in class_report.items():
            if rep["error"]:
                log(f"note  {name}: reader could not rebuild classes: {rep['error']}")
            for k, v in rep["products"].items():
                for kind, cols in v.items():
                    log(f"note  {name}: {k}: {kind}: {', '.join(cols)}")

    # --- 5. summary -----------------------------------------------------------
    total = 0
    log("\nfile sizes:")
    for name in names:
        h, j = os.path.getsize(paths[name]), os.path.getsize(os.path.join(out_dir(name), name + ".expected.json"))
        shipped = out_dir(name) == args.out
        total += (h + j) if shipped else 0
        log(f"  {name:24s} h5 {h:7d} B ({h / 1024:6.1f} KiB)   json {j:7d} B ({j / 1024:6.1f} KiB)"
            f"{'' if shipped else '   [not shipped]'}")
        for line in notes.get(name, "").splitlines():
            if "not in this SPINE" in line:
                log("      " + line.strip())
    log(f"  total shipped {total} B ({total / 1024:.1f} KiB)")
    import shutil
    shutil.rmtree(scratch, ignore_errors=True)
    if failures:
        raise SystemExit(f"{len(failures)} cross-check(s) failed")


if __name__ == "__main__":
    main()
