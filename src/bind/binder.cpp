#include "bind/binder.hpp"

#include <cctype>
#include <string_view>

namespace spine::bind {
namespace {

std::string shape_of(ScalarType type, std::size_t count, bool variable) {
  std::string shape(to_string(type));
  if (type == ScalarType::String) return shape;
  if (variable) return shape + "[]";
  if (count != 1) return shape + "[" + std::to_string(count) + "]";
  return shape;
}

bool equal_ignoring_case(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

const EnumMember* find_label(const EnumDescriptor& enumeration, std::string_view label) {
  for (std::size_t i = 0; i < enumeration.size; ++i) {
    if (equal_ignoring_case(enumeration.members[i].name, label)) return &enumeration.members[i];
  }
  return nullptr;
}

const core::FileEnumMember* find_label(const core::ColumnInfo& column, std::string_view label) {
  for (const core::FileEnumMember& member : column.enum_members) {
    if (equal_ignoring_case(member.name, label)) return &member;
  }
  return nullptr;
}

// Enums are read by value, as SPINE reads its own files: labels are not stable enough to go by
// (SPINE v0.9-v0.10 label 6 "Unknown", which is LARCV_UNKNOWN in the model, not UNKNOWN = -1).
// But when two labels that file and model both have cross, e.g. the file has Track=0 and
// Shower=1 where the model has TRACK=1 and SHOWER=0, the numbering itself differs and the values
// cannot be trusted: the field is then not read.
std::string crossed_numbering(const FieldDescriptor& field, const core::ColumnInfo& column) {
  for (const core::FileEnumMember& member : column.enum_members) {
    const EnumMember* same_label = find_label(*field.enumeration, member.name);
    if (same_label == nullptr || same_label->value == member.value) continue;
    const std::string_view model_name = field.enumeration->name_of(member.value);
    if (model_name.empty()) continue;
    const core::FileEnumMember* other = find_label(column, model_name);
    if (other == nullptr) continue;
    return "enum numbering differs: file " + member.name + "=" + std::to_string(member.value) +
           " and " + other->name + "=" + std::to_string(other->value) + ", model " +
           std::string(same_label->name) + "=" + std::to_string(same_label->value) + " and " +
           std::string(model_name) + "=" + std::to_string(member.value);
  }
  return {};
}

void note_enum_differences(const FieldDescriptor& field, const core::ColumnInfo& column,
                           std::vector<std::string>& notes) {
  for (const core::FileEnumMember& member : column.enum_members) {
    const std::string_view model_name = field.enumeration->name_of(member.value);
    const std::string value = std::to_string(member.value);
    if (model_name.empty()) {
      notes.push_back(std::string(field.name) + ": file label " + member.name + "=" + value +
                      " is unknown to the model");
    } else if (!equal_ignoring_case(model_name, member.name)) {
      notes.push_back(std::string(field.name) + ": value " + value + " is " + member.name +
                      " in the file but " + std::string(model_name) + " in the model");
    }
  }
}

}  // namespace

std::string incompatibility(const FieldDescriptor& field, const core::ColumnInfo& column) {
  const bool field_string = field.type == ScalarType::String;
  const bool column_string = column.type == ScalarType::String;
  const bool same_shape = field_string || column_string
                              ? field_string == column_string
                              : field.variable == column.variable &&
                                    (field.variable || field.count == column.count);
  if (same_shape) return {};
  return "file " + shape_of(column.type, column.count, column.variable) + " vs model " +
         shape_of(field.type, field.count, field.variable);
}

ProductBinding::ProductBinding(const ClassDescriptor& cls, core::ProductSource& source)
    : cls_(&cls), source_(&source), present_(cls.num_fields, false) {
  const core::ProductInfo& info = source.info();
  report_.product = info.name;
  report_.class_name = info.class_name;

  for (std::size_t i = 0; i < cls.num_fields; ++i) {
    const FieldDescriptor& field = cls.fields[i];
    const core::ColumnInfo* column = info.find(field.name);
    if (column == nullptr) {
      report_.missing.emplace_back(field.name);
      continue;
    }
    std::string why = incompatibility(field, *column);
    if (why.empty() && field.enumeration != nullptr) why = crossed_numbering(field, *column);
    if (!why.empty()) {
      report_.mismatched.push_back(std::string(field.name) + ": " + why);
      continue;
    }
    // A bool stored as a plain integer (SPINE before v0.11.0) is read as that integer and
    // turned into true/false by assign, so any non-zero value counts as true.
    const bool integer_bool = field.type == ScalarType::Bool && column->type != ScalarType::Bool;
    requests_.push_back({column, integer_bool ? column->type : field.type});
    fields_.push_back(i);
    present_[i] = true;
    if (field.enumeration != nullptr) note_enum_differences(field, *column, report_.enum_notes);
  }

  for (const core::ColumnInfo& column : info.columns) {
    if (cls.find(column.name) == nullptr) report_.extra.push_back(column.name);
  }
}

}  // namespace spine::bind
