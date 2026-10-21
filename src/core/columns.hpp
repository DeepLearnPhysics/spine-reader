#pragma once

// Maps HDF5 types onto ColumnInfo, and ColumnInfo plus a requested ScalarType onto HDF5
// memory types. Shared by the V1 and V2 readers.

#include <optional>
#include <string>

#include <hdf5.h>

#include "core/source.hpp"
#include "h5/handle.hpp"

namespace spine::core {

/// The element type of `file_type` as a column called `name`: integers, floats, enums,
/// fixed-size arrays of those (H5T_ARRAY), VLEN sequences of those and variable-length
/// strings. Returns nothing for any other HDF5 type (nested compounds, references, opaque,
/// fixed-length strings, ...), which spine-reader cannot read.
std::optional<ColumnInfo> describe_type(const std::string& name, hid_t file_type);

/// The predefined HDF5 native type for `type`; Bool maps to uint8. Not owned: never close it.
/// String has no native HDF5 type and throws.
hid_t native_type(ScalarType type);

/// Throws spine::Error unless `column` can be read as `requested`: numbers convert to any
/// number, enums to integers, Bool only to Bool, strings only to String.
void check_request(const ColumnInfo& column, ScalarType requested);

/// HDF5 memory type for one element of `column` read as `requested`, as a compound member.
/// `file_type` is the column's type in the file. The result is the converted scalar, an
/// H5T_ARRAY of it with the file's dimensions for fixed arrays, an hvl_t for VLEN columns, and
/// a copy of `file_type` itself for strings.
h5::Datatype memory_type(const ColumnInfo& column, ScalarType requested, hid_t file_type);

}  // namespace spine::core
