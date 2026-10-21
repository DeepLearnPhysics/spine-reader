#include <spine/descriptor.hpp>

namespace spine {

std::string_view to_string(ScalarType t) {
  switch (t) {
    case ScalarType::Bool:
      return "bool";
    case ScalarType::Int8:
      return "int8";
    case ScalarType::Int16:
      return "int16";
    case ScalarType::Int32:
      return "int32";
    case ScalarType::Int64:
      return "int64";
    case ScalarType::UInt8:
      return "uint8";
    case ScalarType::UInt16:
      return "uint16";
    case ScalarType::UInt32:
      return "uint32";
    case ScalarType::UInt64:
      return "uint64";
    case ScalarType::Float32:
      return "float32";
    case ScalarType::Float64:
      return "float64";
    case ScalarType::String:
      return "string";
  }
  return "?";
}

std::string_view EnumDescriptor::name_of(std::int64_t value) const {
  for (std::size_t i = 0; i < size; ++i) {
    if (members[i].value == value) return members[i].name;
  }
  return {};
}

const FieldDescriptor* ClassDescriptor::find(std::string_view field_name) const {
  for (std::size_t i = 0; i < num_fields; ++i) {
    if (fields[i].name == field_name) return &fields[i];
  }
  return nullptr;
}

}  // namespace spine
