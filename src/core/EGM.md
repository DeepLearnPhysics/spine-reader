# core

## Overview

Private columnar core. It presents one SPINE HDF5 file, in any layout, as a list of named
products made of typed columns, with a row range per event. `FileSource::open` detects the
layout (V1 region references, V2 at the file root, or V2 under `/products`). It lists the
products and opens each one lazily through the per-layout factories (`v1_product.cpp`,
`v2_product.cpp`). Every `ProductSource` answers `rows(event)`, `num_rows()` and a batched
`read()` into `detail::ColumnBuffer`s. HDF5 converts types at read time. This header set is
used only by `src/bind/`, `src/reader.cpp` and the tests, and is not installed. The header
comments of `v1_product.cpp` and `v2_product.cpp` describe the on-disk layouts and point to
the SPINE writer that produces them (`src/spine/io/write/hdf5/writer.py`, `region.py` and
`product.py` in the SPINE repository).

## Interface

- `source.hpp` (main contract)
  - Enums `Layout { V1, V2Root, V2Products }` and
    `ProductKind { Objects, Array, String, List, MultiList }`, each with a `to_string`.
  - Structs `FileEnumMember`, `ColumnInfo` (`name`, `type`, `count`, `variable`,
    `enum_members`), `ProductInfo` (`name`, `kind`, `class_name`, `scalar`, `columns`,
    `find()`), `RowRange` (`[begin, end)`, `size()`), `ColumnRequest` (`column`, `type`),
    `OpenOptions` (`allow_incomplete`) and `FileInfo` (`path`, `layout`, `format_version`,
    `spine_version`, `complete`, `num_events`, `cfg`).
  - `class ProductSource` (abstract): `info()`, `rows(event)`, `num_rows()`,
    `read(RowRange, const std::vector<ColumnRequest>&, std::vector<detail::ColumnBuffer>&)`.
  - `class FileSource` (abstract): `static open(path, OpenOptions)`, `info()`,
    `product_names()` (in file order), `has_product()`, `product(name)` (opened on first use
    and owned by the `FileSource`).
  - `std::recursive_mutex& hdf5_mutex()` (in `mutex.cpp`).
- `products.hpp`: `open_v1_product(path, file, name, num_events)` and
  `open_v2_product(path, products_root, name, num_events)`.
- `columns.hpp`: `describe_type(name, file_type)` turns an HDF5 type into a `ColumnInfo`, or
  `nullopt` when it is unreadable. Also `native_type(ScalarType)`,
  `check_request(column, requested)` and `memory_type(column, requested, file_type)`.
- `row_io.hpp`
  - `class RowLayout` (`add`, `row_size`, `empty`, `build`): a packed memory compound.
  - `struct RowSelection`: a hyperslab of rows plus a matching memory space.
  - Functions `read_elements`, `prepare`, `copy_fixed`, `copy_vlen`, `copy_strings`,
    `normalize_bool`, `describe_array_dataset`, `read_array_rows`, `column_index`.
- `guard.hpp`: `template <class Body> guarded(where, body)` runs `body` under an
  `h5::ErrorSilencer` and rethrows `spine::Error` as `"spine-reader: <where>: <message>"`.
- `yaml_list.hpp`: `parse_yaml_list(text)` parses the YAML lists that SPINE writes (block
  style or `[]`, with plain or quoted scalars).
- Implementations: `file_source.cpp` (`FileImpl`, layout detection), `v1_product.cpp`
  (`V1Objects`, `V1Array`, `V1String`, `V1Unsupported`), `v2_product.cpp` (`V2Objects` with
  `Pool`/`PoolSlices`, `V2Array`, `V2String`, `V2Unsupported`), `product_info.cpp`
  (`to_string`, `ProductInfo::find`).

## Invariants

- Locking: `hdf5_mutex()` serialises every HDF5 call made by spine-reader, because the
  experiments' HDF5 builds are not thread-safe. Code in `core/` and `h5/` assumes that the
  caller already holds it. The public entry points in `src/reader.cpp` take the lock;
  `guarded()` does not.
- Entry points that call HDF5 (`FileSource::open`, the product factories, `rows`,
  `num_rows`, `read`) run inside `guarded()`, so their errors name the file and the product.
  `product(name)` throws its own `"spine-reader: <path>: no product ..."`. `info()`,
  `product_names()` and `has_product()` make no HDF5 calls. Nothing prints.
- `FileSource::open` rejects:
  - files without both `/info` and `/events`;
  - files with `/info@format == "cache"` (SPINE cache shards);
  - `format_version` values other than 1 or 2;
  - files with `/info@complete` false, unless `allow_incomplete` is set.
- Layout detection: `V2Products` when `/products` is a group and `format_version` is absent
  or 2. Otherwise `V2Root` when `format_version == 2`, else `V1`. A V1 file may contain a
  product named `products`.
- Product order is the order of the `events` compound members (V1), or the children of the
  products root in creation order, falling back to name order (V2). Only V2 groups with a
  `kind` attribute count as products.
- V1 row location: the region reference of an event must select one contiguous block of whole
  rows. A reference with no selection means zero rows at row 0. A scalar product must have
  exactly one row per event.
- V2 row location: rows come from `event_offsets[e]` and `event_offsets[e + 1]`. A scalar
  product returns `{event, event + 1}` without reading offsets. `locate` throws
  `event_offsets is corrupt` if the start is negative or the end comes before the start. Rows
  past the end of the dataset are rejected by `check_rows` when they are read. Pool and
  string-product offsets are also checked against their data length.
- V2 `objects`: the `_var_offsets_<i>` helper columns are hidden from `ProductInfo::columns`.
  Pool fields come after the fixed columns, pool by pool. Each `_var_offsets_<i>` must have
  `fields.size() + 1` entries and a matching `variables/pool_<i>`.
- For `Objects` products, `read()` makes one compound read for all requested fixed columns
  (and pool offsets), plus one read per pool and element type. `Array` products make one read
  per request.
- Type conversion rules (`check_request`): numbers convert to any number, enums convert to
  integers, `Bool` converts only to `Bool`, and `String` only to `String`. Bool buffers are
  normalised to 0 or 1.
- Enums are read as their base integer type and never through a native enum memory type.
  With non-little-endian files, a native enum type would map values by label and turn
  unlabelled values into -1. SPINE's `{FALSE, TRUE}` enum reports as `Bool`.
- Unreadable member types are left out of `columns` without failing: nested compounds,
  references, opaque types and fixed-length strings.
- `List` and `MultiList` products are listed with their columns, but `rows()`, `num_rows()`
  and `read()` on them throw ("not supported").
- V1 reads that HDF5 fills with VLEN data or strings (compound and string products) zero the
  buffer first and reclaim it with `h5::Reclaim`, even when an exception is thrown. V2
  `fixed` is read as plain fixed-size data, with no `Reclaim`. Opening a V2 `objects` product
  rejects a variable-length (VLEN or variable-length string) member of `fixed` with
  `'fixed' member '<name>' is variable-length; V2 keeps variable-length fields in
  variables/pool_<i>`. Variable-length V2 data comes only from the pools.
- Validation: `open` validates the structure.
  - An unknown V2 `kind`, a pool without `kind` (`string`/`array`) or `fields`, a duplicate
    `pool_<i>`, a pool with no 1-D `_var_offsets_<i>` column, and a variable-length member
    of `fixed` all throw.
  - `rows(e)` and `read(rows)` range-check their arguments.
  - V1 groups are List (`elements`) or MultiList (`index` + `element_<k>`). Anything else
    throws.
  - V2 string products are always scalar, and `multi_list` products have no `event_offsets`.
  - `spine_version` falls back to `/info@version`. `/info` may be an empty dataset in files
    from before SPINE v0.11.0.
- V1 support needs HDF5 built with deprecated symbols (`H5Rget_region`). Otherwise
  `v1_product.cpp` fails with `#error`.

## Side Effects

- Opens SPINE files read-only and keeps the file and dataset handles open for the lifetime of
  the `FileSource`. Each product's handles are cached in `FileImpl::opened_`.
- Reads HDF5 data, and changes HDF5's error-printing setting temporarily (through
  `guarded`/`ErrorSilencer`).
- `hdf5_mutex()` is a process-wide, function-local static `std::recursive_mutex`.

## Verification

- Run:
  `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh && cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
- Main unit tests: `tests/test_core_*.cpp`. `spine_reader_real_files` reads
  `$SPINE_READER_REAL_FILES` and reports Skipped when that variable is unset.
