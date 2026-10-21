#!/usr/bin/env python3
"""Writes the small hand-made files of the core tests: edge cases the SPINE samples lack.

    source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
    python3 make_edge_cases.py        # run from this directory

Files (all tiny):
  edge_not_spine.h5          no /info
  edge_cache.h5              /info@format = "cache" (a SPINE cache shard)
  edge_bad_version.h5        /info@format_version = 3
  edge_incomplete_v1.h5      /info@complete = False, V1
  edge_incomplete_v2.h5      /info@complete = False, V2
  edge_v1_products.h5        V1 (format_version 1) with a product that is called "products"
  edge_attributes.h5         attributes of every kind the h5 layer reads
  edge_v1_columns.h5         V1: enums (negative, unlisted, big-endian), bools, strings, VLEN
  edge_v1_regions.h5         V1: event references that are not one block of rows
  edge_v2_corrupt.h5         V2: offsets that do not match their pool
"""
import h5py
import numpy as np

BOOL = h5py.enum_dtype({"FALSE": 0, "TRUE": 1}, basetype=np.int8)
UTF8 = h5py.string_dtype()
ASCII = h5py.string_dtype("ascii")


def info(f, complete=True, fmt="spine_hdf5", version=None):
    g = f.create_group("info")
    g.attrs.create("cfg", "a: 1\n", dtype=UTF8)
    g.attrs.create("spine_version", "1.4.0", dtype=UTF8)
    g.attrs.create("version", "1.4.0", dtype=UTF8)
    g.attrs.create("format", fmt, dtype=UTF8)
    if version is not None:
        g.attrs.create("format_version", version, dtype=np.int64)
    g.attrs.create("complete", complete, dtype=BOOL)
    return g


def v2_file(path, complete=True, fmt="spine_hdf5", version=2):
    """A V2 file with one scalar array product, `index`, over three events."""
    with h5py.File(path, "w") as f:
        info(f, complete, fmt, version)
        f.create_dataset("events", data=np.arange(3), maxshape=(None,))
        g = f.create_group("products").create_group("index")
        g.attrs.create("kind", "array", dtype=UTF8)
        g.attrs.create("scalar", True, dtype=BOOL)
        g.create_dataset("event_offsets", data=np.arange(4), maxshape=(None,))
        g.create_dataset("values", data=np.array([10, 11, 12]), maxshape=(None,))


def v1_array(f, name, data, scalar=False):
    d = f.create_dataset(name, data=data, maxshape=(None,) + data.shape[1:])
    d.attrs.create("scalar", scalar, dtype=BOOL)
    return d


def v1_file(path, products, complete=True, version=1):
    """products: {name: (dataset, [slice per event])}; writes /events with their references."""
    with h5py.File(path, "w") as f:
        info(f, complete, "spine_hdf5", version)
        datasets = {}
        for name, (maker, _) in products.items():
            datasets[name] = maker(f)
        n = len(next(iter(products.values()))[1])
        dtype = np.dtype([(name, h5py.regionref_dtype) for name in products])
        events = f.create_dataset("events", (n,), maxshape=(None,), dtype=dtype)
        for e in range(n):
            row = np.empty(1, dtype)
            for name, (_, selections) in products.items():
                row[name][0] = datasets[name].regionref[selections[e]]
            events[e] = row[0]


# --- files without a valid SPINE header
with h5py.File("edge_not_spine.h5", "w") as f:
    f.create_dataset("events", data=np.arange(3))
v2_file("edge_cache.h5", fmt="cache")
v2_file("edge_bad_version.h5", version=3)
v2_file("edge_incomplete_v2.h5", complete=False)
v1_file("edge_incomplete_v1.h5", {"index": (lambda f: v1_array(f, "index", np.arange(3), True), [slice(0, 1), slice(1, 2), slice(2, 3)])},
        complete=False)

# --- a V1 file whose product is called "products": not a V2 products group
v1_file("edge_v1_products.h5", {"products": (lambda f: v1_array(f, "products", np.arange(3), True), [slice(0, 1), slice(1, 2), slice(2, 3)])},
        version=1)

# --- attributes
with h5py.File("edge_attributes.h5", "w") as f:
    a = f.attrs
    a.create("vlen_utf8", "héllo µ", dtype=UTF8)
    a.create("vlen_ascii", "plain", dtype=ASCII)
    a.create("fixed_null", np.bytes_(b"abc"), dtype=h5py.string_dtype("ascii", 8))
    a.create("fixed_exact", np.bytes_(b"abcd"), dtype=h5py.string_dtype("ascii", 4))
    a.create("int64", -5, dtype=np.int64)
    a.create("int8", 7, dtype=np.int8)
    a.create("uint8", 200, dtype=np.uint8)
    a.create("bool_true", True, dtype=BOOL)
    a.create("bool_false", False, dtype=BOOL)
    a.create("float", 1.5, dtype=np.float64)
    a.create("two_ints", [1, 2], dtype=np.int64)
    a.create("empty_text", "", dtype=UTF8)
    f.create_dataset("data", data=np.arange(3)).attrs.create("on_dataset", 42, dtype=np.int64)

# --- V1 columns: enums, bools, strings, VLEN
enum_shape = h5py.enum_dtype({"UNKNOWN": -1, "SHOWER": 0, "TRACK": 1, "LARCV_UNKNOWN": 6}, basetype=np.int64)
enum_u8 = h5py.enum_dtype({"A": 0, "B": 200}, basetype=np.uint8)
enum_be = h5py.enum_dtype({"NEG": -2, "POS": 5}, basetype=np.dtype(">i4"))
vlen_i4 = h5py.vlen_dtype(np.int32)
vlen_f8 = h5py.vlen_dtype(np.float64)
edge_dtype = np.dtype([
    ("shape", enum_shape),      # -1, a label, a value that has no label
    ("flag", BOOL),             # 0, 1, and (broken file) 2
    ("plain_flag", np.uint8),   # SPINE before v0.11.0 wrote bools like this
    ("unsigned_enum", enum_u8),
    ("big_endian", enum_be),    # base type not in native order
    ("narrow", np.int32),
    ("wide", np.float64),
    ("triple", np.float32, (3,)),
    ("single", np.int64, (1,)),  # an array of length one is still an array
    ("name", UTF8),
    ("ascii_name", ASCII),
    ("ids", vlen_i4),
    ("weights", vlen_f8),
])
rows = np.zeros(5, dtype=edge_dtype)
rows["shape"] = [-1, 0, 1, 6, 99]
rows["flag"] = [0, 1, 1, 0, 2]
rows["plain_flag"] = [0, 1, 0, 1, 1]
rows["unsigned_enum"] = [0, 200, 200, 0, 7]
rows["big_endian"] = [-2, 5, 5, -2, 3]
rows["narrow"] = [-3, 4, 70000, 0, 2147483647]
rows["wide"] = [0.5, -1.25, 1e300, 0.0, np.pi]
rows["triple"] = [[1, 2, 3], [4, 5, 6], [7, 8, 9], [0, 0, 0], [-1, -2, -3]]
rows["single"] = [[1], [2], [3], [4], [5]]
for i in range(5):
    rows["name"][i] = ["", "pi π", "café", "x", "a longer string with spaces"][i]
    rows["ascii_name"][i] = ["a", "", "ccc", "dd", "e"][i]
    rows["ids"][i] = np.arange(i, dtype=np.int32) * 3
    rows["weights"][i] = np.linspace(0, 1, i * 2, dtype=np.float64)


def edge_objects(f):
    d = f.create_dataset("objs", data=rows, maxshape=(None,))
    d.attrs.create("class_name", "Edge", dtype=UTF8)
    d.attrs.create("scalar", False, dtype=BOOL)
    return d


def edge_labels(f):
    d = f.create_dataset("labels", data=np.array(["zero", "one µ", ""], dtype=object), dtype=UTF8, maxshape=(None,))
    d.attrs.create("scalar", True, dtype=BOOL)
    return d


v1_file("edge_v1_columns.h5", {
    "objs": (edge_objects, [slice(0, 2), slice(2, 2), slice(2, 5)]),
    "labels": (edge_labels, [slice(0, 1), slice(1, 2), slice(2, 3)]),
})

# --- V1 references that are not one block of whole rows
wide = np.arange(20, dtype=np.float32).reshape(10, 2)
v1_file("edge_v1_regions.h5", {
    "ok": (lambda f: v1_array(f, "ok", np.arange(10)), [slice(0, 3), slice(3, 3), slice(3, 10)]),
    "split": (lambda f: v1_array(f, "split", np.arange(10)), [slice(0, 3), [3, 4, 8], slice(5, 10)]),
    "partial_width": (lambda f: v1_array(f, "partial_width", wide), [(slice(0, 3), slice(0, 2)), (slice(3, 5), slice(0, 1)), (slice(5, 10), slice(0, 2))]),
})

# --- V2 offsets that disagree with the data
with h5py.File("edge_v2_corrupt.h5", "w") as f:
    info(f, True, "spine_hdf5", 2)
    f.create_dataset("events", data=np.arange(2), maxshape=(None,))
    products = f.create_group("products")
    # array product whose second event runs past the end of the values
    g = products.create_group("short")
    g.attrs.create("kind", "array", dtype=UTF8)
    g.attrs.create("scalar", False, dtype=BOOL)
    g.create_dataset("event_offsets", data=np.array([0, 2, 5]), maxshape=(None,))
    g.create_dataset("values", data=np.arange(3), maxshape=(None,))
    # objects product whose _var_offsets_0 has the wrong length for its pool
    g = products.create_group("wrong_width")
    g.attrs.create("kind", "objects", dtype=UTF8)
    g.attrs.create("class_name", "Toy", dtype=UTF8)
    g.attrs.create("scalar", False, dtype=BOOL)
    g.create_dataset("event_offsets", data=np.array([0, 1, 1]), maxshape=(None,))
    dt = np.dtype([("id", np.int64), ("_var_offsets_0", np.int64, (4,))])
    g.create_dataset("fixed", data=np.zeros(1, dt), maxshape=(None,))
    p = g.create_group("variables").create_group("pool_0")
    p.attrs.create("kind", "array", dtype=UTF8)
    p.attrs.create("fields", "- a\n- b\n", dtype=UTF8)
    p.create_dataset("values", data=np.zeros(0, np.int32), maxshape=(None,))
    # objects product whose offsets point outside the pool
    g = products.create_group("past_pool")
    g.attrs.create("kind", "objects", dtype=UTF8)
    g.attrs.create("class_name", "Toy", dtype=UTF8)
    g.attrs.create("scalar", False, dtype=BOOL)
    g.create_dataset("event_offsets", data=np.array([0, 1, 1]), maxshape=(None,))
    dt = np.dtype([("id", np.int64), ("_var_offsets_0", np.int64, (2,))])
    row = np.zeros(1, dt)
    row["_var_offsets_0"] = [[0, 9]]
    g.create_dataset("fixed", data=row, maxshape=(None,))
    p = g.create_group("variables").create_group("pool_0")
    p.attrs.create("kind", "array", dtype=UTF8)
    p.attrs.create("fields", "- a\n", dtype=UTF8)
    p.create_dataset("values", data=np.zeros(2, np.int32), maxshape=(None,))
print("wrote the edge-case files")
