#pragma once

// Matches a generated model class to the columns of one product, by name, and fills
// model objects from the columns the core reads.

#include <bitset>
#include <cstddef>
#include <string>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/detail/column_buffer.hpp>
#include <spine/object.hpp>
#include <spine/schema_report.hpp>

#include "core/source.hpp"

namespace spine::bind {

/// Why a file column cannot fill a model field, or "" when it can.
std::string incompatibility(const FieldDescriptor& field, const core::ColumnInfo& column);

/// Binding of one model class to one product of one open file. Build it once per file and
/// product; reading then costs one core read per call.
class ProductBinding {
 public:
  ProductBinding(const ClassDescriptor& cls, core::ProductSource& source);

  const ClassDescriptor& descriptor() const { return *cls_; }
  core::ProductSource& source() const { return *source_; }
  const ProductReport& report() const { return report_; }
  bool present(std::size_t field) const { return present_[field]; }

  /// Model objects for `rows`, one per row. Fields the file lacks keep their defaults.
  template <class T>
  void read(core::RowRange rows, std::vector<T>& out) const;

 private:
  const ClassDescriptor* cls_;
  core::ProductSource* source_;
  std::vector<core::ColumnRequest> requests_;
  std::vector<std::size_t> fields_;  // model field index answered by each request
  std::vector<bool> present_;        // per model field
  ProductReport report_;
  mutable std::vector<detail::ColumnBuffer> buffers_;
};

template <class T>
void ProductBinding::read(core::RowRange rows, std::vector<T>& out) const {
  source_->read(rows, requests_, buffers_);
  std::bitset<T::kNumFields> mask;
  for (std::size_t i = 0; i < present_.size() && i < T::kNumFields; ++i) mask[i] = present_[i];
  out.assign(static_cast<std::size_t>(rows.size()), T{});
  for (std::size_t k = 0; k < requests_.size(); ++k) {
    const FieldDescriptor& field = cls_->fields[fields_[k]];
    for (std::size_t r = 0; r < out.size(); ++r) field.assign(&out[r], buffers_[k], r);
  }
  for (T& object : out) detail::ObjectAccess::set_present(object, mask);
}

}  // namespace spine::bind
