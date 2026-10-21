#!/usr/bin/env python3
"""Dump SPINE's field metadata for the object classes the HDF5 writer stores.

Usage (SPINE and its dependencies must be importable; the LCG view provides numpy and h5py)::

    PYTHONPATH=<spine clone>/src python3 tools/gen/dump_schema.py [--out-dir schema]

The output, ``schema/spine-<version>.json``, is the only input of ``generate.py``. Everything in
it is read from SPINE itself:

* the stored fields of each class, in the order of ``as_dict(lite=False)`` (which is the order
  of the HDF5 compound members written by ``HDF5Writer``), with their ``FieldMetadata``;
* the element type, fixed length or variable length of each field, from the *declared* types
  (annotation or ``FieldMetadata.dtype``), never from a data file;
* the value each field has on a default-constructed object;
* the doc line of each field, parsed from the class docstrings;
* the enums of ``spine.constants.enums`` and the fields they are bound to.

Each field's declared type is then compared with what ``HDF5Writer.get_object_dtype`` produces
for a default-constructed object, and every disagreement is listed under ``type_check``.
"""

import argparse
import ast
import dataclasses
import functools
import inspect
import json
import math
import re
import sys
from enum import IntEnum
from pathlib import Path

# Classes the HDF5 writer stores as object products and that a CAF can need.
#   RecoParticle, TruthParticle, RecoInteraction, TruthInteraction: the SPINE reconstruction.
#   RunInfo, Trigger: event identity and trigger time (the DUNE ND CAFMaker reads both).
#   ImageMeta3D: the voxel/pixel geometry (`Meta` is an alias of it in SPINE; legacy files store
#       the class name "Meta").
#   Flash, CRTHit: measured optical flashes and CRT hits that the reco objects point at.
#   FlashHypothesis: predicted light per interaction (RecoInteraction.flash_hypothesis_ids).
# Not included: RecoFragment/TruthFragment (stored only in full, non-lite output; no CAFMaker reads
# them), Neutrino (merged into TruthInteraction, never stored on its own), ImageMeta2D (the
# detectors served by the CAFMakers are 3D). Add a name here to include another class.
DEFAULT_CLASSES = [
    "RecoParticle",
    "TruthParticle",
    "RecoInteraction",
    "TruthInteraction",
    "RunInfo",
    "Trigger",
    "ImageMeta3D",
    "Flash",
    "CRTHit",
    "FlashHypothesis",
]

CLASS_ROLE = {
    "RecoParticle": "reconstructed particle (main reco product)",
    "TruthParticle": "truth particle matched to the reco output (main truth product)",
    "RecoInteraction": "reconstructed interaction (main reco product)",
    "TruthInteraction": "truth interaction, includes the generator neutrino record",
    "RunInfo": "run, subrun and event number of the event",
    "Trigger": "trigger id, type and times",
    "ImageMeta3D": "voxel image geometry; SPINE's `Meta` is an alias, legacy files say 'Meta'",
    "Flash": "measured optical flash",
    "CRTHit": "measured CRT hit",
    "FlashHypothesis": "predicted optical response of one interaction in one optical volume",
}

# Order of the flag names in the JSON; the C++ FieldFlag enum uses the same set.
FLAG_NAMES = [
    "derived",
    "position",
    "vector",
    "index",
    "categorical",
    "pdg",
    "pdg_minus_one_valid",
    "pointwise",
    "lite_skip",
]

SCALAR_ELEMENT = {int: "int64", float: "float64", bool: "bool", str: "string"}
PYTYPE_NAME = {int: "int", float: "float", bool: "bool", str: "str"}


def element_of_dtype(dtype):
    """ScalarType-style name of a numpy dtype."""
    import numpy as np

    dt = np.dtype(dtype)
    if dt.kind == "b":
        return "bool"
    names = {
        ("i", 1): "int8",
        ("i", 2): "int16",
        ("i", 4): "int32",
        ("i", 8): "int64",
        ("u", 1): "uint8",
        ("u", 2): "uint16",
        ("u", 4): "uint32",
        ("u", 8): "uint64",
        ("f", 4): "float32",
        ("f", 8): "float64",
    }
    try:
        return names[(dt.kind, dt.itemsize)]
    except KeyError:
        raise TypeError(f"unsupported numpy dtype {dt}") from None


# --------------------------------------------------------------------------------------
# Docstrings
# --------------------------------------------------------------------------------------

_ENTRY = re.compile(r"^(\w+)\s*:(.*)$")


def parse_attributes(doc):
    """Map attribute name -> one-line description from a numpy-style `Attributes` section."""
    if not doc:
        return {}
    lines = doc.splitlines()
    try:
        start = next(
            i
            for i in range(len(lines) - 1)
            if lines[i].strip() == "Attributes" and set(lines[i + 1].strip()) == {"-"}
        )
    except StopIteration:
        return {}
    docs, name, text = {}, None, []

    def close():
        if name is not None:
            docs[name] = " ".join(" ".join(text).split())

    for line in lines[start + 2 :]:
        if not line.strip():
            continue
        if not line[0].isspace():
            match = _ENTRY.match(line)
            if match is None:  # next section
                break
            close()
            name, text = match.group(1), []
        else:
            text.append(line.strip())
    close()
    return docs


@functools.lru_cache(maxsize=None)
def raw_attribute_docs(klass):
    """Attribute docs of one class's own docstring, as written in the source.

    SPINE merges the parents' `Attributes` into every subclass docstring at import time, which
    loses which class said what. The unmerged text is recovered from the source file.
    """
    module = sys.modules.get(klass.__module__)
    try:
        tree = ast.parse(Path(inspect.getsourcefile(module)).read_text())
    except (TypeError, OSError):
        return {}
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == klass.__name__:
            return parse_attributes(ast.get_docstring(node))
    return {}


def defining_class(cls, name):
    """First class of the MRO that declares `name` (dataclass annotation or property)."""
    for klass in cls.__mro__:
        if name in inspect.get_annotations(klass) or isinstance(klass.__dict__.get(name), property):
            return klass
    return None


def field_doc(cls, name):
    """(doc, source) for a field: the Attributes entry of the class that declares it, then of
    the classes below it in the MRO, then of any class of the MRO, then the property docstring."""
    mro = list(cls.__mro__)
    owner = defining_class(cls, name)
    order = mro[mro.index(owner) :] + mro[: mro.index(owner)] if owner in mro else mro
    for klass in order:
        entry = raw_attribute_docs(klass).get(name)
        if entry:
            return entry, klass.__name__
    prop = next((k.__dict__[name] for k in mro if isinstance(k.__dict__.get(name), property)), None)
    if prop is not None and prop.fget.__doc__:
        summary = inspect.cleandoc(prop.fget.__doc__).split("\n\n")[0]
        return " ".join(summary.split()), "property"
    return "", None


# --------------------------------------------------------------------------------------
# Defaults
# --------------------------------------------------------------------------------------


def encode_scalar(value, element):
    """JSON form of one default value. Non-finite floats are the strings nan, inf and -inf."""
    if element == "bool":
        return bool(value)
    if element == "string":
        return str(value)
    if element.startswith("float"):
        value = float(value)
        if math.isnan(value):
            return "nan"
        if math.isinf(value):
            return "inf" if value > 0 else "-inf"
        return value
    return int(value)


def encode_default(value, storage, element):
    if storage in ("scalar", "string"):
        return encode_scalar(value, element)
    return [encode_scalar(v, element) for v in value.tolist()]


def same_default(a, b):
    """Equality of two values with NaN == NaN, for comparing defaults."""
    import numpy as np

    try:
        return bool(np.array_equal(np.asarray(a), np.asarray(b), equal_nan=True))
    except TypeError:
        return bool(np.array_equal(np.asarray(a), np.asarray(b)))


# --------------------------------------------------------------------------------------
# Declared types
# --------------------------------------------------------------------------------------


def declared_type(cls, name, meta, derived):
    """(storage, element, length, python_type) from the declaration alone."""
    import numpy as np
    from spine.data.base import DataBase

    pytype = meta.return_type if derived else cls._field_types[name]
    if meta.enum is not None:
        if pytype is not int:
            raise TypeError(f"{cls.__name__}.{name}: enum field must be declared int")
        return "scalar", "int64", None, "int"
    if pytype in SCALAR_ELEMENT:
        return (
            "string" if pytype is str else "scalar",
            SCALAR_ELEMENT[pytype],
            None,
            PYTYPE_NAME[pytype],
        )
    if DataBase._annotation_matches(pytype, np.ndarray):
        if meta.dtype is None:
            raise TypeError(f"{cls.__name__}.{name}: array field without a declared dtype")
        element = element_of_dtype(meta.dtype)
        if meta.length is None:
            return "variable_array", element, None, "np.ndarray"
        return "fixed_array", element, int(meta.length), "np.ndarray"
    raise TypeError(f"{cls.__name__}.{name}: unsupported declared type {pytype!r}")


# --------------------------------------------------------------------------------------
# Writer cross-check
# --------------------------------------------------------------------------------------


def describe_writer_spec(spec):
    """Plain description of one compound member as `HDF5Writer.get_object_dtype` gives it."""
    import h5py
    import numpy as np

    dt = np.dtype(spec[1])
    if len(spec) == 3:
        return {"storage": "fixed_array", "dtype": dt.str, "length": int(spec[2])}
    vlen = h5py.check_dtype(vlen=dt)
    enum = h5py.check_dtype(enum=dt)
    if vlen is str:
        return {"storage": "string", "dtype": "utf8_vlen_string"}
    if vlen is not None:
        return {"storage": "variable_array", "dtype": np.dtype(vlen).str}
    if enum is not None:
        return {"storage": "enum", "dtype": dt.str, "members": dict(enum)}
    return {"storage": "scalar", "dtype": dt.str}


def expected_writer(storage, element, length, enum):
    """What the writer should produce for a declared type."""
    import numpy as np

    if enum is not None:
        return {"storage": "enum", "dtype": np.dtype("int64").str}
    if storage == "string":
        return {"storage": "string", "dtype": "utf8_vlen_string"}
    dtype = np.dtype("bool" if element == "bool" else element).str
    out = {"storage": storage, "dtype": dtype}
    if storage == "fixed_array":
        out["length"] = length
    return out


def compare_writer(expected, actual, enum_members):
    problems = []
    for key in ("storage", "dtype", "length"):
        if expected.get(key) != actual.get(key):
            problems.append(f"{key}: declared {expected.get(key)!r}, writer {actual.get(key)!r}")
    if enum_members is not None and actual.get("members") != enum_members:
        problems.append(f"enum members: declared {enum_members}, writer {actual.get('members')}")
    return problems


# --------------------------------------------------------------------------------------
# Enums
# --------------------------------------------------------------------------------------


def collect_enums():
    import spine.constants.enums as mod

    enums = {}
    for name in mod.__all__:
        obj = getattr(mod, name, None)
        if not (isinstance(obj, type) and issubclass(obj, IntEnum)):
            continue
        members = [[m.name, int(m.value)] for m in obj]
        aliases = [[n, int(m.value), m.name] for n, m in obj.__members__.items() if n != m.name]
        enums[name] = {
            "module": obj.__module__,
            "doc": " ".join((obj.__doc__ or "").split()),
            "members": members,
            "aliases": aliases,
            "bound_fields": [],
            "scheme_dependent": False,
        }
    return enums


def scheme_tables(enums):
    """Which enum interprets interaction_mode / interaction_type under each scheme.

    Read from `Neutrino.resolve_enum`, not from a table in this script: an enum is the answer for
    a scheme when `resolve_enum` returns a member of that enum for every one of its values.
    """
    import spine.constants.enums as mod
    from spine.data.larcv.neutrino import Neutrino

    scheme_enum = mod.NuInteractionScheme
    candidates = [n for n in enums if n.endswith("NuInteractionType")]
    tables = {}
    for attr in ("interaction_mode", "interaction_type"):
        tables[attr] = {}
        for scheme in scheme_enum:
            if scheme.value < 0:
                continue
            obj = Neutrino(interaction_scheme=int(scheme))
            found = [
                n
                for n in candidates
                if all(
                    type(obj.resolve_enum(attr, int(v))) is getattr(mod, n) for v in getattr(mod, n)
                )
            ]
            tables[attr][scheme.name] = found[0] if len(found) == 1 else None
            if len(found) != 1:
                raise RuntimeError(
                    f"cannot identify the enum of {attr} under {scheme.name}: {found}"
                )
    return tables


# --------------------------------------------------------------------------------------
# Classes
# --------------------------------------------------------------------------------------


def dump_class(cls, writer, enums, mismatches):
    from dataclasses import fields as dc_fields

    obj = cls()
    stored = obj.as_dict(False)
    lite = obj.as_dict(True)
    field_names = [f.name for f in dc_fields(cls)]
    dataclass_fields = {f.name: f for f in dc_fields(cls)}
    writer_spec = writer.get_object_dtype(obj)
    if [s[0] for s in writer_spec] != list(stored):
        raise RuntimeError(f"{cls.__name__}: writer dtype order differs from as_dict order")

    out_fields = []
    for name, spec in zip(stored, writer_spec):
        meta = cls.attr_metadata(name)
        derived = name not in field_names
        if derived and name not in cls._derived_attrs:
            raise RuntimeError(f"{cls.__name__}.{name}: neither a field nor a stored property")
        storage, element, length, pytype = declared_type(cls, name, meta, derived)
        value = getattr(obj, name)
        default = encode_default(value, storage, element)

        if not derived:
            f = dataclass_fields[name]
            raw = f.default_factory() if f.default_factory is not dataclasses.MISSING else f.default
            if not same_default(raw, value):
                mismatches.append(
                    {
                        "class": cls.__name__,
                        "field": name,
                        "what": "default",
                        "detail": f"dataclass default {raw!r} != instance value {value!r}",
                    }
                )

        enum_name = meta.enum.__name__ if meta.enum is not None else None
        enum_members = None
        if meta.enum is not None:
            enum_members = {m.name: int(m.value) for m in meta.enum}
            enums[enum_name]["bound_fields"].append(f"{cls.__name__}.{name}")

        expected = expected_writer(storage, element, length, meta.enum)
        actual = describe_writer_spec(spec)
        for problem in compare_writer(expected, actual, enum_members):
            mismatches.append(
                {"class": cls.__name__, "field": name, "what": "writer dtype", "detail": problem}
            )

        flags = []
        for flag in FLAG_NAMES:
            if flag == "derived":
                on = derived
            else:
                on = bool(getattr(meta, flag))
            if on:
                flags.append(flag)

        doc, doc_source = field_doc(cls, name)
        entry = {
            "name": name,
            "defined_in": getattr(defining_class(cls, name), "__name__", None),
            "storage": storage,
            "element": element,
            "length": length,
            "python_type": pytype,
            "flags": flags,
            "units": meta.units,
            "enum": enum_name,
            "reference": meta.reference,
            "reference_space": meta.reference_space,
            "default": default,
            "default_source": "default_instance" if derived else "dataclass",
            "doc": doc,
            "doc_source": doc_source,
            "writer": actual,
        }
        if enum_members is not None:
            entry["enum_members"] = [[k, v] for k, v in enum_members.items()]
        out_fields.append(entry)

    return {
        "module": cls.__module__,
        "mro": [k.__name__ for k in cls.__mro__ if k is not object],
        "role": CLASS_ROLE.get(cls.__name__, ""),
        "doc": " ".join((raw_attribute_class_summary(cls)).split()),
        "num_fields": len(out_fields),
        "fields": out_fields,
        "skipped_fields": [n for n in field_names if n not in stored],
        "lite_dropped": [n for n in stored if n not in lite],
    }


def raw_attribute_class_summary(cls):
    doc = inspect.cleandoc(cls.__doc__ or "")
    return doc.split("\n\n")[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--spine-src", help="SPINE `src` directory to import (else PYTHONPATH)")
    parser.add_argument("--out-dir", default="schema", help="directory for spine-<version>.json")
    parser.add_argument("--out", help="output file (overrides --out-dir)")
    parser.add_argument("--classes", nargs="*", default=DEFAULT_CLASSES)
    parser.add_argument("--strict", action="store_true", help="exit 1 on any type mismatch")
    args = parser.parse_args()
    if args.spine_src:
        sys.path.insert(0, args.spine_src)

    import spine.data as sd
    from spine.version import __version__

    try:
        from spine.io.write import HDF5Writer
    except ImportError:  # older layout
        from spine.io.core.write import HDF5Writer
    writer = HDF5Writer.__new__(HDF5Writer)
    writer.lite = False

    enums = collect_enums()
    mismatches = []
    classes = {}
    for name in args.classes:
        cls = getattr(sd, name, None)
        if cls is None:
            sys.exit(f"dump_schema: spine.data has no class {name}")
        classes[name] = dump_class(cls, writer, enums, mismatches)

    tables = scheme_tables(enums)
    for attr, table in tables.items():
        for enum_name in set(v for v in table.values() if v):
            enums[enum_name]["scheme_dependent"] = True
            enums[enum_name].setdefault("used_for", [])
            if attr not in enums[enum_name]["used_for"]:
                enums[enum_name]["used_for"].append(attr)

    # interaction_mode and interaction_type carry no FieldMetadata enum, on purpose.
    for cdump in classes.values():
        for entry in cdump["fields"]:
            if entry["name"] in tables:
                entry["scheme_dependent_enum"] = {
                    "scheme_field": "interaction_scheme",
                    "enum_by_scheme": tables[entry["name"]],
                }

    result = {
        "schema_format": 1,
        "spine_version": __version__,
        "generator": "tools/gen/dump_schema.py",
        "notes": [
            "Field order is the order of as_dict(lite=False): the HDF5 compound member order.",
            "Types are the declared ones (annotation or FieldMetadata.dtype), not read from files.",
            "interaction_mode and interaction_type are plain int64 with no enum: SPINE interprets "
            "them under interaction_scheme (see scheme_dependent_enum on those fields and "
            "enums[*].scheme_dependent).",
            "derived fields (SPINE stored_property) take as default the value on a "
            "default-constructed object (default_source = default_instance).",
            "float32/float64 defaults 'nan', 'inf' and '-inf' are strings in this file.",
        ],
        "classes": classes,
        "enums": enums,
        "scheme_tables": tables,
        "type_check": {
            "method": "declared type vs numpy dtype of HDF5Writer.get_object_dtype(cls())",
            "mismatches": mismatches,
        },
    }

    out = Path(args.out) if args.out else Path(args.out_dir) / f"spine-{__version__}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=1) + "\n")
    total = sum(c["num_fields"] for c in classes.values())
    print(
        f"SPINE {__version__}: {len(classes)} classes, {total} fields, {len(enums)} enums -> {out}"
    )
    for name, c in classes.items():
        print(f"  {name:18s} {c['num_fields']:3d} fields")
    print(f"type check: {len(mismatches)} mismatch(es)")
    for m in mismatches:
        print(f"  {m['class']}.{m['field']} [{m['what']}]: {m['detail']}")
    return 1 if (mismatches and args.strict) else 0


if __name__ == "__main__":
    sys.exit(main())
