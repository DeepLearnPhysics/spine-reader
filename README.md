# spine-reader

A C++17 library that reads the HDF5 files written by
[SPINE](https://github.com/DeepLearnPhysics/spine) and hands CAFMakers (SBN, DUNE FD,
DUNE ND) typed SPINE objects. It reads both SPINE HDF5 layouts: V1 (region references,
the default writer up to at least v1.4.0) and V2 (offset-based, `format_version = 2`). It
needs only the HDF5 C library, version 1.12 or newer.

Status: under construction.

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
| `spine_reader_tests` | The doctest unit tests (`tests/test_*.cpp`), a few seconds. |
| `spine_reader_real_files` | Reads every `*.h5` in `$SPINE_READER_REAL_FILES` (or `-DSPINE_READER_REAL_FILES=dir`). Without a directory it reports **Skipped**, never Passed. |
| `spine_reader_generated_model` | `tools/gen/generate.py --check`: the generated model is what the committed SPINE schema produces. |
| `spine_reader_golden_members` | `tests/golden_members.inc` (the member list `test_golden.cpp` compares object by object) is current. |
| `spine_dump_output` | `spine-dump` prints what it should for the Reader test files. |

The test files in `tests/data/` come with the scripts that write them: `core/make_edge_cases.py`,
`core/make_audit_cases.py`, `core/make_expected.py`, `reader/make_reader_cases.py` (SPINE's own
writer) and `tools/fixtures/` (golden fixtures). `ci/sl7-ups.sh` builds with the SL7 UPS toolchain
(gcc 12, HDF5 1.12.2); `ci/sanitize.sh` builds with ASan and UBSan (`ci/sanitize.sh thread`: ThreadSanitizer).

## Conventions

- C++17 in everything installed; no HDF5 type appears in a public header.
- Every HDF5 call happens with `core::hdf5_mutex()` held.
- Errors are `spine::Error` with the file, product and HDF5 message in the text. Nothing prints.
- Fields are bound by name, never by position. Types are converted by HDF5 at read time.
- Enums are mapped by integer value; the file's labels are only used for reporting.
- Style: 2-space indent, 100 columns, `snake_case` functions, `CamelCase` types.
