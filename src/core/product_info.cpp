#include "core/source.hpp"

namespace spine::core {

std::string_view to_string(Layout layout) {
  switch (layout) {
    case Layout::V1:
      return "V1";
    case Layout::V2Root:
      return "V2Root";
    case Layout::V2Products:
      return "V2Products";
  }
  return "?";
}

std::string_view to_string(ProductKind kind) {
  switch (kind) {
    case ProductKind::Objects:
      return "objects";
    case ProductKind::Array:
      return "array";
    case ProductKind::String:
      return "string";
    case ProductKind::List:
      return "list";
    case ProductKind::MultiList:
      return "multi_list";
  }
  return "?";
}

const ColumnInfo* ProductInfo::find(std::string_view column) const {
  for (const ColumnInfo& info : columns) {
    if (info.name == column) return &info;
  }
  return nullptr;
}

}  // namespace spine::core
