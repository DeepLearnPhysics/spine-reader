# tools/gen

## Overview

The SPINE object model generator. `dump_schema.py` imports SPINE at a pinned tag
and writes `schema/spine-<version>.json`. `generate.py` reads only that JSON and
writes the C++ model in `include/spine/model/`, `include/spine/model.hpp` and
`src/model/`; nothing in those paths is edited by hand. `check_names.py` scans the
generated code for identifiers that clash with system macros. The hand-written
[README.md](./README.md) is the reference for the regeneration procedure, the JSON
keys and the type mapping.

```text
SPINE clone --dump_schema.py--> schema/spine-<version>.json --generate.py--> C++ model
```

## Interface

### `dump_schema.py` (CLI; needs SPINE, numpy, h5py)

- Flags:
  - `--spine-src DIR` (inserted at `sys.path[0]`; otherwise `PYTHONPATH` is used)
  - `--out-dir` (default `schema`)
  - `--out FILE` (overrides `--out-dir`)
  - `--classes NAME...` (default `DEFAULT_CLASSES`)
  - `--strict`
- `DEFAULT_CLASSES`: RecoParticle, TruthParticle, RecoInteraction,
  TruthInteraction, RunInfo, Trigger, ImageMeta3D, Flash, CRTHit, FlashHypothesis.
- `CLASS_ROLE`, `FLAG_NAMES` (derived, position, vector, index, categorical, pdg,
  pdg_minus_one_valid, pointwise, lite_skip), `SCALAR_ELEMENT`.
- Helpers: `declared_type`, `dump_class`, `collect_enums`, `scheme_tables`,
  `field_doc`, `compare_writer`.
- Output keys: `schema_format` (1), `spine_version`, `generator`, `notes`,
  `classes`, `enums`, `scheme_tables`, `type_check {method, mismatches}`.

### `generate.py` (CLI; standard library only)

- Flags:
  - `--schema FILE` (default: the only `<root>/schema/spine-*.json`)
  - `--root` (default `.`)
  - `--check`
- `generate(schema) -> {relative path: text}`. The paths are:
  - `include/spine/model/common.hpp`
  - `include/spine/model/enums.hpp`
  - `include/spine/model/<snake_class>.hpp`
  - `include/spine/model.hpp`
  - `src/model/enums.cpp`
  - `src/model/field_check.hpp`
  - `src/model/<snake_class>.cpp`

  Each file starts with
  `// GENERATED from SPINE <version> by tools/gen/generate.py. Do not edit.`
- Emitted C++:
  - `enum class X : std::int64_t`, `k<X>Enum` descriptors, `enum_descriptor<E>()`
    and `to_string`.
  - `interaction_mode_enum(NuInteractionScheme)` and
    `interaction_type_enum(NuInteractionScheme)`, built from `scheme_tables`.
  - `struct <Class> : Object<N>` with `enum class Field : std::uint16_t`, members
    with SPINE defaults, `has(Field)` and `descriptor()`.
  - `kFields[]` rows with `detail::assign<&Class::field>`, and one
    `SPINE_MODEL_CHECK_FIELD` per field.
  - `SPINE_READER_MODEL_SPINE_VERSION`, `SPINE_READER_FOR_EACH_CLASS(X)`, and
    `using Shape = ParticleShape;`.
- Tables: `RESERVED` (macro names such as `NULL`, `DOMAIN`, `major`),
  `CXX_KEYWORDS`, `CLASS_MEMBERS`, `ELEMENT_CPP`, `SCALAR_TYPE`, `FLAG_CPP`,
  `ENUM_ALIASES`. Error type: `GenerationError`.

### `check_names.py` (CLI)

- Flags:
  - `--cxx` (default `$CXX`, else `g++`)
  - `--root` (default `.`)
  - `--include HDR` (repeatable)
- `HEADERS` (C, C++ and POSIX headers scanned), `OWN_MACROS` (`SPINE_[A-Z_]+`, `X`
  allowed), `system_macros()`.

## Invariants

- Without `--schema`, `generate.py` requires exactly one
  `<root>/schema/spine-*.json`, otherwise it exits with
  `generate: pass --schema (found N files in schema/)` [generate.py:main]. The
  model describes one SPINE release.
- Output is deterministic: generated files and globs are processed in sorted
  order, so a diff after regeneration is a real change [generate.py:main].
- Stale-file rule: any `.hpp`/`.cpp` directly under `include/spine/model/` or
  `src/model/` that `generate()` does not produce is deleted, or with `--check`
  reported. Other suffixes are left alone [generate.py:main].
- `--check` writes nothing. It prints `stale: <rel>` for each extra, missing or
  different file and returns 1 if there are any, otherwise 0 [generate.py:main].
- `generate.py` raises `GenerationError` (exit `generate: <err>`) for these
  schema problems:
  - an invalid identifier, or one containing `__`;
  - a field named in `RESERVED`, `CXX_KEYWORDS` or `CLASS_MEMBERS`;
  - an enum field that is not scalar int64;
  - a duplicate field, or more than 65535 fields;
  - a snake_case file name collision;
  - an enum member that collides with another after renaming;
  - a missing `NuInteractionScheme`;
  - enum aliases;
  - non-ASCII text in comments or strings.

  [generate.py:check_identifier, FieldInfo, ClassInfo, EnumInfo, generate]
- Enumerators that clash with `RESERVED` or `CXX_KEYWORDS` get a trailing `_`
  (`NULL` -> `NULL_`), while the descriptor keeps the SPINE spelling
  [generate.py:enumerator_name].
- `dump_schema.py` field order is `as_dict(lite=False)` order, which is the HDF5
  compound member order. It raises `RuntimeError` if
  `HDF5Writer.get_object_dtype` disagrees with that order [dump_schema.py:dump_class].
- Types come from declarations (annotations or `FieldMetadata`), never from data
  files. Each declared type is cross-checked against the writer dtype, and
  dataclass defaults are checked against instance values. Mismatches are recorded
  under `type_check.mismatches`. They fail the run (exit 1) only with `--strict`
  [dump_schema.py:declared_type, dump_class, main].
- `dump_schema.py` stops on:
  - an unknown `--classes` name (`dump_schema: spine.data has no class <NAME>`);
  - a field type it cannot map (`TypeError`: an enum not declared `int`, an array
    without a declared dtype, an unsupported type);
  - a stored name that is neither a field nor a stored property (`RuntimeError`);
  - a scheme whose interaction enum cannot be identified uniquely (`RuntimeError`)
  [dump_schema.py:declared_type, dump_class, scheme_tables, main].
- Fields that `as_dict` does not store go to `skipped_fields`; fields dropped by
  `as_dict(lite=True)` go to `lite_dropped` [dump_schema.py:dump_class].
- `check_names.py` exits 1 if any model identifier, after comments and strings are
  stripped, is a macro defined by the scanned headers and is not in `OWN_MACROS`
  [check_names.py:main].

## Side Effects

- `dump_schema.py` imports `spine.data`, `spine.version`, `HDF5Writer` and
  `spine.constants.enums`. It reads SPINE source files (for the docstrings) and
  writes the JSON with `mkdir(parents=True)`. It prints field counts and the
  mismatch list to stdout.
- `generate.py` reads the schema. Unless `--check` is given, it deletes stale
  files and writes only the files whose content changed. It prints
  `SPINE <version>: N files, M written (C classes, E enums)`, where M counts the
  changed files (written or deleted).
- `check_names.py` compiles a temporary `all.cpp` with
  `<cxx> -std=gnu++17 -dM -E -x c++` (`check=True`). It reads
  `include/spine/model*` and `src/model/*`, and prints the clashes and a summary.

## Verification

- Run (ctest registers `generate.py --check --root <repo>` as
  `spine_reader_generated_model`):

  ```bash
  source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
  cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure
  python3 tools/gen/generate.py --check
  python3 tools/gen/check_names.py
  ```
