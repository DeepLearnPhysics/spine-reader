#include "core/columns.hpp"

#include <vector>

#include <spine/error.hpp>

#include "h5/error.hpp"
#include "h5/util.hpp"

namespace spine::core {
namespace {

// Enum conversion, measured on HDF5 1.12.2 and 1.14.6 (the tests keep it honest): an HDF5
// enum converts to an integer or floating-point memory type, also as a member of a compound
// and also when the file's base type is big-endian, exactly as its base integer would. The
// value need not be one of the labels; -1 stays -1 and 99 stays 99 in an enum that does not
// list it. Narrowing follows the integer rules (saturation: -1 read as unsigned gives 0).
// So enums are read like integers, and the labels of the file are only reported. Do not use
// the other route, a memory type that is a native copy of the file's enum: when the file's
// base type is not little-endian, HDF5 then maps by label and turns every value without one
// into -1.

std::optional<ScalarType> integer_type(hid_t type) {
  const bool is_signed = H5Tget_sign(type) == H5T_SGN_2;
  switch (H5Tget_size(type)) {
    case 1: return is_signed ? ScalarType::Int8 : ScalarType::UInt8;
    case 2: return is_signed ? ScalarType::Int16 : ScalarType::UInt16;
    case 4: return is_signed ? ScalarType::Int32 : ScalarType::UInt32;
    case 8: return is_signed ? ScalarType::Int64 : ScalarType::UInt64;
    default: return std::nullopt;
  }
}

/// The labels of an enum whose base type is `base_type`, with values taken from a native-order
/// copy of the type.
std::vector<FileEnumMember> enum_members(hid_t enum_type, ScalarType base_type) {
  h5::Datatype native(
      h5::check(H5Tget_native_type(enum_type, H5T_DIR_ASCEND), "H5Tget_native_type"));
  std::vector<FileEnumMember> members;
  const int count = h5::check(H5Tget_nmembers(native.get()), "H5Tget_nmembers");
  for (int i = 0; i < count; ++i) {
    const auto index = static_cast<unsigned>(i);
    alignas(8) unsigned char raw[8] = {};
    h5::check(H5Tget_member_value(native.get(), index, raw), "H5Tget_member_value");
    std::int64_t value = 0;
    switch (base_type) {
      case ScalarType::Int8: value = *reinterpret_cast<std::int8_t*>(raw); break;
      case ScalarType::UInt8: value = *reinterpret_cast<std::uint8_t*>(raw); break;
      case ScalarType::Int16: value = *reinterpret_cast<std::int16_t*>(raw); break;
      case ScalarType::UInt16: value = *reinterpret_cast<std::uint16_t*>(raw); break;
      case ScalarType::Int32: value = *reinterpret_cast<std::int32_t*>(raw); break;
      case ScalarType::UInt32: value = *reinterpret_cast<std::uint32_t*>(raw); break;
      default: value = *reinterpret_cast<std::int64_t*>(raw); break;  // 64-bit
    }
    members.push_back({h5::member_name(native.get(), index), value});
  }
  return members;
}

bool is_bool_enum(const std::vector<FileEnumMember>& members) {
  if (members.size() != 2) return false;
  for (const FileEnumMember& member : members) {
    if (!((member.name == "FALSE" && member.value == 0) ||
          (member.name == "TRUE" && member.value == 1))) {
      return false;
    }
  }
  return members[0].name != members[1].name;
}

/// Scalar type of an integer, float or enum type; fills `members` for enums.
std::optional<ScalarType> scalar_type(hid_t type, std::vector<FileEnumMember>& members) {
  switch (H5Tget_class(type)) {
    case H5T_INTEGER:
      return integer_type(type);
    case H5T_FLOAT:
      switch (H5Tget_size(type)) {
        case 4: return ScalarType::Float32;
        case 8: return ScalarType::Float64;
        default: return std::nullopt;
      }
    case H5T_ENUM: {
      h5::Datatype base(h5::check(H5Tget_super(type), "H5Tget_super"));
      const std::optional<ScalarType> base_type = integer_type(base.get());
      if (!base_type) return std::nullopt;
      members = enum_members(type, *base_type);
      return is_bool_enum(members) ? ScalarType::Bool : *base_type;
    }
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<ColumnInfo> describe_type(const std::string& name, hid_t file_type) {
  ColumnInfo column;
  column.name = name;
  switch (H5Tget_class(file_type)) {
    case H5T_STRING:
      if (h5::check(H5Tis_variable_str(file_type), "H5Tis_variable_str", name) == 0) {
        return std::nullopt;
      }
      column.type = ScalarType::String;
      column.variable = true;
      return column;
    case H5T_ARRAY:
    case H5T_VLEN: {
      const bool is_array = H5Tget_class(file_type) == H5T_ARRAY;
      h5::Datatype element(h5::check(H5Tget_super(file_type), "H5Tget_super", name));
      const std::optional<ScalarType> type = scalar_type(element.get(), column.enum_members);
      if (!type) return std::nullopt;
      column.type = *type;
      if (is_array) {
        const int rank = h5::check(H5Tget_array_ndims(file_type), "H5Tget_array_ndims", name);
        std::vector<hsize_t> dims(static_cast<std::size_t>(rank));
        h5::check(H5Tget_array_dims2(file_type, dims.data()), "H5Tget_array_dims2", name);
        column.count = 1;
        for (hsize_t dim : dims) column.count *= static_cast<std::size_t>(dim);
      } else {
        column.variable = true;
        column.count = 0;
      }
      return column;
    }
    default: {
      const std::optional<ScalarType> type = scalar_type(file_type, column.enum_members);
      if (!type) return std::nullopt;
      column.type = *type;
      return column;
    }
  }
}

hid_t native_type(ScalarType type) {
  switch (type) {
    case ScalarType::Bool:
    case ScalarType::UInt8: return H5T_NATIVE_UINT8;
    case ScalarType::Int8: return H5T_NATIVE_INT8;
    case ScalarType::Int16: return H5T_NATIVE_INT16;
    case ScalarType::UInt16: return H5T_NATIVE_UINT16;
    case ScalarType::Int32: return H5T_NATIVE_INT32;
    case ScalarType::UInt32: return H5T_NATIVE_UINT32;
    case ScalarType::Int64: return H5T_NATIVE_INT64;
    case ScalarType::UInt64: return H5T_NATIVE_UINT64;
    case ScalarType::Float32: return H5T_NATIVE_FLOAT;
    case ScalarType::Float64: return H5T_NATIVE_DOUBLE;
    case ScalarType::String: break;
  }
  throw Error("strings have no native HDF5 type");
}

void check_request(const ColumnInfo& column, ScalarType requested) {
  const bool column_string = column.type == ScalarType::String;
  const bool request_string = requested == ScalarType::String;
  if (column_string != request_string) {
    throw Error("column '" + column.name + "' is " + (column_string ? "a string" : "numeric") +
                " and cannot be read as " + std::string(to_string(requested)));
  }
  if (requested == ScalarType::Bool && column.type != ScalarType::Bool) {
    throw Error("column '" + column.name + "' is not a bool; read it as " +
                std::string(to_string(column.type)));
  }
}

h5::Datatype memory_type(const ColumnInfo& column, ScalarType requested, hid_t file_type) {
  if (column.type == ScalarType::String) return h5::copy_type(file_type);
  const hid_t element = native_type(requested);
  if (column.variable) {
    return h5::Datatype(h5::check(H5Tvlen_create(element), "H5Tvlen_create", column.name));
  }
  if (H5Tget_class(file_type) == H5T_ARRAY) {  // same dimensions as in the file
    const int rank = h5::check(H5Tget_array_ndims(file_type), "H5Tget_array_ndims", column.name);
    std::vector<hsize_t> dims(static_cast<std::size_t>(rank));
    h5::check(H5Tget_array_dims2(file_type, dims.data()), "H5Tget_array_dims2", column.name);
    const auto unsigned_rank = static_cast<unsigned>(rank);
    return h5::Datatype(h5::check(H5Tarray_create2(element, unsigned_rank, dims.data()),
                                  "H5Tarray_create2", column.name));
  }
  return h5::copy_type(element);
}

}  // namespace spine::core
