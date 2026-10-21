#pragma once

#include <bitset>
#include <cstddef>

namespace spine {

namespace detail {
struct ObjectAccess;
}

/// Base of every generated SPINE class. Records which fields the source file provided.
/// A field the file lacks keeps its SPINE default value and reports has() == false.
template <std::size_t N>
class Object {
 public:
  static constexpr std::size_t kNumFields = N;

 protected:
  bool has_field(std::size_t index) const { return present_.test(index); }

 private:
  std::bitset<N> present_;
  friend struct detail::ObjectAccess;
};

namespace detail {

struct ObjectAccess {
  template <std::size_t N>
  static void set_present(Object<N>& object, const std::bitset<N>& mask) {
    object.present_ = mask;
  }
};

}  // namespace detail
}  // namespace spine
