#!/usr/bin/env python3
"""Writes the hand-made files added by the test audit: cases the SPINE samples and
make_edge_cases.py do not have. Writes only its own files, so it can be re-run without touching
the others.

    source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
    python3 make_audit_cases.py        # run from this directory

Files (all tiny):
  edge_attributes_padded.h5   a space-padded fixed-length string attribute
  edge_v1_products_group.h5   V1 (format_version 1) whose product "products" is a group
  edge_v2_noversion.h5        V2 with /products but without /info@format_version
  edge_creation_order.h5      V2 whose /products group tracks creation order (zeta, alpha, mid)
  edge_v1_scalar.h5           V1 scalar product with an empty and a two-row event reference
  edge_v2_corrupt2.h5         V2: negative event_offsets, unsorted string / pool offsets
  edge_v2_strings.h5          V2 string products, one flagged scalar and one not
  edge_dtypes_v1.h5           one column of every numeric dtype, enum bools holding 2, in V1
                              (and two products whose variable-length strings were never written;
                              a 2-D member array; members of a kind that is left out)  ...
  edge_dtypes_v2.h5           ... and the same content in V2 (fixed columns and three pools)
  edge_model_binding_v1.h5    V1 "reco_particles" with the awkward columns of the binder
  edge_v2_structure.h5        V2 products whose structure is wrong in one way each, and ones with
                              more than ten pools / list elements (numbered 0..11)
  edge_v1_group_odd.h5        V1 product that is a group but no kind of list, and a multi-list of
                              twelve arrays
  edge_v2_empty.h5            V2 with no events (run_info with no rows)
"""
import h5py
import numpy as np



def small_chunks(rows=8):
    """h5py gives every extendible dataset a chunk of 1 kB to 1 MB, which makes tiny files big.
    Ask for `rows` rows per chunk instead; content, dtypes and attributes are untouched."""
    original = h5py.Group.create_dataset

    def create_dataset(self, name, shape=None, dtype=None, data=None, **kwargs):
        if kwargs.get("chunks") is None and kwargs.get("maxshape") is not None:
            extent = tuple(np.shape(data)) if data is not None and shape is None else shape
            if extent is not None and len(extent) > 0:
                kwargs["chunks"] = (rows,) + tuple(max(d, 1) for d in tuple(extent)[1:])
        return original(self, name, shape, dtype, data, **kwargs)

    h5py.Group.create_dataset = create_dataset


def compact_files(libver="v110"):
    """The default file format bound ('earliest') uses symbol-table groups, 1 to 2 kB each; the
    newer one stores small groups in their object headers. Readers need HDF5 >= 1.10 for it."""
    original = h5py.File.__init__

    def init(self, *args, **kwargs):
        kwargs.setdefault("libver", libver)
        original(self, *args, **kwargs)

    h5py.File.__init__ = init


small_chunks()
compact_files()

BOOL = h5py.enum_dtype({"FALSE": 0, "TRUE": 1}, basetype=np.int8)
UTF8 = h5py.string_dtype()
ASCII = h5py.string_dtype("ascii")


def info(f, complete=True, version=None, with_versions=True):
    g = f.create_group("info")
    g.attrs.create("cfg", "a: 1\n", dtype=UTF8)
    if with_versions:
        g.attrs.create("spine_version", "1.4.0", dtype=UTF8)
    g.attrs.create("format", "spine_hdf5", dtype=UTF8)
    if version is not None:
        g.attrs.create("format_version", version, dtype=np.int64)
    g.attrs.create("complete", complete, dtype=BOOL)
    return g


def v1_file(path, products, version=1):
    """products: {name: (maker(f) -> dataset or group, [selection per event])}."""
    with h5py.File(path, "w") as f:
        info(f, True, version)
        targets = {}
        for name, (maker, _) in products.items():
            targets[name] = maker(f)
        n = len(next(iter(products.values()))[1])
        dtype = np.dtype([(name, h5py.regionref_dtype) for name in products])
        events = f.create_dataset("events", (n,), maxshape=(None,), dtype=dtype)
        for e in range(n):
            row = np.empty(1, dtype)
            for name, (_, selections) in products.items():
                target = targets[name]
                # a group product (a list) is referenced through its `index` dataset
                dataset = target["index"] if isinstance(target, h5py.Group) else target
                row[name][0] = dataset.regionref[selections[e]]
            events[e] = row[0]


def v2_array(products, name, values, offsets, scalar=False):
    g = products.create_group(name)
    g.attrs.create("kind", "array", dtype=UTF8)
    g.attrs.create("scalar", scalar, dtype=BOOL)
    g.create_dataset("event_offsets", data=np.asarray(offsets, np.int64), maxshape=(None,))
    g.create_dataset("values", data=np.asarray(values), maxshape=(None,))
    return g


def v2_string(products, name, text_bytes, offsets):
    g = products.create_group(name)
    g.attrs.create("kind", "string", dtype=UTF8)
    g.attrs.create("scalar", True, dtype=BOOL)
    g.create_dataset("event_offsets", data=np.asarray(offsets, np.int64), maxshape=(None,))
    g.create_dataset("values", data=np.frombuffer(text_bytes, np.uint8), maxshape=(None,))
    return g


def v2_objects(products, name, class_name, fixed, event_offsets, pools=(), scalar=False):
    """pools: [(kind, [field names], values)]; `fixed` must already hold the _var_offsets_i."""
    g = products.create_group(name)
    g.attrs.create("kind", "objects", dtype=UTF8)
    g.attrs.create("class_name", class_name, dtype=UTF8)
    g.attrs.create("scalar", scalar, dtype=BOOL)
    g.create_dataset("event_offsets", data=np.asarray(event_offsets, np.int64), maxshape=(None,))
    g.create_dataset("fixed", data=fixed, maxshape=(None,))
    variables = g.create_group("variables")
    for i, (kind, fields, values) in enumerate(pools):
        p = variables.create_group(f"pool_{i}")
        p.attrs.create("kind", kind, dtype=UTF8)
        p.attrs.create("fields", "".join(f"- {x}\n" for x in fields), dtype=UTF8)
        p.create_dataset("values", data=values, maxshape=(None,))
    return g


def v2_file(path, build, num_events, version=2, with_versions=True):
    with h5py.File(path, "w") as f:
        info(f, True, version, with_versions)
        f.create_dataset("events", data=np.arange(num_events), maxshape=(None,))
        build(f.create_group("products"))


# --- a space-padded fixed-length string attribute (h5py cannot make one without the low level)
with h5py.File("edge_attributes_padded.h5", "w") as f:
    tid = h5py.h5t.C_S1.copy()
    tid.set_size(8)
    tid.set_strpad(h5py.h5t.STR_SPACEPAD)
    space = h5py.h5s.create(h5py.h5s.SCALAR)
    attr = h5py.h5a.create(f.id, b"space_padded", tid, space)
    attr.write(np.array(b"ab c    ", dtype="S8"), mtype=tid)
    attr = h5py.h5a.create(f.id, b"space_padded_blank", tid, space)
    attr.write(np.array(b"        ", dtype="S8"), mtype=tid)

# --- a V1 file whose product "products" is a group (a list), not a dataset
def products_group(f):
    g = f.create_group("products")
    g.attrs.create("scalar", False, dtype=BOOL)
    g.create_dataset("index", (3,), maxshape=(None,), dtype=h5py.regionref_dtype)
    g.create_dataset("elements", data=np.arange(6, dtype=np.int64), maxshape=(None,))
    return g


v1_file("edge_v1_products_group.h5",
        {"products": (products_group, [slice(0, 1), slice(1, 2), slice(2, 3)])})

# --- a V2 file without format_version but with a /products group
v2_file("edge_v2_noversion.h5",
        lambda p: v2_array(p, "index", [10, 11, 12], [0, 1, 2, 3], scalar=True),
        3, version=None, with_versions=False)

# --- a /products group that tracks creation order; the names are not in alphabetical order
with h5py.File("edge_creation_order.h5", "w") as f:
    info(f, True, 2)
    f.create_dataset("events", data=np.arange(2), maxshape=(None,))
    products = f.create_group("products", track_order=True)
    for name in ("zeta", "alpha", "mid"):
        v2_array(products, name, [1, 2], [0, 1, 2])

# --- V1 scalar product: event 1 has an empty selection, event 2 has two rows
def scalar_dataset(f):
    d = f.create_dataset("scal", data=np.arange(3, dtype=np.int64), maxshape=(None,))
    d.attrs.create("scalar", True, dtype=BOOL)
    return d


v1_file("edge_v1_scalar.h5", {"scal": (scalar_dataset, [slice(0, 1), slice(1, 1), slice(1, 3)])})


# --- more corrupt V2 offsets
def corrupt2(p):
    # array: event 1 starts below zero
    v2_array(p, "negative", [1, 2, 3], [0, -3, 2, 3])
    # string: offsets decrease; event 1 runs past the data; event offsets below zero
    v2_string(p, "str_unsorted", b"abcdefgh", [0, 5, 3, 8])
    v2_string(p, "str_past", b"abcd", [0, 2, 99, 99])
    v2_string(p, "str_negative", b"abcd", [-1, 2, 3, 4])
    # objects: the offsets of one object decrease inside a pool
    dt = np.dtype([("id", np.int64), ("_var_offsets_0", np.int64, (3,))])
    values = np.zeros(6, np.int32)
    rows = np.zeros(2, dt)
    rows["_var_offsets_0"] = [[0, 2, 1], [1, 1, 1]]
    v2_objects(p, "pool_unsorted", "Toy", rows, [0, 2, 2, 2], [("array", ["a", "b"], values)])
    # objects: the second object starts before the first object's first offset
    rows = np.zeros(2, dt)
    rows["_var_offsets_0"] = [[2, 3, 4], [1, 4, 5]]
    v2_objects(p, "pool_before_start", "Toy", rows, [0, 2, 2, 2], [("array", ["a", "b"], values)])
    # objects: the first object ends after the last object does
    rows = np.zeros(2, dt)
    rows["_var_offsets_0"] = [[0, 1, 5], [1, 2, 3]]
    v2_objects(p, "pool_beyond_last", "Toy", rows, [0, 2, 2, 2], [("array", ["a", "b"], values)])


v2_file("edge_v2_corrupt2.h5", corrupt2, 3)

# --- V2 products that are malformed in one way each
def structure(p):
    def objects_group(name, class_name="Toy"):
        g = p.create_group(name)
        g.attrs.create("kind", "objects", dtype=UTF8)
        g.attrs.create("class_name", class_name, dtype=UTF8)
        g.attrs.create("scalar", False, dtype=BOOL)
        g.create_dataset("event_offsets", data=np.array([0, 1, 1, 1], np.int64), maxshape=(None,))
        return g

    def pool(g, i, kind="array", fields=("a",), values=None):
        pool_group = g.require_group("variables").create_group(f"pool_{i}")
        pool_group.attrs.create("kind", kind, dtype=UTF8)
        pool_group.attrs.create("fields", "".join(f"- {x}\n" for x in fields), dtype=UTF8)
        if values is None:
            values = np.zeros(2, np.int32)
        pool_group.create_dataset("values", data=values, maxshape=(None,))

    unknown = p.create_group("unknown_kind")
    unknown.attrs.create("kind", "weird", dtype=UTF8)

    g = objects_group("unknown_pool_kind")
    dt = np.dtype([("id", np.int64), ("_var_offsets_0", np.int64, (2,))])
    g.create_dataset("fixed", data=np.zeros(1, dt), maxshape=(None,))
    pool(g, 0, kind="blob")

    g = objects_group("fixed_not_compound")
    g.create_dataset("fixed", data=np.zeros(1, np.int64), maxshape=(None,))

    g = objects_group("orphan_offsets")  # _var_offsets_1, and no pool_1
    dt = np.dtype([("id", np.int64), ("_var_offsets_1", np.int64, (2,))])
    g.create_dataset("fixed", data=np.zeros(1, dt), maxshape=(None,))
    g.create_group("variables")

    g = objects_group("pool_without_offsets")  # pool_0, and no _var_offsets_0
    g.create_dataset("fixed", data=np.zeros(1, np.dtype([("id", np.int64)])), maxshape=(None,))
    pool(g, 0)

    g = objects_group("duplicate_pool")  # pool_0 and pool_00 are the same pool number
    dt = np.dtype([("id", np.int64), ("_var_offsets_0", np.int64, (2,))])
    g.create_dataset("fixed", data=np.zeros(1, dt), maxshape=(None,))
    pool(g, 0)
    pool(g, "00")

    g = objects_group("offsets_not_1d")
    dt = np.dtype([("id", np.int64), ("_var_offsets_0", np.int64, (2, 2))])
    g.create_dataset("fixed", data=np.zeros(1, dt), maxshape=(None,))
    pool(g, 0)

    v2_array(p, "array_of_text", np.array([b"abc", b"def", b"ghi"]), [0, 1, 2, 3])

    # twelve pools, one field each: pool_10 and pool_11 must not be read as pool_0 and pool_1
    n = 12
    dt = np.dtype([("id", np.int64)] + [(f"_var_offsets_{k}", np.int64, (2,)) for k in range(n)])
    rows = np.zeros(1, dt)
    for k in range(n):
        rows[f"_var_offsets_{k}"] = [[0, 1]]
    g = objects_group("many_pools")
    g.create_dataset("fixed", data=rows, maxshape=(None,))
    for k in range(n):
        pool(g, k, fields=(f"f{k}",), values=np.array([100 + k], np.int32))

    # a multi_list of twelve arrays: element_10 comes after element_9, not after element_1
    ml = p.create_group("many_elements")
    ml.attrs.create("kind", "multi_list", dtype=UTF8)
    ml.attrs.create("scalar", False, dtype=BOOL)
    for k in range(n):
        e = ml.create_group(f"element_{k}")
        e.create_dataset("event_offsets", data=np.array([0, 1, 1, 1], np.int64), maxshape=(None,))
        e.create_dataset("values", data=np.full(1, k, np.int32), maxshape=(None,))


v2_file("edge_v2_structure.h5", structure, 3)


def odd_group(f):
    g = f.create_group("odd")
    g.attrs.create("scalar", False, dtype=BOOL)
    g.create_dataset("index", (3,), maxshape=(None,), dtype=h5py.regionref_dtype)
    return g


def many_elements(f):
    g = f.create_group("many_elements")
    g.attrs.create("scalar", False, dtype=BOOL)
    g.create_dataset("index", (3, 12), maxshape=(None, 12), dtype=h5py.regionref_dtype)
    for k in range(12):
        g.create_dataset(f"element_{k}", data=np.full(3, k, np.int32), maxshape=(None,))
    return g


v1_file("edge_v1_group_odd.h5", {"odd": (odd_group, [slice(0, 1), slice(1, 2), slice(2, 3)]),
                                 "many_elements": (many_elements, [(slice(0, 1), slice(0, 12)),
                                                                   (slice(1, 2), slice(0, 12)),
                                                                   (slice(2, 3), slice(0, 12))])})

# --- a V2 file with no events at all
def empty(p):
    dt = np.dtype([("run", np.int64), ("subrun", np.int64), ("event", np.int64)])
    v2_objects(p, "run_info", "RunInfo", np.zeros(0, dt), [0], scalar=True)
    v2_array(p, "index", np.zeros(0, np.int64), [0], scalar=True)


v2_file("edge_v2_empty.h5", empty, 0)

# --- V2 string products: the rows of a string product are events, flagged scalar or not
def strings(p):
    text = "alpha \u00b5".encode("utf-8")  # 2-byte character: 8 bytes
    for name, flag in (("flagged", True), ("unflagged", False)):
        g = v2_string(p, name, text, [0, 5, 5, 8])
        g.attrs.modify("scalar", flag)


v2_file("edge_v2_strings.h5", strings, 3)

# --- every numeric dtype, bools holding 2, variable-length columns of several element types
NUMERIC = [
    ("c_i8", np.int8, [-128, 127, 0, -1]),
    ("c_i16", np.int16, [-32768, 32767, 0, -1]),
    ("c_i32", np.int32, [-2147483648, 2147483647, 0, -1]),
    ("c_i64", np.int64, [-2**63, 2**63 - 1, 0, -1]),
    ("c_u8", np.uint8, [0, 255, 1, 128]),
    ("c_u16", np.uint16, [0, 65535, 1, 32768]),
    ("c_u32", np.uint32, [0, 4294967295, 1, 2147483648]),
    ("c_u64", np.uint64, [0, 2**64 - 1, 1, 2**63]),
    ("c_f32", np.float32, [-0.5, 16777216.0, 0.0, 1.25]),
    ("c_f64", np.float64, [-0.1, 1e300, 0.0, 2.5]),
    # converts to a different value in every memory type: saturation tells them apart
    ("probe", np.int64, [-300, 200, 70000, 5000000000]),
]
FLAG = [2, 0, 1, 3]  # a bool enum holding values other than 0 and 1
V_PROBE = [[-300, 200], [70000], [], [5000000000]]
V_PROBE2 = [[1], [2, 3], [], [4]]
V_FLAG = [[2, 0], [], [3], [1]]
STRINGS = ["alpha", "", "gamma µ", "d"]
EVENT_ROWS = [(0, 2), (2, 2), (2, 4)]  # events: two rows, none, two rows


def offsets_of(columns, nrows):
    """_var_offsets for one pool: columns is [rows-per-field...]; absolute offsets per object."""
    out = np.zeros((nrows, len(columns) + 1), np.int64)
    position = 0
    for r in range(nrows):
        for j, rows in enumerate(columns):
            out[r, j] = position
            position += len(rows[r])
        out[r, len(columns)] = position
    return out


E_I16 = h5py.enum_dtype({"NEG": -2, "POS": 7}, basetype=np.int16)
E_U16 = h5py.enum_dtype({"LOW": 0, "HIGH": 60000}, basetype=np.uint16)
E_U32 = h5py.enum_dtype({"ZERO": 0, "BIG": 4000000000}, basetype=np.uint32)
E_VALUES = {"e_i16": [-2, 7, 7, -2], "e_u16": [0, 60000, 60000, 0],
            "e_u32": [0, 4000000000, 4000000000, 0]}
C_2D = [[[1, 2, 3], [4, 5, 6]], [[7, 8, 9], [10, 11, 12]], [[0, 0, 0], [0, 0, 0]],
        [[-1, -2, -3], [-4, -5, -6]]]
NESTED = np.dtype([("a", np.int32), ("b", np.float32)])
# members spine-reader cannot read and leaves out: a fixed-length string and a nested compound
zoo_fixed_fields = ([(name, dtype) for name, dtype, _ in NUMERIC] +
                    [("flag", BOOL), ("e_i16", E_I16), ("e_u16", E_U16), ("e_u32", E_U32),
                     ("c_2d", np.float32, (2, 3)), ("fixed_text", "S4"), ("nested", NESTED)])


def zoo_rows_v1():
    dt = np.dtype(zoo_fixed_fields + [
        ("vprobe", h5py.vlen_dtype(np.int64)), ("vprobe2", h5py.vlen_dtype(np.int64)),
        ("vflag", h5py.vlen_dtype(BOOL)), ("s", UTF8)])
    rows = np.zeros(4, dt)
    for name, _, values in NUMERIC:
        rows[name] = values
    rows["flag"] = FLAG
    for name, values in E_VALUES.items():
        rows[name] = values
    rows["c_2d"] = C_2D
    rows["fixed_text"] = [b"abcd", b"", b"x", b"yz"]
    for r in range(4):
        rows["vprobe"][r] = np.array(V_PROBE[r], np.int64)
        rows["vprobe2"][r] = np.array(V_PROBE2[r], np.int64)
        rows["vflag"][r] = np.array(V_FLAG[r], np.int8)
        rows["s"][r] = STRINGS[r]
    return rows


def dtypes_v1():
    rows = zoo_rows_v1()

    def zoo(f):
        d = f.create_dataset("zoo", data=rows, maxshape=(None,))
        d.attrs.create("class_name", "Zoo", dtype=UTF8)
        d.attrs.create("scalar", False, dtype=BOOL)
        return d

    def arr_probe(f):
        d = f.create_dataset("arr_probe", data=np.array(NUMERIC[-1][2], np.int64), maxshape=(None,))
        d.attrs.create("scalar", False, dtype=BOOL)
        return d

    def arr_flag(f):
        data = np.array(FLAG, np.int8).astype(BOOL)
        d = f.create_dataset("arr_flag", data=data, maxshape=(None,))
        d.attrs.create("scalar", False, dtype=BOOL)
        return d

    # datasets that were created but never written: their variable-length strings are NULL
    def unset_objects(f):
        dt = np.dtype([("label", UTF8), ("ids", h5py.vlen_dtype(np.int32))])
        d = f.create_dataset("unset_objects", (2,), maxshape=(None,), dtype=dt)
        d.attrs.create("class_name", "Unset", dtype=UTF8)
        d.attrs.create("scalar", False, dtype=BOOL)
        return d

    def unset_labels(f):
        d = f.create_dataset("unset_labels", (2,), maxshape=(None,), dtype=UTF8)
        d.attrs.create("scalar", True, dtype=BOOL)
        return d

    sel = [slice(a, b) for a, b in EVENT_ROWS]
    short = [slice(0, 1), slice(1, 1), slice(1, 2)]
    v1_file("edge_dtypes_v1.h5", {"zoo": (zoo, sel), "arr_probe": (arr_probe, sel),
                                  "arr_flag": (arr_flag, sel),
                                  "unset_objects": (unset_objects, short),
                                  "unset_labels": (unset_labels, short)})


def dtypes_v2():
    dt = np.dtype(zoo_fixed_fields + [("_var_offsets_0", np.int64, (3,)),
                                      ("_var_offsets_1", np.int64, (2,)),
                                      ("_var_offsets_2", np.int64, (2,))])
    fixed = np.zeros(4, dt)
    for name, _, values in NUMERIC:
        fixed[name] = values
    fixed["flag"] = FLAG
    for name, values in E_VALUES.items():
        fixed[name] = values
    fixed["c_2d"] = C_2D
    fixed["fixed_text"] = [b"abcd", b"", b"x", b"yz"]
    fixed["_var_offsets_0"] = offsets_of([V_PROBE, V_PROBE2], 4)
    fixed["_var_offsets_1"] = offsets_of([V_FLAG], 4)
    encoded = [[b for b in s.encode("utf-8")] for s in STRINGS]
    fixed["_var_offsets_2"] = offsets_of([encoded], 4)
    # the pool holds, object after object, the elements of vprobe then those of vprobe2
    pool0 = np.array(sum((V_PROBE[r] + V_PROBE2[r] for r in range(4)), []), np.int64)
    pool1 = np.array(sum(V_FLAG, []), np.int8).astype(BOOL)
    pool2 = np.array(sum(encoded, []), np.uint8)
    pools = [("array", ["vprobe", "vprobe2"], pool0), ("array", ["vflag"], pool1),
             ("string", ["s"], pool2)]
    offsets = [0]
    for a, b in EVENT_ROWS:
        offsets.append(b)

    def build(p):
        v2_objects(p, "zoo", "Zoo", fixed, offsets, pools)
        v2_array(p, "arr_probe", np.array(NUMERIC[-1][2], np.int64), offsets)
        v2_array(p, "arr_flag", np.array(FLAG, np.int8).astype(BOOL), offsets)

    v2_file("edge_dtypes_v2.h5", build, 3)


dtypes_v1()
dtypes_v2()

# --- a "reco_particles" product that binds to RecoParticle with every kind of difference
SHAPE = h5py.enum_dtype({"UNKNOWN": -1, "TRACK": 0, "SHOWER": 1, "NOVEL": 9}, basetype=np.int64)


def binding_v1():
    dt = np.dtype([
        ("id", np.int64),
        ("is_primary", np.uint8),     # a bool stored as a plain integer (SPINE before v0.11.0)
        ("is_valid", BOOL),           # SPINE's bool enum, holding 2 in one row
        ("shape", SHAPE),             # the labels disagree with the model's by value
        ("units", UTF8),
        ("start_point", np.float64, (4,)),   # the model has float32[3]
        ("brand_new", np.float32),           # not in the model
        ("match_ids", h5py.vlen_dtype(np.int32)),
    ])
    rows = np.zeros(3, dt)
    rows["id"] = [3, 4, 5]
    rows["is_primary"] = [2, 0, 1]
    rows["is_valid"] = [1, 0, 2]
    rows["shape"] = [0, 1, 9]
    for r, units in enumerate(["cm", "px", "cm"]):
        rows["units"][r] = units
    rows["start_point"] = [[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12]]
    rows["brand_new"] = [0.5, 1.5, 2.5]
    for r, ids in enumerate([[1, 2], [], [3]]):
        rows["match_ids"][r] = np.array(ids, np.int32)

    def objects(f):
        d = f.create_dataset("reco_particles", data=rows, maxshape=(None,))
        d.attrs.create("class_name", "RecoParticle", dtype=UTF8)
        d.attrs.create("scalar", False, dtype=BOOL)
        return d

    v1_file("edge_model_binding_v1.h5",
            {"reco_particles": (objects, [slice(0, 2), slice(2, 3)])})


binding_v1()
