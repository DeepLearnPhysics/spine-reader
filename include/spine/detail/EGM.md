# spine/detail

## Overview

Internal, header-only plumbing between the HDF5 read path (`src/core`, `src/bind`) and the
generated model classes. It holds one column of converted file data (`ColumnBuffer`) and the
templates that copy one row of such a column into one member of a generated class
(`assign<&Class::member>`). User code should not include these headers directly. The generated
field tables in `src/model/*.cpp` are the only intended users of `assign`.

## Interface

- `assign.hpp` (namespace `spine::detail`)
  - `template <auto Member> void assign(void* object, const ColumnBuffer& column, std::size_t row)`:
    the function stored in `FieldDescriptor::assign`. It casts `object` to the class that owns
    `Member` and forwards to `assign_value`.
  - `template <class M> void assign_value(M& out, const ColumnBuffer& column, std::size_t row)`:
    handles `std::string`, arithmetic/enum scalars, `std::array<E, N>`, and `std::vector<E>`.
  - `template <class T> T load(const ColumnBuffer& column, std::size_t i)`: element `i`
    converted with `static_cast` from the buffer's `ScalarType` to `T`. Enums load through
    their underlying type.
  - Traits: `is_std_array`, `is_std_vector`, `member_pointer_traits`, `dependent_false`.
- `column_buffer.hpp` (namespace `spine::detail`)
  - `struct ColumnBuffer { ScalarType type; bool variable; std::size_t count; std::size_t rows;
    std::vector<unsigned char> data; std::vector<std::uint64_t> offsets;
    std::vector<std::string> strings; const T* elements<T>() const; }`

## Invariants

- Fixed columns: `data` holds `rows * count` elements, row-major. Row `r` starts at element
  `r * count` (`column_buffer.hpp`).
- Variable columns: row `r` spans elements `[offsets[r], offsets[r + 1])`. `offsets` has
  `rows + 1` entries and `offsets[0] == 0`. `assign_value` for `std::vector` uses these offsets
  when `column.variable` is true.
- String columns, fixed or variable in the file, hold one `std::string` per row in `strings`
  and leave `data` and `offsets` empty. `assign_value<std::string>` reads `strings[row]`.
- `data` must already be converted to `type`. `elements<T>()` is a `reinterpret_cast` with no
  checks, so the element type has to match `type` (`Bool` is stored as `uint8_t`).
- `load` throws `spine::Error("spine-reader: a string column cannot be read as a number")` for
  a `ScalarType::String` column.
- `std::array` members copy `min(out.size(), column.count)` elements. Any extra array elements
  keep their default value.
- Any other member type fails to compile (`static_assert`, "unsupported member type in a
  generated class").

## Side Effects

- None outside the target member: `assign` writes only the one member named by `Member`.
  No I/O and no locking. `assign` runs inside `ProductBinding::read` (`src/bind/binder.hpp`),
  which `Reader` calls with `core::hdf5_mutex()` held.

## Verification

- Run (after `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh`):
  `cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
