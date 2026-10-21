// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/model/common.hpp>
#include <spine/object.hpp>

namespace spine {

/// 3D rasterized image metadata. SPINE `spine.data.larcv.meta.ImageMeta3D`: 4 fields, in the order
/// SPINE's HDF5 writer stores them. A field the source file lacks keeps its default and has() is
/// false.
struct ImageMeta3D : Object<4> {
  enum class Field : std::uint16_t {
    lower,
    upper,
    size,
    count,
  };

  /// (3) Array of image lower bounds in detector coordinates (cm) [units: cm]
  std::array<float, 3> lower = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Array of image upper bounds in detector coordinates (cm) [units: cm]
  std::array<float, 3> upper = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Array of pixel size in each dimension (cm) [units: cm]
  std::array<float, 3> size = {detail::kNaNf, detail::kNaNf, detail::kNaNf};
  /// (3) Array of pixel count in each dimension
  std::array<std::int64_t, 3> count = {-1, -1, -1};

  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }
  static const ClassDescriptor& descriptor();
};

}  // namespace spine
