#!/usr/bin/env python3
"""SPINE-side worker for make_fixtures.py.  Needs a SPINE version on PYTHONPATH.

    PYTHONPATH=<spine>/src python3 spine_worker.py write   --out F.h5 --format-version 1|2 [--lite]
                                                           [--incomplete] [--n-events N] [--exotic]
    PYTHONPATH=<spine>/src python3 spine_worker.py litify  --src A.h5 --out B.h5
    PYTHONPATH=<spine>/src python3 spine_worker.py read    --file F.h5 --out reader.json

``write`` builds the objects of ``content.py`` with the SPINE classes of the
SPINE on the path (dropping fields that version does not have, and reporting
them on stderr) and writes them with that SPINE's own ``HDF5Writer``.

``read`` loads the file back with that SPINE's own ``HDF5Reader`` and dumps what
it returns as JSON (stored values only, ``build_classes=False``), plus, when the
reader can rebuild the classes, the re-derived values of the rebuilt objects.
"""

import argparse
import dataclasses
import inspect
import json
import math
import os
import sys
import warnings

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import content  # noqa: E402  (SPINE-free)

warnings.filterwarnings("ignore")

CORE_KEYS = [
    "run_info",
    "meta",
    "reco_particles",
    "reco_interactions",
    "truth_particles",
    "truth_interactions",
]
KEYS = CORE_KEYS + ["trigger", "flashes", "crthits"]
MIN_KEYS = ["run_info", "reco_particles"]


# --------------------------------------------------------------------------
# SPINE imports that moved between releases
# --------------------------------------------------------------------------
def spine_api():
    import spine.data as sd
    from spine.version import __version__

    try:
        from spine.io.write import HDF5Writer
        from spine.io.read import HDF5Reader
    except ImportError:  # v0.11.x: spine.io.core.*
        from spine.io.core.write import HDF5Writer
        from spine.io.core.read import HDF5Reader
    return sd, __version__, HDF5Writer, HDF5Reader


# --------------------------------------------------------------------------
# content -> SPINE objects
# --------------------------------------------------------------------------
DROPPED = {}  # (class name, field) -> count, reported at the end


def cast(proto, value):
    """Cast a plain Python ``value`` to the type of the class default ``proto``."""
    if isinstance(proto, np.ndarray):
        arr = np.asarray(value, dtype=proto.dtype)
        if proto.ndim == 2:
            arr = arr.reshape(-1, proto.shape[1])
        return arr
    if isinstance(proto, (bool, np.bool_)):
        return bool(value)
    if isinstance(proto, (int, np.integer)):
        return int(value)
    if isinstance(proto, (float, np.floating)):
        return float(value)
    if isinstance(proto, str):
        return str(value)
    return value


def make(cls, spec, **extra):
    """Instantiate ``cls`` with the fields of ``spec`` that this SPINE knows."""
    proto = cls()
    names = {f.name for f in dataclasses.fields(cls)}
    kwargs = {}
    for source in (spec, extra):
        for k, v in source.items():
            if k == "particles":
                continue
            if k not in names:
                DROPPED[(cls.__name__, k)] = DROPPED.get((cls.__name__, k), 0) + 1
                continue
            kwargs[k] = cast(getattr(proto, k), v)
    return cls(**kwargs)


def update(obj, spec, **extra):
    """Set ``spec`` fields on an existing object (same rules as ``make``)."""
    names = {f.name for f in dataclasses.fields(type(obj))}
    proto = type(obj)()
    for source in (spec, extra):
        for k, v in source.items():
            if k == "particles":
                continue
            if k not in names:
                DROPPED[(type(obj).__name__, k)] = DROPPED.get((type(obj).__name__, k), 0) + 1
                continue
            setattr(obj, k, cast(getattr(proto, k), v))


def build_event(sd, ev):
    """Turn one ``content`` event into SPINE objects."""
    out = {
        "run_info": make(sd.RunInfo, ev["run_info"]),
        "meta": make(getattr(sd, "ImageMeta3D", None) or sd.Meta, ev["meta"]),
        "trigger": make(sd.Trigger, ev["trigger"]),
    }
    for side, pcls, icls in (("reco", sd.RecoParticle, sd.RecoInteraction),
                             ("truth", sd.TruthParticle, sd.TruthInteraction)):
        parts_all, ints = [], []
        for iid, it in enumerate(ev[side]):
            parts = []
            for ps in it["particles"]:
                pid = len(parts_all) + len(parts)
                extra = dict(id=pid, interaction_id=iid)
                if side == "reco":
                    extra.update(fragment_ids=[pid], ppn_ids=[pid])
                else:
                    extra.update(nu_id=iid, orig_interaction_id=iid + 3)
                parts.append(make(pcls, ps, **extra))
            inter = icls.from_particles(parts) if parts else icls()
            # attributes shared with the particles (units, index, ...) were set by
            # from_particles; now the interaction's own fields
            update(inter, it, id=iid)
            parts_all += parts
            ints.append(inter)
        out[f"{side}_particles"] = sd.ObjectList(parts_all, pcls())
        out[f"{side}_interactions"] = sd.ObjectList(ints, icls())
    out["flashes"] = sd.ObjectList(
        [make(sd.Flash, f, id=i) for i, f in enumerate(ev["flashes"])], sd.Flash())
    out["crthits"] = sd.ObjectList(
        [make(sd.CRTHit, c, id=i) for i, c in enumerate(ev["crthits"])], sd.CRTHit())
    return out


def exotic_batch(sd, evs):
    """Non-object products for the exotic case (see content.exotic_events)."""
    b = {"index": np.arange(len(evs))}
    b["label"] = [e["label"] for e in evs]
    b["arr_f32_n3"] = [np.asarray(e["arr_f32_n3"], np.float32).reshape(-1, 3) for e in evs]
    same = []
    for e in evs:
        arrs = [np.asarray(a, np.int64) for a in e["lists_same_width"]]
        same.append(sd.ObjectList(arrs, np.empty(0, np.int64)) if not arrs else arrs)
    b["lists_same_width"] = same
    b["lists_mixed_width"] = [
        [np.asarray(e["lists_mixed_width"][0], np.float32).reshape(-1, 3),
         np.asarray(e["lists_mixed_width"][1], np.float32),
         np.asarray(e["lists_mixed_width"][2], np.float32).reshape(-1, 2)]
        for e in evs]
    return b


# --------------------------------------------------------------------------
# write
# --------------------------------------------------------------------------
def apply_libver(libver):
    """Make every ``h5py.File`` the writer opens use the given HDF5 file-format
    bounds.  SPINE does not expose this; the default (``earliest``) makes SPINE's
    one-chunk-per-dataset layout ~25-35% larger (symbol-table groups, v1 B-tree
    chunk indexes).  Logical content, datatypes, attributes, chunk shapes and
    offsets are untouched."""
    if libver in (None, "", "earliest"):
        return
    import h5py

    original = h5py.File.__init__

    def init(self, *a, **kw):
        kw.setdefault("libver", libver)
        original(self, *a, **kw)

    h5py.File.__init__ = init


def cmd_write(args):
    apply_libver(args.libver)
    sd, version, HDF5Writer, _ = spine_api()
    params = inspect.signature(HDF5Writer.__init__).parameters
    cfg = {"fixture": os.path.basename(args.out), "note": "spine-reader golden fixture"}

    kw = dict(file_name=args.out, overwrite=True)
    if args.lite:
        kw["lite"] = True
    if "format_version" in params:
        kw["format_version"] = args.format_version
    elif args.format_version != 1:
        sys.exit(f"SPINE {version} cannot write format_version {args.format_version}")
    keys = {"all": KEYS, "core": CORE_KEYS, "min": MIN_KEYS}[args.products]
    if not args.exotic:
        kw["keys"] = keys
    if os.path.exists(args.out):
        os.remove(args.out)
    writer = HDF5Writer(**kw)

    if args.exotic:
        evs = content.exotic_events()
        batches = [(0, 2), (2, 3)]
        full = exotic_batch(sd, evs)
    else:
        events = [build_event(sd, e) for e in content.build_events()[: args.n_events]]
        batches = [(a, min(b, len(events))) for a, b in content.BATCHES if a < len(events)]
        n = len(events)
        full = {"index": np.arange(n)}
        for key in keys:
            full[key] = [e[key] for e in events]

    for a, b in batches:
        writer({k: v[a:b] for k, v in full.items()}, cfg=cfg)

    if not args.incomplete and hasattr(writer, "finalize"):
        writer.finalize()  # sets info@complete = True
    if hasattr(writer, "close"):
        writer.close()
    del writer

    print(f"SPINE {version}: wrote {args.out} ({os.path.getsize(args.out)} bytes)", file=sys.stderr)
    for (cls, field), n in sorted(DROPPED.items()):
        print(f"  not in this SPINE: {cls}.{field} (x{n})", file=sys.stderr)


def cmd_litify(args):
    apply_libver(args.libver)
    from spine.io.transform import litify_hdf5
    from spine.io.transform.hdf5 import DEFAULT_LITE_KEYS

    litify_hdf5(args.src, args.out, mode="fixed_only", overwrite=True, keys=DEFAULT_LITE_KEYS)
    print(f"litified (fixed_only) {args.src} -> {args.out} ({os.path.getsize(args.out)} bytes)",
          file=sys.stderr)


# --------------------------------------------------------------------------
# read
# --------------------------------------------------------------------------
def jsonify(v):
    if isinstance(v, np.ndarray):
        if v.dtype == object:
            return [jsonify(x) for x in v]
        return [jsonify(x) for x in v.tolist()]
    if isinstance(v, np.generic):
        return jsonify(v.item())
    if isinstance(v, bytes):
        return v.decode("utf-8")
    if isinstance(v, float):
        if math.isnan(v):
            return "nan"
        if math.isinf(v):
            return "inf" if v > 0 else "-inf"
        return v
    if isinstance(v, dict):
        return {k: jsonify(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [jsonify(x) for x in v]
    return v


def cmd_read(args):
    sd, version, _, HDF5Reader = spine_api()
    params = inspect.signature(HDF5Reader.__init__).parameters

    def open_reader(build_classes):
        kw = dict(file_keys=args.file, build_classes=build_classes)
        if "ignore_incomplete" in params:
            kw["ignore_incomplete"] = True
        return HDF5Reader(**kw)

    result = {"spine_version": version}
    # default behaviour: does the reader accept the file without ignore_incomplete?
    try:
        HDF5Reader(file_keys=args.file).close()
        result["strict_open"] = "ok"
    except Exception as exc:  # noqa: BLE001
        result["strict_open"] = f"{type(exc).__name__}: {exc}"
    reader = open_reader(False)
    result["num_entries"] = len(reader)
    result["reader_version"] = reader.version
    result["raw"] = [jsonify(reader.get(i)) for i in range(len(reader))]
    reader.close() if hasattr(reader, "close") else None

    # Rebuild the classes and dump what the classes carry (stored fields are
    # re-derived for the derived properties), for the derived-field report.
    try:
        reader = open_reader(True)
        rebuilt = []
        for i in range(len(reader)):
            ev = reader.get(i)
            d = {}
            for key, val in ev.items():
                objs = val if isinstance(val, list) else [val]
                if objs and hasattr(objs[0], "as_dict"):
                    d[key] = [jsonify(o.as_dict()) for o in objs]
            rebuilt.append(d)
        result["classes"] = rebuilt
    except Exception as exc:  # noqa: BLE001
        result["classes_error"] = f"{type(exc).__name__}: {exc}"
    with open(args.out, "w") as f:
        json.dump(result, f)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    w = sub.add_parser("write")
    w.add_argument("--out", required=True)
    w.add_argument("--format-version", type=int, default=1)
    w.add_argument("--lite", action="store_true")
    w.add_argument("--incomplete", action="store_true", help="skip finalize(): info@complete stays False")
    w.add_argument("--n-events", type=int, default=content.NUM_EVENTS)
    w.add_argument("--exotic", action="store_true")
    w.add_argument("--products", choices=("all", "core", "min"), default="all",
                   help="core = run_info, meta and the four reco/truth object products; "
                        "min = run_info and reco_particles")
    w.add_argument("--libver", default="v110",
                   help="HDF5 file-format bound passed to every h5py.File (earliest = h5py default)")
    w.set_defaults(fn=cmd_write)
    li = sub.add_parser("litify")
    li.add_argument("--src", required=True)
    li.add_argument("--out", required=True)
    li.add_argument("--libver", default="v110")
    li.set_defaults(fn=cmd_litify)
    r = sub.add_parser("read")
    r.add_argument("--file", required=True)
    r.add_argument("--out", required=True)
    r.set_defaults(fn=cmd_read)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
