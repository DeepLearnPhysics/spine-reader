#!/usr/bin/env python3
"""Writes expected.json: what the sample files hold, decoded with h5py alone (no spine-reader).

The core tests compare spine-reader's columns with these values, for the V1 and the V2 file
of each pair. The script also checks that the two layouts of a pair decode to the same thing.

    source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
    python3 make_expected.py          # run from this directory
"""
import json
import math
import sys

import h5py
import numpy as np

BIG = 1500  # arrays with more elements than this are summarised, not listed


def type_name(dtype):
    if h5py.check_enum_dtype(dtype) is not None:
        members = h5py.check_enum_dtype(dtype)
        if sorted(members) == ["FALSE", "TRUE"]:
            return "bool"
        dtype = dtype.base if hasattr(dtype, "base") else dtype
    dtype = np.dtype(dtype)
    if dtype.kind == "b":
        return "bool"  # h5py hands SPINE's {FALSE, TRUE} enum over as numpy bool
    if dtype.kind == "O" or dtype.kind == "S":
        return "string"
    return {"i1": "int8", "i2": "int16", "i4": "int32", "i8": "int64", "u1": "uint8", "u2": "uint16",
            "u4": "uint32", "u8": "uint64", "f4": "float32", "f8": "float64"}[dtype.kind + str(dtype.itemsize)]


def plain(value):
    """JSON-friendly copy of a numpy value (bytes become str, bools become 0/1)."""
    if isinstance(value, bytes):
        return value.decode("utf-8")
    if isinstance(value, np.ndarray):
        return [plain(v) for v in value.tolist()]
    if isinstance(value, (list, tuple)):
        return [plain(v) for v in value]
    if isinstance(value, (bool, np.bool_)):
        return int(value)
    if isinstance(value, np.generic):
        return plain(value.item())
    if isinstance(value, float) and not math.isfinite(value):
        return "nan" if math.isnan(value) else ("inf" if value > 0 else "-inf")  # JSON has none
    return value


def column_entry(dtype, count, variable, rows_by_event):
    entry = {"type": type_name(dtype), "count": count, "variable": variable}
    members = h5py.check_enum_dtype(dtype)
    if members is not None:
        entry["enum"] = {name: int(value) for name, value in members.items()}
    entry["data"] = plain(rows_by_event)
    return entry


def array_entry(arrays, scalar):
    """arrays: one numpy array (rows, ...) per event."""
    width = int(np.prod(arrays[0].shape[1:])) if arrays[0].ndim > 1 else 1
    entry = {"type": type_name(arrays[0].dtype), "count": width, "variable": False}
    if sum(a.size for a in arrays) <= BIG:
        entry["data"] = plain([a.tolist() for a in arrays])
    else:
        entry["shapes"] = [list(a.shape) for a in arrays]
        entry["sums"] = [float(a.astype(np.float64).sum()) for a in arrays]
        entry["first"] = [plain(a[0]) if len(a) else None for a in arrays]
        entry["last"] = [plain(a[-1]) if len(a) else None for a in arrays]
    return entry


def split_by_event(rows, offsets):
    return [rows[offsets[e]:offsets[e + 1]] for e in range(len(offsets) - 1)]


def v2_group_products(f):
    root = f["products"] if "products" in f else f
    return {name: g for name, g in root.items() if isinstance(g, h5py.Group) and "kind" in g.attrs}


def decode_v2(path):
    out = {}
    with h5py.File(path, "r") as f:
        for name, g in v2_group_products(f).items():
            kind = g.attrs["kind"]
            scalar = bool(g.attrs["scalar"])
            product = {"kind": kind, "scalar": scalar}
            if kind == "objects":
                product["class_name"] = g.attrs["class_name"]
                offsets = g["event_offsets"][:]
                fixed = g["fixed"][:]
                columns = {}
                for col in fixed.dtype.names:
                    if col.startswith("_var_offsets_"):
                        continue
                    dt = fixed.dtype[col]
                    sub = dt.subdtype
                    if sub is not None:
                        columns[col] = column_entry(sub[0], int(np.prod(sub[1])), False, split_by_event(fixed[col], offsets))
                    else:
                        columns[col] = column_entry(dt, 1, False, split_by_event(fixed[col], offsets))
                for pool_name, pool in g["variables"].items():
                    index = int(pool_name.split("_")[1])
                    fields = [line[2:] for line in pool.attrs["fields"].splitlines() if line.startswith("- ")]
                    values = pool["values"][:]
                    bounds = fixed[f"_var_offsets_{index}"]
                    for j, field in enumerate(fields):
                        per_row = [values[b[j]:b[j + 1]] for b in bounds]
                        if pool.attrs["kind"] == "string":
                            per_row = [bytes(v).decode("utf-8") for v in per_row]
                            entry = {"type": "string", "count": 1, "variable": True,
                                     "data": split_by_event(per_row, offsets)}
                        else:
                            entry = column_entry(values.dtype, 0, True, split_by_event(per_row, offsets))
                        columns[field] = entry
                product["rows_per_event"] = [int(n) for n in np.diff(offsets)]
                product["columns"] = columns
            elif kind == "array":
                offsets = g["event_offsets"][:]
                values = g["values"][:]
                product["rows_per_event"] = [int(n) for n in np.diff(offsets)]
                product["columns"] = {"": array_entry(split_by_event(values, offsets), scalar)}
            elif kind == "string":
                offsets = g["event_offsets"][:]
                values = g["values"][:]
                strings = [bytes(values[offsets[e]:offsets[e + 1]]).decode("utf-8") for e in range(len(offsets) - 1)]
                product["rows_per_event"] = [1] * len(strings)
                product["columns"] = {"": {"type": "string", "count": 1, "variable": True, "data": [[s] for s in strings]}}
            else:
                product["unsupported"] = True
            out[name] = product
    return out


def decode_v1(path):
    out = {}
    with h5py.File(path, "r") as f:
        events = f["events"][:]
        for name in events.dtype.names:
            node = f[name]
            scalar = bool(node.attrs["scalar"])
            if isinstance(node, h5py.Group):
                out[name] = {"kind": "list" if "elements" in node else "multi_list", "scalar": scalar, "unsupported": True}
                continue
            per_event = [node[events[name][e]] for e in range(len(events))]
            product = {"scalar": scalar, "rows_per_event": [len(p) for p in per_event]}
            if "class_name" in node.attrs:
                product["kind"] = "objects"
                product["class_name"] = node.attrs["class_name"]
                columns = {}
                for col in node.dtype.names:
                    dt = node.dtype[col]
                    rows = [p[col] for p in per_event]
                    if dt.kind == "O":
                        if h5py.check_string_dtype(dt) is not None:
                            columns[col] = {"type": "string", "count": 1, "variable": True,
                                            "data": [[s.decode("utf-8") if isinstance(s, bytes) else s for s in r] for r in rows]}
                        else:
                            vlen = h5py.check_vlen_dtype(dt)
                            columns[col] = column_entry(vlen, 0, True, [[np.asarray(v) for v in r] for r in rows])
                    elif dt.subdtype is not None:
                        columns[col] = column_entry(dt.subdtype[0], int(np.prod(dt.subdtype[1])), False, rows)
                    else:
                        columns[col] = column_entry(dt, 1, False, rows)
                product["columns"] = columns
            elif h5py.check_string_dtype(node.dtype) is not None:
                product["kind"] = "string"
                product["columns"] = {"": {"type": "string", "count": 1, "variable": True,
                                           "data": [[s.decode("utf-8") if isinstance(s, bytes) else s for s in p] for p in per_event]}}
            else:
                product["kind"] = "array"
                product["columns"] = {"": array_entry(per_event, scalar)}
            out[name] = product
    return out


def same(a, b, where):
    """Asserts that two decoded files agree (arrays by value)."""
    assert json.dumps(a, sort_keys=True) == json.dumps(b, sort_keys=True), f"V1 and V2 differ in {where}"


expected = {}
for key, v1, v2 in [("events", "spine_v1_events.h5", "spine_v2_events.h5"),
                    ("events_lite", "spine_v1_events_lite.h5", "spine_v2_events_lite.h5"),
                    ("exotic", "spine_v1_exotic.h5", "spine_v2_exotic.h5")]:
    a, b = decode_v1(v1), decode_v2(v2)
    for name in sorted(set(a) | set(b)):
        if a.get(name, {}).get("unsupported") or b.get(name, {}).get("unsupported"):
            continue
        # V1 has no events/ordering difference; only the column sets and values must agree
        if name not in a or name not in b:
            print(f"{key}: product {name} only in one layout", file=sys.stderr)
            continue
        ca, cb = a[name]["columns"], b[name]["columns"]
        assert sorted(ca) == sorted(cb), f"{key}/{name}: column sets differ {set(ca) ^ set(cb)}"
        a[name]["columns"] = {c: ca[c] for c in sorted(ca)}
        b[name]["columns"] = {c: cb[c] for c in sorted(cb)}
        same(a[name], b[name], f"{key}/{name}")
    if key != "events_lite":  # a subset of "events": the tests read it against that
        expected[key] = {name: b[name] for name in b}

# The two mini files come from SPINE v1.4.0 and v0.16.3: same objects, different schemas
# (v0.16.3 has no depositions_q_sum, and chi2_pid is a plain integer, not an enum).
expected["mini_v1.4.0"] = decode_v2("mini_v2_v1.4.0.h5")
expected["mini_v0.16.3"] = decode_v2("mini_v2_v0.16.3.h5")

with open("expected.json", "w") as out:
    json.dump(expected, out, sort_keys=True, separators=(",", ":"))
print("wrote expected.json")
