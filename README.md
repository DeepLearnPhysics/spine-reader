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

## Conventions

- C++17 in everything installed; no HDF5 type appears in a public header.
- Every HDF5 call happens with `core::hdf5_mutex()` held.
- Errors are `spine::Error` with the file, product and HDF5 message in the text. Nothing prints.
- Fields are bound by name, never by position. Types are converted by HDF5 at read time.
- Enums are mapped by integer value; the file's labels are only used for reporting.
- Style: 2-space indent, 100 columns, `snake_case` functions, `CamelCase` types.
