# h5

## Overview

Private, thin RAII layer over the HDF5 C API (the HDF5 C++ API is deliberately not used).
It owns identifiers, turns failed calls into `spine::Error`, silences HDF5's own error
printing for the duration of a call, and offers helpers for opening objects, listing
children and reading attributes. The rest of `src/` uses this layer for handles and errors.
Calls that open, read or create objects go through `h5::check`. Simple type queries
(`H5Tget_class`, `H5Tget_size`, `H5Tget_sign`, `H5Iget_type`) are called directly and not
checked.

## Interface

- `handle.hpp`
  - `template <class Closer> class spine::h5::Handle`: move-only owner of one `hid_t`.
    Provides `get()`, `explicit operator bool()` (`id >= 0`) and `reset()`.
  - Aliases: `File`, `Group`, `Dataset`, `Object`, `Datatype`, `Dataspace`, `Attribute`.
    Their closers are `FileClose`, `GroupClose`, `DatasetClose`, `ObjectClose`,
    `DatatypeClose`, `DataspaceClose` and `AttributeClose` (`H5Fclose`, `H5Gclose`, ...).
- `error.hpp` / `error.cpp`
  - `class spine::h5::ErrorSilencer`: turns HDF5 auto error printing off
    (`H5Eset_auto2(..., nullptr, nullptr)`) for its lifetime and restores the previous
    handler in the destructor.
  - `[[noreturn]] void fail(std::string_view call, std::string_view object = {})`: throws
    `spine::Error("<call>[ on '<object>'] failed: <innermost HDF5 message>")`, or
    `"<call>[ on '<object>'] failed (no message on the HDF5 error stack)"` when the stack is
    empty. The innermost message comes from `H5Ewalk2(H5E_WALK_DOWNWARD)`.
  - `template <class T> T check(T result, std::string_view call, std::string_view object = {})`:
    calls `fail` when `result < 0`.
- `util.hpp` / `util.cpp`
  - Open: `open_file` (read-only, `H5F_ACC_RDONLY`), `open_object`, `open_group`, `open_dataset`.
  - Query: `object_kind`, `has_link`, `dataset_type`, `dataset_space`, `copy_type`,
    `member_type`, `member_name`, `extent`, `has_attribute`.
  - `child_names(hid_t group, bool* by_creation_order = nullptr)`: lists links in creation
    order if the group tracks it, else in name order (whether it tracks it is taken from the
    group creation property list).
  - `read_int_attribute`: reads an integer or enum attribute as `int64`. `read_string_attribute`:
    reads a variable-length (UTF-8 or ASCII) or fixed-length string attribute. Fixed-length
    strings are trimmed: trailing spaces for SPACEPAD, otherwise cut at the first NUL.
  - `class Reclaim`: RAII `H5Treclaim` of the VLEN and string memory that `H5Dread` or
    `H5Aread` allocated in a buffer.

## Invariants

- Every function here assumes the caller holds `core::hdf5_mutex()`. Closing a handle is an
  HDF5 call too, so handles must also be destroyed under the lock.
- A default-constructed or moved-from `Handle` holds `H5I_INVALID_HID` and closes nothing.
  Close failures in `Handle::reset()` are ignored, because they cannot be reported from a
  destructor.
- `check` treats any negative result as failure (`hid_t`, `herr_t`, `htri_t`, `hssize_t`).
- Callbacks passed to the C library (`visit`, `collect_name`) catch all exceptions and
  return `-1`. No exception crosses the C library.
- Attribute readers require exactly one element. They throw on a wrong type class.
- String attributes are read with a copy of the file's own type as the memory type. Do not
  switch to `H5T_C_S1`: HDF5 has no conversion from SPINE's UTF-8 strings to it.
- `Reclaim` must be constructed before the read, on a zeroed buffer, and destroyed before
  the type and space it references.
- `ErrorSilencer` never changes the error-printing setting permanently (the host application
  may use HDF5 too).

## Side Effects

- Opens HDF5 files read-only. Nothing here writes to disk.
- Changes HDF5's library-global automatic error-printing setting temporarily (`ErrorSilencer`).
- Frees HDF5-allocated memory (`H5free_memory` in `member_name`, `H5Treclaim` in `Reclaim`).
- Nothing prints. Every failure becomes `spine::Error`.

## Verification

- Run:
  `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh && cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
- Main unit tests: `tests/test_core_h5.cpp`, `tests/test_core_leaks.cpp`.
