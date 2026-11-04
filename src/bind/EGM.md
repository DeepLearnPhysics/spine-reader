# bind

## Overview

Private glue between the generated object model (`include/spine/model/`, `src/model/`) and
the columnar core (`src/core/`). `ProductBinding` matches the fields of one generated class
to the columns of one product, by name, and records what matched in a `ProductReport`. It then
fills `std::vector<T>` of model objects from the `detail::ColumnBuffer`s the core returns.
`find_class` maps a SPINE class name to the generated class that the Reader instantiates.
These headers are not installed.

## Interface

- `binder.hpp`
  - `std::string spine::bind::incompatibility(const FieldDescriptor&, const core::ColumnInfo&)`:
    returns `""` when the file column can fill the model field. Otherwise it returns a text
    such as `"file float32[3] vs model float32[]"`.
  - `class spine::bind::ProductBinding`
    - `ProductBinding(const ClassDescriptor& cls, core::ProductSource& source)`: binds every
      field of `cls` to a column of `source.info()`, by name.
    - `descriptor()`, `source()`, `report()` (a `ProductReport` with `missing`, `mismatched`,
      `extra`, `enum_notes`), `present(std::size_t field)`.
    - `template <class T> void read(core::RowRange rows, std::vector<T>& out) const`: one
      `ProductSource::read` call, then one object per row through each `FieldDescriptor::assign`.
      It then calls `detail::ObjectAccess::set_present` with the mask of fields found in the file.
- `registry.hpp`
  - `struct spine::bind::ClassEntry { descriptor; read; }`: `read` returns a type-erased
    `std::shared_ptr<const void>` that holds a `std::vector<Class>`.
  - `const ClassEntry* spine::bind::find_class(std::string_view class_name)`: returns `nullptr`
    for unknown classes. The table `kClasses` is built from `SPINE_READER_FOR_EACH_CLASS`
    (`include/spine/model.hpp`).

## Invariants

- Fields are bound by name (`core::ProductInfo::find(field.name)`), never by position.
- Shape compatibility (`incompatibility`): a string field matches only a string column. A
  variable-length field matches only a variable-length column. A fixed field matches only a
  fixed column with the same `count`. Shapes that do not match go to `report.mismatched` and
  the field is not read.
- Enums are read by integer value. The file's labels are compared case-insensitively with the
  model's labels. If two labels cross (`crossed_numbering`: the file numbers the labels one way
  and the model another), the field goes to `mismatched` and is not read. Other label
  differences only add entries to `report.enum_notes`. Both checks run only on fields whose
  shape matched, and `enum_notes` lists bound fields only.
- A model `Bool` field backed by any non-`Bool` column (in SPINE files: the plain integers
  written before v0.11.0) is requested as the column's own type. `assign` converts it with
  `static_cast<bool>`, so any non-zero value is true.
- Model fields that the file lacks keep their default values (`out.assign(rows, T{})`). Their
  `present` bit is cleared.
- `report.extra` lists the file columns that the model class does not have.
- `find_class("Meta")` resolves to `ImageMeta3D` (older SPINE alias).
- A `ProductBinding` holds a raw pointer to its `ProductSource`. The binding must not outlive
  the `core::FileSource` that owns the product.
- `ProductBinding::read` reuses a `mutable` buffer vector, so concurrent calls on one binding
  are not safe. Callers serialise them by holding `core::hdf5_mutex()` (as `Reader` does).

## Side Effects

- `ProductBinding::read` performs HDF5 reads through `core::ProductSource::read`. The caller
  must hold `core::hdf5_mutex()`.
- The constructor only inspects the `ProductInfo` that is already loaded. It does no I/O.
- Nothing prints. Errors from the core arrive as `spine::Error`.

## Verification

- Run:
  `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh && cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
- Main unit tests: `tests/test_binder.cpp`.
