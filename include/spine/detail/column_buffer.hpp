#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <spine/descriptor.hpp>

namespace spine::detail {

/// One column read over a range of rows, already converted to `type`.
///
/// Fixed columns hold rows * count elements in `data`, row-major.
/// Variable columns hold all elements of all rows in `data`, and row r spans elements
/// [offsets[r], offsets[r + 1]); offsets has rows + 1 entries and offsets[0] == 0.
/// String columns (fixed or variable in the file) hold one std::string per row in `strings`
/// and leave `data` and `offsets` empty.
struct ColumnBuffer {
  ScalarType type = ScalarType::Int64;
  bool variable = false;
  std::size_t count = 1;  // elements per row, fixed columns only
  std::size_t rows = 0;
  std::vector<unsigned char> data;
  std::vector<std::uint64_t> offsets;
  std::vector<std::string> strings;

  template <class T>
  const T* elements() const {
    return reinterpret_cast<const T*>(data.data());
  }
};

}  // namespace spine::detail
