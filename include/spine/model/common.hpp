// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#pragma once

#include <array>
#include <cstddef>
#include <limits>

namespace spine::detail {

// Default values of floating-point members that SPINE initialises to NaN.
inline constexpr float kNaNf = std::numeric_limits<float>::quiet_NaN();
inline constexpr double kNaNd = std::numeric_limits<double>::quiet_NaN();

/// A std::array with every element set to `value`.
template <class T, std::size_t N>
constexpr std::array<T, N> filled(T value) {
  std::array<T, N> out{};
  for (T& element : out) element = value;
  return out;
}

}  // namespace spine::detail
