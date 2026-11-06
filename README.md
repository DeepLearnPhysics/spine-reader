# spine-reader

A C++17 library that reads the HDF5 files written by
[SPINE](https://github.com/DeepLearnPhysics/spine) and hands CAFMakers (SBN, DUNE FD,
DUNE ND) typed SPINE objects. It reads both SPINE HDF5 layouts: V1 (region references,
the default writer up to at least v1.4.0) and V2 (offset-based, `format_version = 2`). It
needs only the HDF5 C library, version 1.12 or newer.

Status: under construction.

## Versions

- A spine-reader release is a git tag `vX.Y.Z` made from `project(spine-reader VERSION X.Y.Z)`
  in `CMakeLists.txt`, which is the only place the number is written. Before pushing a tag, run
  `ci/check_tag_version.sh vX.Y.Z`; CI runs it again on the tag push.
- `spine::version()` (`spine/version.hpp`) and `spine-dump --version` print it. Each file's
  `SchemaReport::summary()` prints it too, next to the SPINE version that wrote the file and the
  SPINE version the model was generated from, for example
  `SPINE 1.4.1, format_version 1 (V1); read by spine-reader 0.1.0 (model from SPINE 1.4.0)`.
- `find_package(SpineReader X.Y)` accepts any X.Y.z (`SameMinorVersion`).
- Between releases, builds of `main` report the last release number; production builds should
  use a tag.
- A published tag is never moved or deleted. A wrong release is followed by a fixed one, and is
  listed here as withdrawn. No release has been withdrawn.

## Layout

| Path | Contents |
| --- | --- |
| `include/spine/` | Public API. `descriptor.hpp` and `object.hpp` define the generated model's building blocks. |
| `include/spine/model/` | Generated SPINE classes (`RecoParticle`, ...). Do not edit by hand. |
| `src/h5/` | Thin RAII layer over the HDF5 C API. Private. |
| `src/core/` | Columnar core: layout detection, V1 and V2 product readers (`source.hpp`). Private. |
| `src/model/` | Generated field tables. Do not edit by hand. |
| `tools/gen/` | Generator: dumps SPINE `FieldMetadata` to `schema/` and writes the model. |
| `tools/fixtures/` | Writes the test files in `tests/data/` with SPINE's own writer, plus expected values. |
| `tools/release/` | Tooling for following SPINE releases: `follow_spine.py` runs one SPINE tag end to end, `classify.py` proposes the version bump, `scan_consumer_fields.py` lists the model fields a CAFMaker build reads ([README](tools/release/README.md)). |
| `tests/` | doctest unit tests (`test_*.cpp`). |

## Build

```bash
source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure
```

## Tests

`ctest --test-dir build` runs:

| ctest entry | What it does |
| --- | --- |
| `spine_reader_tests` | The doctest unit tests (`tests/test_*.cpp`), a few seconds. They hold for any SPINE tag once its fixtures are written: the golden test checks the model's fields against the columns of the current fixtures. |
| `spine_reader_contracts` | What the CAFMakers rely on (`tests/test_contracts.cpp`): the unit and default of each model field a CAFMaker reads, and the enum values they use. A failure means the SPINE change is reviewed with the CAFMaker maintainers before a release. |
| `spine_reader_real_files` | Reads every `*.h5` in `$SPINE_READER_REAL_FILES` (or `-DSPINE_READER_REAL_FILES=dir`). Without a directory it reports **Skipped**, never Passed. |
| `spine_reader_generated_model` | `tools/gen/generate.py --check`: the generated model is what the committed SPINE schema produces. |
| `spine_reader_golden_members` | `tests/golden_members.inc` (the member list that `test_golden.cpp` compares object by object, and that `test_model.cpp` and `test_contracts.cpp` walk) is current. |
| `spine_reader_release_tools` | The unit tests of `tools/release` (`classify.py`, `follow_spine.py`), without SPINE or the network. Registered for Python 3.10 or newer. |
| `spine_dump_output` | `spine-dump` prints what it should for the Reader test files. |

When a CAFMaker starts reading a model field, add its row to the contract table.
`tools/release/scan_consumer_fields.py --contracts tests/test_contracts.cpp`, run on that
CAFMaker's build in its own environment, lists the fields it reads that have no row
([tools/release/EGM.md](tools/release/EGM.md)).

The test files in `tests/data/` come with the scripts that write them: `core/make_edge_cases.py`,
`core/make_audit_cases.py`, `core/make_expected.py`, `reader/make_reader_cases.py` (SPINE's own
writer) and `tools/fixtures/` (golden fixtures). `ci/sl7-ups.sh` builds with the SL7 UPS toolchain
(gcc 12, HDF5 1.12.2); `ci/sanitize.sh` builds with ASan and UBSan (`ci/sanitize.sh thread`: ThreadSanitizer).

CI (`.github/workflows/ci.yml`) builds and runs ctest on every push and pull request, with HDF5
1.14 (LCG_110, Ubuntu 24.04) and HDF5 1.12 (LCG_105, Ubuntu 22.04) from the LCG views on CVMFS.
Both jobs run `ci/github-ci.sh`, which configures with `-DSPINE_READER_REQUIRE_PYTHON=ON`: a missing
Python 3 then fails the configuration instead of silently leaving out the Python-based checks.
The 1.14 job then runs `tools/release/follow_spine.py` on the model's own SPINE tag, which must
come out `identical`; its artifacts are uploaded when it fails.
`ci/EGM.md` shows how to run the same jobs locally.

## Conventions

- C++17 in everything installed; no HDF5 type appears in a public header.
- Every HDF5 call happens with `core::hdf5_mutex()` held.
- Errors are `spine::Error` with the file, product and HDF5 message in the text. Nothing prints.
- Fields are bound by name, never by position. Types are converted by HDF5 at read time.
- Enums are mapped by integer value; the file's labels are only used for reporting.
- Style: 2-space indent, 100 columns, `snake_case` functions, `CamelCase` types.
