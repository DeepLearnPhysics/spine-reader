#pragma once

#include <string>
#include <vector>

namespace spine {

/// How one product of one file lined up with the generated model class.
struct ProductReport {
  std::string product;     // product key, e.g. "reco_particles"
  std::string class_name;  // SPINE class recorded in the file
  /// Model fields the file does not have. They keep SPINE's default and has() is false.
  std::vector<std::string> missing;
  /// Fields the file has but in a shape the model cannot take ("name: reason"). Treated as
  /// missing.
  std::vector<std::string> mismatched;
  /// File columns the model does not know. They are not read.
  std::vector<std::string> extra;
  /// Enum labels in the file that differ from the model's. Values are still mapped by number.
  std::vector<std::string> enum_notes;

  /// True when every model field was read as-is. Extra file columns do not count.
  bool complete() const { return missing.empty() && mismatched.empty(); }
};

/// What spine-reader found when it opened a file: one entry per product it binds, and the
/// products it leaves alone.
struct SchemaReport {
  std::string path;
  std::string spine_version;  // writer release, from /info
  int format_version = 1;
  std::string layout;         // "V1", "V2Root" or "V2Products"
  std::string model_version;  // SPINE release the C++ model was generated from
  std::vector<ProductReport> products;
  std::vector<std::string> skipped;  // "points: array product", "foo: no model class Bar", ...

  /// Multi-line, human-readable summary for logs.
  std::string summary() const;
};

}  // namespace spine
