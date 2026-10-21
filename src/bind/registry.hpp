#pragma once

#include <memory>
#include <string_view>

#include <spine/descriptor.hpp>

#include "bind/binder.hpp"
#include "core/source.hpp"

namespace spine::bind {

/// A generated class the Reader can fill: its descriptor, and a reader that returns a
/// std::vector of that class, type-erased.
struct ClassEntry {
  const ClassDescriptor& (*descriptor)();
  std::shared_ptr<const void> (*read)(const ProductBinding& binding, core::RowRange rows);
};

/// The generated class for a SPINE class name, or nullptr.
const ClassEntry* find_class(std::string_view class_name);

}  // namespace spine::bind
