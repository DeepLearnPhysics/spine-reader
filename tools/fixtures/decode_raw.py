#!/usr/bin/env python3
"""Decode a SPINE HDF5 file with plain h5py, independently of SPINE's reader.

This is the oracle of the spine-reader tests: it follows the documented
on-disk layouts and nothing else (no ``spine`` import).

* V1 (``format_version`` absent or 1): the ``events`` compound holds one HDF5
  dataset-region reference per product; each reference is resolved with the
  low-level ``h5r.get_region`` / ``h5s`` selection API, not with h5py's
  ``dset[ref]`` convenience.
* V2 (``format_version`` 2): ``event_offsets`` (and ``element_offsets``) index
  flat ``values`` / ``fixed`` datasets; object products keep their variable
  fields in ``variables/pool_<i>`` located through the ``_var_offsets_<i>``
  helper columns of ``fixed``.  Product groups live in ``/products`` (SPINE >=
  0.17) or directly at the file root (0.16.x).

Output (see README.md for the full description)::

    {"case", "writer": {spine_version, format_version, lite, complete},
     "layout": "V1" | "V2Root" | "V2Products", "num_events",
     "products": {key: {kind, class_name, scalar, columns, events}}}

Numbers are JSON numbers; NaN/+inf/-inf are the strings "nan", "inf", "-inf";
bools are true/false; enums are their integer values; strings are strings.

    python3 decode_raw.py FILE.h5 [--case NAME] [--lite] [-o OUT.json]
"""

import argparse
import json
import math
import re
import sys

import h5py
import numpy as np

KINDS = ("objects", "array", "string", "list", "multi_list")


class DecodeError(Exception):
    pass


# ---------------------------------------------------------------------------
# value and type description
# ---------------------------------------------------------------------------
def special(x):
    """float -> JSON-safe value (NaN/inf as strings)."""
    if math.isnan(x):
        return "nan"
    if math.isinf(x):
        return "inf" if x > 0 else "-inf"
    return x


def scalar_json(x):
    """One numpy/python scalar -> JSON value."""
    if isinstance(x, (bytes, np.bytes_)):
        return bytes(x).decode("utf-8")
    if isinstance(x, str):
        return x
    if isinstance(x, (np.generic, np.ndarray)):
        x = x.item()
    if isinstance(x, bool):
        return x
    if isinstance(x, int):
        return x
    if isinstance(x, float):
        return special(x)
    raise DecodeError(f"cannot convert {type(x)}")


def list_json(a):
    """1-D array -> list of JSON values (NaN/inf as strings)."""
    return [scalar_json(x) for x in np.asarray(a).tolist()]


def np_type_name(dt):
    dt = np.dtype(dt)
    if dt.kind == "b":
        return "bool"
    if dt.kind in "iuf":
        return dt.name  # int8..int64, uint8..uint64, float32, float64
    raise DecodeError(f"unsupported element dtype {dt}")


def describe_dtype(dt):
    """HDF5 member type -> (column description, kind) with kind in
    scalar | string | fixed | vlen."""
    dt = np.dtype(dt)
    if dt.subdtype is not None:  # H5T_ARRAY
        base, shape = dt.subdtype
        inner, _ = describe_dtype(base)
        n = int(np.prod(shape))
        return dict(type=inner["type"], count=n, variable=False, enum=inner["enum"]), "fixed"
    if h5py.check_string_dtype(dt) is not None:
        return dict(type="string", count=1, variable=False, enum=None), "string"
    vl = h5py.check_vlen_dtype(dt)
    if vl is not None:
        inner, _ = describe_dtype(np.dtype(vl))
        return dict(type=inner["type"], count=0, variable=True, enum=inner["enum"]), "vlen"
    labels = h5py.check_enum_dtype(dt)
    if labels is not None:
        if set(labels) == {"FALSE", "TRUE"} and labels["FALSE"] == 0 and labels["TRUE"] == 1:
            return dict(type="bool", count=1, variable=False, enum=None), "scalar"
        enum = {k: int(v) for k, v in sorted(labels.items(), key=lambda kv: kv[1])}
        return dict(type=np_type_name(dt), count=1, variable=False, enum=enum), "scalar"
    return dict(type=np_type_name(dt), count=1, variable=False, enum=None), "scalar"


def column_values(arr, kind):
    """Column of a structured array (all rows) -> list of JSON values."""
    out = []
    for x in arr:
        if kind == "scalar":
            out.append(scalar_json(x))
        elif kind == "string":
            out.append(scalar_json(x))
        elif kind in ("fixed", "vlen"):
            out.append(list_json(x))
        else:
            raise DecodeError(kind)
    return out


def parse_fields(text):
    """The ``fields`` attribute of a V2 pool: a YAML list of identifiers."""
    if isinstance(text, bytes):
        text = text.decode()
    names = []
    for line in text.splitlines():
        if not line.strip():
            continue
        m = re.fullmatch(r"-\s+([A-Za-z_][A-Za-z0-9_]*)\s*", line)
        if m is None:
            raise DecodeError(f"unexpected pool fields line {line!r}")
        names.append(m.group(1))
    return names


def attr_str(obj, name, default=None):
    if name not in obj.attrs:
        return default
    v = obj.attrs[name]
    if isinstance(v, bytes):
        v = v.decode()
    return str(v)


def attr_bool(obj, name, default=None):
    if name not in obj.attrs:
        return default
    return bool(obj.attrs[name])


def rows_json(a):
    """Rows of an array product: 1-D -> scalars, 2-D -> lists."""
    a = np.asarray(a)
    if a.ndim == 1:
        return [scalar_json(x) for x in a.tolist()]
    return [list_json(r) for r in a]


def array_column(dt, width):
    desc, kind = describe_dtype(dt)
    if kind == "string":
        return dict(type="string", count=1, variable=False, enum=None)
    if kind != "scalar":
        raise DecodeError(f"array product with element type {dt}")
    desc = dict(desc)
    desc["count"] = 1 if width is None else int(width)
    return desc


# ---------------------------------------------------------------------------
# V1
# ---------------------------------------------------------------------------
def region_rows(ref, dset):
    """Row range [a, b) selected by a dataset-region reference (hyperslab of
    whole rows, or an empty selection)."""
    space = h5py.h5r.get_region(ref, dset.id)
    n = space.get_select_npoints()
    if n == 0:
        return 0, 0
    typ = space.get_select_type()
    if typ != h5py.h5s.SEL_HYPERSLABS:
        raise DecodeError(f"{dset.name}: region selection type {typ} is not a hyperslab")
    lo, hi = space.get_select_bounds()
    a, b = int(lo[0]), int(hi[0]) + 1
    rest = int(np.prod([int(h) - int(l) + 1 for l, h in zip(lo[1:], hi[1:])])) if len(lo) > 1 else 1
    if n != (b - a) * rest:
        raise DecodeError(f"{dset.name}: region of {n} points is not a contiguous block of rows")
    if len(lo) > 1 and (any(int(x) != 0 for x in lo[1:]) or
                        [int(h) + 1 for h in hi[1:]] != list(dset.shape[1:])):
        raise DecodeError(f"{dset.name}: region does not span whole rows")
    return a, b


def check_tiling(name, spans, total):
    """Event ranges of a V1 product must tile the dataset in event order."""
    pos = 0
    for e, (a, b) in enumerate(spans):
        if b == a:
            continue
        if a != pos:
            raise DecodeError(f"{name}: event {e} starts at row {a}, expected {pos}")
        pos = b
    if pos != total:
        raise DecodeError(f"{name}: events cover {pos} rows of {total}")


def v1_object_columns(dt):
    cols, kinds = {}, {}
    for name in dt.names:
        cols[name], kinds[name] = describe_dtype(dt.fields[name][0])
    return cols, kinds


def decode_v1(f):
    events = f["events"]
    n = events.shape[0]
    names = events.dtype.names
    if names is None:
        raise DecodeError("V1 'events' is not a compound of references")
    refs = events[:]
    products = {}
    for key in sorted(names):
        node = f[key]
        scalar = attr_bool(node, "scalar", False)
        class_name = attr_str(node, "class_name")
        col_refs = refs[key]
        if isinstance(node, h5py.Dataset):
            spans = [region_rows(r, node) for r in col_refs]
            check_tiling(key, spans, node.shape[0])
            if node.dtype.names:  # objects
                cols, kinds = v1_object_columns(node.dtype)
                data = node[:]
                colv = {c: column_values(data[c], kinds[c]) for c in cols}
                evs = [[{c: colv[c][r] for c in sorted(cols)} for r in range(a, b)] for a, b in spans]
                products[key] = dict(kind="objects", class_name=class_name, scalar=scalar,
                                     columns=dict(sorted(cols.items())), events=evs)
            else:
                width = node.shape[1] if node.ndim == 2 else None
                col = array_column(node.dtype, width)
                data = node[:]
                if col["type"] == "string":
                    rows = [scalar_json(x) for x in data.tolist()]
                else:
                    rows = rows_json(data)
                kind = "string" if col["type"] == "string" else "array"
                products[key] = dict(kind=kind, class_name=class_name, scalar=scalar,
                                     columns={"": col}, events=[rows[a:b] for a, b in spans])
        else:  # group: list or multi_list
            index = node["index"]
            if index.ndim == 1:  # list of same-width arrays
                elements = node["elements"]
                espans = [region_rows(r, index) for r in col_refs]
                check_tiling(key + "/index", espans, index.shape[0])
                eref = index[:]
                width = elements.shape[1] if elements.ndim == 2 else None
                data = elements[:]
                rows = rows_json(data)
                el_spans = [region_rows(r, elements) for r in eref]
                check_tiling(key + "/elements", el_spans, elements.shape[0])
                evs = [[rows[a2:b2] for a2, b2 in el_spans[a:b]] for a, b in espans]
                products[key] = dict(kind="list", class_name=class_name, scalar=scalar,
                                     columns={"": array_column(elements.dtype, width)}, events=evs)
            else:  # index [N, K]: K differently shaped arrays per event
                k = index.shape[1]
                elems = [node[f"element_{j}"] for j in range(k)]
                rowspans = [region_rows(r, index) for r in col_refs]
                check_tiling(key + "/index", rowspans, index.shape[0])
                iref = index[:]
                datas = [rows_json(e[:]) for e in elems]
                spans_j = [[region_rows(iref[r, j], elems[j]) for r in range(index.shape[0])] for j in range(k)]
                for j in range(k):
                    check_tiling(f"{key}/element_{j}", spans_j[j], elems[j].shape[0])
                evs = []
                for a, b in rowspans:
                    if b - a != 1:
                        raise DecodeError(f"{key}: multi_list event spans {b - a} index rows")
                    evs.append([datas[j][spans_j[j][a][0]:spans_j[j][a][1]] for j in range(k)])
                cols = {f"element_{j}": array_column(elems[j].dtype,
                                                      elems[j].shape[1] if elems[j].ndim == 2 else None)
                        for j in range(k)}
                products[key] = dict(kind="multi_list", class_name=class_name, scalar=scalar,
                                     columns=cols, events=evs)
    # every top-level node except events/info must be a product listed in events
    extra = sorted(set(f.keys()) - set(names) - {"events", "info"})
    if extra:
        raise DecodeError(f"top-level nodes not referenced by 'events': {extra}")
    return n, products


# ---------------------------------------------------------------------------
# V2
# ---------------------------------------------------------------------------
def offsets(ds, n_items, name):
    o = ds[:].astype(np.int64)
    if len(o) != n_items + 1 or o[0] != 0 or np.any(np.diff(o) < 0):
        raise DecodeError(f"{name}: bad offsets (len {len(o)}, expected {n_items + 1}): {o.tolist()[:12]}")
    return o


def decode_v2_array(g, n, kind):
    values, eo = g["values"], offsets(g["event_offsets"], n, g.name + "/event_offsets")
    if eo[-1] != values.shape[0]:
        raise DecodeError(f"{g.name}: event_offsets end {eo[-1]} != values {values.shape[0]}")
    data = values[:]
    if kind == "string":
        evs = [[bytes(data[a:b].tobytes()).decode("utf-8")] for a, b in zip(eo[:-1], eo[1:])]
        col = dict(type="string", count=1, variable=False, enum=None)
    else:
        rows = rows_json(data)
        evs = [rows[a:b] for a, b in zip(eo[:-1], eo[1:])]
        col = array_column(values.dtype, values.shape[1] if values.ndim == 2 else None)
    return {"": col}, evs


def decode_v2_objects(g, n):
    fixed = g["fixed"][:]
    eo = offsets(g["event_offsets"], n, g.name + "/event_offsets")
    if eo[-1] != len(fixed):
        raise DecodeError(f"{g.name}: event_offsets end {eo[-1]} != fixed rows {len(fixed)}")
    cols, kinds, pools = {}, {}, {}
    helpers = {}
    for name in fixed.dtype.names:
        m = re.fullmatch(r"_var_offsets_(\d+)", name)
        if m:
            helpers[int(m.group(1))] = name
            continue
        cols[name], kinds[name] = describe_dtype(fixed.dtype.fields[name][0])
    variables = g["variables"]
    for pname in variables:
        i = int(pname.split("_")[-1])
        pool = variables[pname]
        fields = parse_fields(pool.attrs["fields"])
        pkind = attr_str(pool, "kind")
        if pkind not in ("string", "array"):
            raise DecodeError(f"{pool.name}: pool kind {pkind!r}")
        if i not in helpers:
            raise DecodeError(f"{pool.name}: no _var_offsets_{i} column")
        if fixed.dtype.fields[helpers[i]][0].shape != (len(fields) + 1,):
            raise DecodeError(f"{pool.name}: helper shape does not match {len(fields)} fields")
        values = pool["values"][:]
        pools[i] = (pkind, fields, values)
        for fname in fields:
            if fname in cols:
                raise DecodeError(f"{g.name}: column {fname} both fixed and pooled")
            if pkind == "string":
                cols[fname] = dict(type="string", count=1, variable=False, enum=None)
            else:
                cols[fname] = dict(type=np_type_name(values.dtype), count=0, variable=True, enum=None)
    if set(helpers) != set(pools):
        raise DecodeError(f"{g.name}: helper columns {sorted(helpers)} vs pools {sorted(pools)}")

    fixed_cols = {c: column_values(fixed[c], kinds[c]) for c in kinds}
    rows = []
    for r in range(len(fixed)):
        row = {c: fixed_cols[c][r] for c in kinds}
        rows.append(row)
    for i, (pkind, fields, values) in pools.items():
        b = fixed[helpers[i]]
        for r in range(len(fixed)):
            if r + 1 < len(fixed) and b[r][-1] != b[r + 1][0]:
                raise DecodeError(f"{g.name}/pool_{i}: row {r} pool span is not contiguous with row {r + 1}")
            for j, fname in enumerate(fields):
                s, e = int(b[r][j]), int(b[r][j + 1])
                if not (0 <= s <= e <= len(values)):
                    raise DecodeError(f"{g.name}/pool_{i}: row {r} field {fname} span [{s},{e}) out of range")
                chunk = values[s:e]
                rows[r][fname] = chunk.tobytes().decode("utf-8") if pkind == "string" else list_json(chunk)
    rows = [{c: row[c] for c in sorted(row)} for row in rows]
    evs = [rows[a:b] for a, b in zip(eo[:-1], eo[1:])]
    return dict(sorted(cols.items())), evs


def decode_v2_list(g, n):
    values = g["values"]
    eo_el = offsets(g["event_offsets"], n, g.name + "/event_offsets")
    n_el = int(eo_el[-1])
    el = offsets(g["element_offsets"], n_el, g.name + "/element_offsets")
    if el[-1] != values.shape[0]:
        raise DecodeError(f"{g.name}: element_offsets end != values")
    rows = rows_json(values[:])
    evs = [[rows[el[k]:el[k + 1]] for k in range(a, b)] for a, b in zip(eo_el[:-1], eo_el[1:])]
    return {"": array_column(values.dtype, values.shape[1] if values.ndim == 2 else None)}, evs


def decode_v2_multi_list(g, n):
    names = sorted((k for k in g if k.startswith("element_")), key=lambda s: int(s.split("_")[1]))
    cols, per = {}, []
    for name in names:
        sub = g[name]
        values = sub["values"]
        eo = offsets(sub["event_offsets"], n, sub.name + "/event_offsets")
        if eo[-1] != values.shape[0]:
            raise DecodeError(f"{sub.name}: event_offsets end != values")
        rows = rows_json(values[:])
        per.append([rows[a:b] for a, b in zip(eo[:-1], eo[1:])])
        cols[name] = array_column(values.dtype, values.shape[1] if values.ndim == 2 else None)
    evs = [[p[e] for p in per] for e in range(n)]
    return cols, evs


def decode_v2(f):
    n = f["events"].shape[0]
    if "products" in f:
        root, layout = f["products"], "V2Products"
    else:
        root, layout = f, "V2Root"
    products = {}
    for key in sorted(root):
        g = root[key]
        if not isinstance(g, h5py.Group) or "kind" not in g.attrs:
            if layout == "V2Root":
                continue  # events, info, ...
            raise DecodeError(f"/products/{key} is not a product group")
        kind = attr_str(g, "kind")
        if kind not in KINDS:
            raise DecodeError(f"{g.name}: unknown kind {kind!r}")
        scalar = attr_bool(g, "scalar", False)
        class_name = attr_str(g, "class_name")
        if kind == "objects":
            cols, evs = decode_v2_objects(g, n)
        elif kind in ("array", "string"):
            cols, evs = decode_v2_array(g, n, kind)
        elif kind == "list":
            cols, evs = decode_v2_list(g, n)
        else:
            cols, evs = decode_v2_multi_list(g, n)
        products[key] = dict(kind=kind, class_name=class_name, scalar=scalar, columns=cols, events=evs)
    return n, layout, products


# ---------------------------------------------------------------------------
# driver
# ---------------------------------------------------------------------------
def decode(path, case=None, lite=False):
    """Decode ``path``; ``lite`` is writer knowledge (not stored in the file)."""
    with h5py.File(path, "r") as f:
        info = f["info"]
        spine_version = attr_str(info, "spine_version", attr_str(info, "version"))
        fv = int(info.attrs["format_version"]) if "format_version" in info.attrs else 1
        if fv == 1:
            n, products = decode_v1(f)
            layout = "V1"
        elif fv == 2:
            n, layout, products = decode_v2(f)
        else:
            raise DecodeError(f"unsupported format_version {fv}")
        complete = attr_bool(info, "complete")  # None when the attribute is absent
    for key, p in products.items():
        if len(p["events"]) != n:
            raise DecodeError(f"{key}: {len(p['events'])} event entries for {n} events")
    return {
        "case": case,
        "writer": {"spine_version": spine_version, "format_version": fv, "lite": bool(lite),
                   "complete": complete},
        "layout": layout,
        "num_events": n,
        "products": products,
    }


# Compact, diff-friendly JSON: one line per column, one line per object row.
def _c(o):
    return json.dumps(o, separators=(",", ":"), ensure_ascii=False, allow_nan=False)


def dumps(doc):
    out = ["{"]
    out.append(f'  "case": {_c(doc["case"])},')
    out.append(f'  "writer": {_c(doc["writer"])},')
    out.append(f'  "layout": {_c(doc["layout"])},')
    out.append(f'  "num_events": {doc["num_events"]},')
    out.append('  "products": {')
    pkeys = list(doc["products"])
    for pi, key in enumerate(pkeys):
        p = doc["products"][key]
        out.append(f"    {_c(key)}: {{")
        out.append(f'      "kind": {_c(p["kind"])}, "class_name": {_c(p["class_name"])}, "scalar": {_c(p["scalar"])},')
        out.append('      "columns": {')
        ckeys = list(p["columns"])
        for ci, c in enumerate(ckeys):
            out.append(f'        {_c(c)}: {_c(p["columns"][c])}' + ("," if ci + 1 < len(ckeys) else ""))
        out.append("      },")
        out.append('      "events": [')
        for ei, ev in enumerate(p["events"]):
            last = ei + 1 == len(p["events"])
            if p["kind"] == "objects":
                if not ev:
                    out.append("        []" + ("" if last else ","))
                else:
                    out.append("        [")
                    for ri, row in enumerate(ev):
                        out.append(f"          {_c(row)}" + ("," if ri + 1 < len(ev) else ""))
                    out.append("        ]" + ("" if last else ","))
            else:
                out.append(f"        {_c(ev)}" + ("" if last else ","))
        out.append("      ]")
        out.append("    }" + ("," if pi + 1 < len(pkeys) else ""))
    out.append("  }")
    out.append("}")
    return "\n".join(out) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file")
    ap.add_argument("--case", default=None)
    ap.add_argument("--lite", action="store_true", help="record writer.lite = true")
    ap.add_argument("-o", "--output", default=None)
    args = ap.parse_args(argv)
    text = dumps(decode(args.file, args.case, args.lite))
    if args.output:
        with open(args.output, "w", encoding="utf-8") as fh:
            fh.write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
