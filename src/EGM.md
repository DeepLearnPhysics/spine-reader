# src

## Overview

Implementation of the `spine_reader` library (the CMake target `SpineReader::spine_reader`,
built from the glob `src/*.cpp` and `src/*/*.cpp`). The public API lives in
`include/spine/`. The direct sources here implement it on top of four private layers:

- `h5/`: an RAII layer over the HDF5 C API.
- `core/`: layout-independent columnar access to V1 and V2 SPINE files.
- `bind/`: binds generated model classes to product columns by name.
- `model/`: generated field and enum descriptor tables.

Data flows `h5` → `core` → `bind` → `reader.cpp`/`event.cpp`. `Reader` opens files,
binds every object product whose class the model knows, and on `event(i)` builds an immutable
`Event` that holds one `std::vector<Class>` per product.

## Interface

Direct sources:

- `reader.cpp` implements `spine::Reader` (`include/spine/reader.hpp`).
  - Constructors from one path or a list of paths, each with `ReaderOptions` (`products`,
    `allow_incomplete`, `strict`).
  - Queries: `num_events()`, `num_files()`, `event(index)`, `schema_report(file)`, and
    `find(run, subrun, event)`, which returns `std::optional<std::uint64_t>`.
  - Private parts: `Reader::Impl`, `OpenFile`, `BoundProduct`, `open_file`, `wanted`, and
    `Impl::build_run_index`.
- `event.cpp` implements `spine::Event` (`include/spine/event.hpp`).
  - Accessors: `index()`, `file_index()`, `entry()`, `has(product)`, `run_info()`, `meta()`,
    and `find_objects`, which backs `objects<T>(product)`.
  - Relations: `particles_of`, `interaction_of`, and `matches`, which returns `Match<T>`
    with an `overlap`.
- `event_data.hpp` (private) defines `spine::detail::EventData`: `index`, `file`, `entry`,
  and `products`, which maps each key to `{const ClassDescriptor* cls,
  std::shared_ptr<const void> objects}`.
- `descriptor.cpp` defines `spine::to_string(ScalarType)`, `EnumDescriptor::name_of(value)`
  and `ClassDescriptor::find(field_name)` (`include/spine/descriptor.hpp`).
- `schema_report.cpp` defines `spine::SchemaReport::summary()` (`include/spine/schema_report.hpp`).
  Its line after the path names the file's SPINE version, the spine-reader version
  (`spine::version()`) and the model's SPINE version.
- `version.cpp` defines `spine::version()` (`include/spine/version.hpp`). It returns
  `SPINE_READER_VERSION_STRING`, a compile definition that `CMakeLists.txt` sets to
  `${PROJECT_VERSION}`; the file refuses to compile without it.

Subdirectory contracts:

- [bind](./bind/EGM.md): `ProductBinding`, `incompatibility`, `ClassEntry` and `find_class`.
- [core](./core/EGM.md): `FileSource`, `ProductSource`, `ColumnInfo`/`ProductInfo`,
  `hdf5_mutex()`, and the V1/V2 product readers.
- [h5](./h5/EGM.md): the `Handle<Closer>` aliases, `ErrorSilencer`, `check`/`fail`, the open
  and attribute helpers, and `Reclaim`.
- [model](./model/EGM.md): generated `<Class>::descriptor()` tables, `EnumDescriptor`s,
  `interaction_mode_enum` and `interaction_type_enum`, and `SPINE_MODEL_CHECK_FIELD`.

## Invariants

- Locking:
  - Every `Reader` entry point that touches HDF5 takes `core::hdf5_mutex()`: the
    constructors, the destructor, move assignment, `event()` and `find()`.
  - Opened files are closed with the lock held. A constructor failure resets `impl_` before
    the lock is released.
  - Reads are therefore serialised: a `Reader` may be used from several threads, but its
    reads do not run in parallel.
- Only `Objects` products whose `class_name` resolves through `bind::find_class` are read.
  The others are listed in `SchemaReport::skipped` as `"<key>: <kind> product"` or
  `"<key>: no model class <Class>"`.
- `ReaderOptions::products` limits reading to the given keys. An empty list means all keys.
  With `strict`, a product whose binding is not `complete()` throws `spine::Error`, and the
  message includes the report summary.
- Event indices run across files in the order given. `first_event` is a prefix sum of the
  files' `num_events`. `event(index)` throws if `index >= num_events()`.
- `Event` is immutable and cheap to copy (a `shared_ptr<const EventData>`), so it is safe to
  share across threads. `objects<T>(key)` returns a static empty vector when the product is
  absent. It throws if the product holds a class other than `T`, which is checked by
  `ClassDescriptor` pointer identity.
- Relations are resolved by SPINE id within the event:
  - `by_id` tries the list position first, then falls back to a linear scan.
  - Ids that match nothing are skipped.
  - A missing `match_overlaps` entry gives an overlap of NaN.
- `find()` builds its `(run, subrun, event)` index on the first call, from every file's
  `run_info`, and throws if a file lacks `run_info`. When a triple repeats, the first event
  wins.
- `SchemaReport::model_version` is `RecoParticle::descriptor().schema_version`.
- Failures are reported as `spine::Error`. Errors raised while opening or reading a file name
  the file (`"spine-reader: <file>..."`), and the product when one is involved. Two errors name
  no file: an out-of-range `event(i)` and a class mismatch in `objects<T>`, which names only
  the product. `schema_report(i)` with an out-of-range `i` throws `std::out_of_range`. Nothing
  prints.

## Side Effects

- `Reader` opens HDF5 files read-only, and keeps them open until the `Reader` is destroyed or
  moved over.
- `event()` reads the event's rows of every bound product. The first `find()` reads all
  `run_info` rows of every file.
- Calls take and hold the process-wide `core::hdf5_mutex()` while they run.
- `SchemaReport::summary()` only builds a string. Nothing is written to stdout or stderr.

## Verification

- Run:
  `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh && cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
- Main unit tests: `tests/test_reader.cpp`, `tests/test_reader_locking.cpp`,
  `tests/test_golden.cpp`, `tests/test_descriptor.cpp`.
