#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include <spine/descriptor.hpp>

namespace spine::detail {

/// What an Event shares: the objects of every product read for that event.
struct EventData {
  struct Product {
    const ClassDescriptor* cls;
    std::shared_ptr<const void> objects;  // std::vector<generated class>
  };

  std::uint64_t index = 0;
  std::size_t file = 0;
  std::uint64_t entry = 0;
  std::map<std::string, Product, std::less<>> products;
};

}  // namespace spine::detail
