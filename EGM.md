# spine-reader Blueprint

## System Overview

spine-reader is a C++17 library (CMake target `SpineReader::spine_reader`; `add_library` without a type, so static unless
`BUILD_SHARED_LIBS` is set) that
reads the HDF5 files written by [SPINE](https://github.com/DeepLearnPhysics/spine) and hands
CAFMakers (SBN, DUNE FD, DUNE ND) typed SPINE objects. It reads both SPINE HDF5 layouts: V1
(region references and VLEN members) and V2 (`format_version = 2`, offset-based, either at the
file root as written by SPINE v0.16 or under `/products` from v0.17). It uses only the HDF5 C
library, version 1.12 or newer. The human-facing introduction is [README.md](./README.md).

The read path is layered:

```
SPINE HDF5 file
  -> src/h5     RAII over the HDF5 C API
  -> src/core   layout detection; each product = named columns + a row range per event
  -> src/bind   model fields bound to columns by name -> SchemaReport
  -> include/spine/model + src/model   classes and field tables generated from SPINE FieldMetadata
  -> include/spine   Reader / Event public API
```

The model is generated at build-maintenance time, not at configure time:
`tools/gen/dump_schema.py` (imports SPINE at a tag) -> `schema/spine-<version>.json` ->
`tools/gen/generate.py` -> `include/spine/model/`, `include/spine/model.hpp`, `src/model/`.

The root folder holds no source files: only `CMakeLists.txt`, `cmake/SpineReaderConfig.cmake.in`,
`schema/spine-<version>.json`, `README.md`, `EGM.md`, `LICENSE`, `.gitignore` and the GitHub Actions
workflow `.github/workflows/ci.yml` (documented in [./ci/EGM.md](./ci/EGM.md)).

Build and packaging (`CMakeLists.txt`):

- Options: `SPINE_READER_BUILD_TESTS` and `SPINE_READER_BUILD_TOOLS` (both default ON for a
  top-level build, OFF as a subproject) and `SPINE_READER_SANITIZE` (`OFF`/`ON`/`thread`). The
  default build type, when none is given, is `RelWithDebInfo`. `tests/CMakeLists.txt` adds
  `SPINE_READER_REQUIRE_PYTHON` (default OFF; CI turns it on): a missing Python 3 then fails the
  configuration instead of leaving out the Python-based checks.
- `cmake --install` installs the library, `spine-dump` (when the tools are built) and the
  headers under `include/spine`. Only headers are installed: the `EGM.md` files are excluded.
- It also exports the CMake package `SpineReader` (`SpineReaderTargets`, version compatibility
  `SameMinorVersion`). Consumers load it with `find_package(SpineReader)` and
  `-DCMAKE_PREFIX_PATH=<prefix>`.
- Version: `project(spine-reader VERSION X.Y.Z)` is the only place the library's version is
  written. It reaches the code as the private compile definition `SPINE_READER_VERSION_STRING`,
  returned by `spine::version()` and printed by `SchemaReport::summary()` and
  `spine-dump --version`. A release is the git tag `vX.Y.Z` made from it
  (`ci/check_tag_version.sh`).

## Subdirectories & System Boundaries

- include: [./include/EGM.md](./include/EGM.md) - Public header root (installed as `include/spine`).
  - [./include/spine/EGM.md](./include/spine/EGM.md) - `Reader`, `ReaderOptions`, `Event`,
    `Match<T>`, `SchemaReport`/`ProductReport`, descriptors, `Object<N>`, `spine::Error`,
    `spine::version()`.
  - [./include/spine/model/EGM.md](./include/spine/model/EGM.md) - Generated SPINE
    classes (`RecoParticle`, `TruthParticle`, `RecoInteraction`, `TruthInteraction`, `RunInfo`,
    `Trigger`, `ImageMeta3D`, `Flash`, `CRTHit`, `FlashHypothesis`) and enums. Do not edit.
  - [./include/spine/detail/EGM.md](./include/spine/detail/EGM.md) - `ColumnBuffer` and
    `assign<&Class::member>`, the bridge from read columns to model members.
- src: [./src/EGM.md](./src/EGM.md) - Implementation of `Reader` and `Event`, descriptor
  helpers, `SchemaReport::summary`, `spine::version()`.
  - [./src/h5/EGM.md](./src/h5/EGM.md) - Move-only HDF5 handles, `ErrorSilencer`,
    `check`/`fail`, read-only open and attribute helpers.
  - [./src/core/EGM.md](./src/core/EGM.md) - `FileSource`/`ProductSource`, V1 and V2 product
    readers, `hdf5_mutex()`.
  - [./src/bind/EGM.md](./src/bind/EGM.md) - `ProductBinding` (name binding, enum checks,
    report) and the class registry `find_class`.
  - [./src/model/EGM.md](./src/model/EGM.md) - Generated field tables and enum descriptors.
    Do not edit.
- tools: [./tools/EGM.md](./tools/EGM.md) - Python developer tools tied to the single
  `schema/spine-*.json`.
  - [./tools/gen/EGM.md](./tools/gen/EGM.md) - Model generator (`dump_schema.py`,
    `generate.py`, `check_names.py`).
  - [./tools/fixtures/EGM.md](./tools/fixtures/EGM.md) - Golden test files written by SPINE's
    own writer (the `current_*` set by any tag, the frozen cases by old releases), plus their
    expected JSON, `manifest.json` and `tests/golden_members.inc`.
  - [./tools/release/EGM.md](./tools/release/EGM.md) - Release tooling:
    `follow_spine.py` (one SPINE tag, end to end: identical, changed, not followed or a
    failure, with `status.json`; `--apply` writes a change into the tree), `classify.py` (the
    change and the proposed bump) and `scan_consumer_fields.py` (the model fields a CAFMaker
    build reads, against the contract table of `tests/test_contracts.cpp`).
- examples: [./examples/EGM.md](./examples/EGM.md) - The `spine-dump` command-line tool.
- ci: [./ci/EGM.md](./ci/EGM.md) - the GitHub Actions workflow and `github-ci.sh` (HDF5 1.14 and
  1.12 from LCG views on CVMFS), `check_tag_version.sh` (release tag against
  `project(VERSION)`), `sanitize.sh` (ASan/UBSan or TSan on EL9) and `sl7-ups.sh` (SL7 container,
  UPS gcc 12 and HDF5 1.12.2).
- tests (not documented by contract): doctest unit tests and the test data under `tests/data/`.

## Global Invariants

- Every HDF5 call runs with the process-wide `spine::core::hdf5_mutex()` held
  (`src/core/mutex.cpp`). Public entry points (`Reader` constructors, destructor, move
  assignment, `event()`, `find()`) take it; code in `src/core/` and `src/h5/` assumes it is held.
- Only the HDF5 C API is used; no HDF5 type appears in an installed header. Installed headers
  are C++17.
- Model fields are bound to file columns by name, never by position. HDF5 converts each column
  to the model's type on read.
- A model field the file lacks keeps its SPINE default, `has(field)` is false, and the field is
  listed in `ProductReport::missing` (or `mismatched`, with a reason). With
  `ReaderOptions::strict` the Reader throws instead.
- Enums are read by integer value. File labels only produce `enum_notes`, except when a file's
  labels cross the model's numbering: the field is then reported as mismatched and not read.
- Failures are reported as `spine::Error`. Errors raised while opening or reading a file name
  the file, and the product when one is involved. Two errors name no file: an out-of-range
  `event(i)` and a class mismatch in `objects<T>`, which names only the product.
  `schema_report(i)` with an out-of-range `i` throws `std::out_of_range`. The library never
  prints.
- `include/spine/model/`, `include/spine/model.hpp` and `src/model/` are generated from the
  one committed `schema/spine-*.json` and must not be edited by hand
  (`generate.py --check` enforces it).
- `Event` is immutable and cheap to copy; relations are resolved by SPINE id within one event.

## Global Side Effects

- Opens SPINE HDF5 files read-only and keeps them open for the `Reader`'s lifetime. Nothing in
  the library writes files.
- Temporarily changes HDF5's library-global automatic error printing (`h5::ErrorSilencer`),
  restoring it afterwards.
- Developer tools only: `tools/gen` rewrites the generated model; `tools/fixtures` clones SPINE
  commits and rewrites `tests/data/fixtures/` (cases and manifest) and `tests/golden_members.inc`;
  `tools/release/scan_consumer_fields.py` runs a CAFMaker's compile commands with
  `-fsyntax-only` in a work folder; `tools/release/follow_spine.py` clones SPINE, and builds
  and tests an export in its artifacts folder. With `--apply`, after a `changed` outcome, it
  writes the new `schema/spine-*.json`, the regenerated model, `tests/golden_members.inc`, the
  current fixtures and the manifest into the working tree, and commits nothing. `ci/` scripts
  create `build-ci/`, `build-asan/`, `build-tsan/` and `build-sl7/`.
- CI: the GitHub Actions workflow builds and tests on GitHub's hosted runners, with read-only
  access to the repository and no secrets.

## System Verification

- Run (EL9, after `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh`):
  `cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
- The ctest entries include `spine_reader_tests` (unit tests), `spine_reader_contracts` (what
  the CAFMakers rely on: `tests/test_contracts.cpp`), `spine_reader_generated_model`
  (`tools/gen/generate.py --check`), `spine_reader_golden_members`,
  `spine_reader_release_tools` (the unit tests of `tools/release`), `spine_dump_output`, and
  `spine_reader_real_files` (skipped unless `SPINE_READER_REAL_FILES` names a directory).
- `spine_reader_tests` excludes the `real files*` and `contract*` test cases, which
  `spine_reader_real_files` and `spine_reader_contracts` run. `spine_reader_generated_model`
  and `spine_reader_golden_members` are registered only when CMake finds Python 3 (with
  `SPINE_READER_REQUIRE_PYTHON=ON`, a missing Python fails the configuration),
  `spine_reader_release_tools` only for Python 3.10 or newer, and `spine_dump_output` only
  when the tools are built.
- CI: `.github/workflows/ci.yml` runs `ci/github-ci.sh` with HDF5 1.14 (LCG_110, Ubuntu 24.04)
  and HDF5 1.12 (LCG_105, Ubuntu 22.04) on every push and pull request. The 1.14 job then runs
  `tools/release/follow_spine.py` on the model's own SPINE tag and requires `identical`.
- Other toolchains: `ci/sl7-ups.sh`, `ci/sanitize.sh`, `ci/sanitize.sh thread`.
